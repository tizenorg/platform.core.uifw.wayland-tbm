/*
Copyright (C) 2015 Samsung Electronics co., Ltd. All Rights Reserved.

Contact: 
      SooChan Lim <sc1.lim@samsung.com>, 
      Sangjin Lee <lsj119@samsung.com>,
      Boram Park <boram1288.park@samsung.com>, 
      Changyeon Lee <cyeon.lee@samsung.com>

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice (including the next
paragraph) shall be included in all copies or substantial portions of the
Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <unistd.h>
#include <fcntl.h>

#include <tbm_bufmgr.h>
#include <tbm_surface.h>
#include <tbm_surface_internal.h>

#include "wayland-tbm-client.h"
#include "wayland-tbm-client-protocol.h"

struct wayland_tbm
{
   struct wl_display* dpy;
   tbm_bufmgr bufmgr;
   uint32_t capabilities;
   char *device;

   //Temp data storage for authentication_info
   uint32_t info_capabilities;
   char *info_device;
   int32_t info_fd;   
};

struct wl_tbm_info 
{
   struct wl_display* dpy;
   struct wl_event_queue *wl_queue;
   struct wl_tbm* wl_tbm;
};


int
_wayland_tbm_client_get_authentication_info(struct wl_tbm* wl_tbm, struct wl_event_queue *wl_queue,
                                 int32_t *fd, uint32_t* capabilities, char**device_name)
{
   struct wayland_tbm *tbm;

   if (!wl_tbm)
      return 0;

   tbm = wl_tbm_get_user_data(wl_tbm);
   wl_tbm_get_authentication_info(wl_tbm);
   if (wl_queue)
      wl_display_roundtrip_queue(tbm->dpy, wl_queue);
   else
      wl_display_roundtrip(tbm->dpy);

   if (!fd)
      close(tbm->info_fd);
   else
      *fd = tbm->info_fd;
   tbm->info_fd = 0;

   if (capabilities)
      *capabilities = tbm->info_capabilities;
   tbm->info_capabilities = 0;
   
   if (device_name)
      *device_name = tbm->info_device;
   tbm->info_device = NULL;

   return 1;
}

static void 
handle_tbm_authentication_info(void *data,
				    struct wl_tbm *wl_tbm,
				    const char *device_name,
				    uint32_t capabilities,
				    int32_t auth_fd)
{
   struct wayland_tbm *tbm = (struct wayland_tbm *)data;

   tbm->info_fd = auth_fd;
   tbm->info_capabilities = capabilities;
   if (device_name)
      tbm->info_device = strndup(device_name, 256);
}

static const struct wl_tbm_listener wl_tbm_client_listener = {
   handle_tbm_authentication_info
};

static void 
handle_registry_global(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version)
{
   struct wl_tbm_info *info = (struct wl_tbm_info *)data;
   struct wayland_tbm *tbm;

   if (!strcmp(interface, "wl_tbm")) {
      int32_t fd;
      uint32_t capabilities;
      char* device_name;

      info->wl_tbm = wl_registry_bind(registry, name, &wl_tbm_interface, version);
      if (!info->wl_tbm)
      {
         printf("Failed to bind wl_tbm\n");
         return;
      }

      tbm = calloc(sizeof(struct wayland_tbm), 1);
      if (!tbm)
      {
         printf("Failed to alloc tbm\n");
         wl_tbm_destroy(info->wl_tbm);
         info->wl_tbm = NULL;
         return;
      }
      tbm->dpy = info->dpy;
      
      wl_tbm_add_listener(info->wl_tbm, &wl_tbm_client_listener, tbm);
      wl_proxy_set_queue((struct wl_proxy *)info->wl_tbm, info->wl_queue);
      if (!_wayland_tbm_client_get_authentication_info(info->wl_tbm,
                                                info->wl_queue,
                                                &fd,
                                                &capabilities,
                                                &device_name))
      {
         printf("failed to get auth_info\n");
         wayland_tbm_client_uninit(info->wl_tbm);
         info->wl_tbm = NULL;
         return;
      }

      tbm->device = device_name;
      tbm->capabilities = capabilities;
      tbm->bufmgr = tbm_bufmgr_init(fd);
      close(fd);
      if (!tbm->bufmgr)
      {
         printf("failed to get auth_info\n");
         wayland_tbm_client_uninit(info->wl_tbm);
         info->wl_tbm = NULL;
         return;
      }
   }
}


static const struct wl_registry_listener registry_listener = {
                           handle_registry_global,
                           NULL
                           };

struct wl_tbm* 
wayland_tbm_client_init(struct wl_display* dpy)
{
   struct wl_registry *wl_registry;
   struct wl_tbm_info info = {
                           .dpy = NULL,
                           .wl_queue = NULL,
                           .wl_tbm = NULL,
                           };

   if (!dpy) {
      printf("Failed to tbm init, dpy=NULL\n");
      return NULL;
   }

   info.dpy = dpy;

   info.wl_queue = wl_display_create_queue(dpy);
   if (!info.wl_queue) {
      printf("Failed to create a WL Queue\n");
      return NULL;
   }
   
   wl_registry = wl_display_get_registry(dpy);
   if (!wl_registry) {
      printf("Failed to get registry\n");
      wl_event_queue_destroy(info.wl_queue);
      return NULL;
   }

   wl_proxy_set_queue((struct wl_proxy *)wl_registry, info.wl_queue);
   wl_registry_add_listener(wl_registry, &registry_listener, &info); 

   wl_display_roundtrip_queue(dpy, info.wl_queue);

   wl_event_queue_destroy(info.wl_queue);
   wl_registry_destroy(wl_registry);

   /* wl_tbm's queue is destroyed above. We should make wl_tbm's queue to
      *  display's default_queue.
      */
   if (info.wl_tbm)
      wl_proxy_set_queue((struct wl_proxy *)info.wl_tbm, NULL);
   else
      printf("failed to create wl_tbm\n");

   return info.wl_tbm;
}

void
wayland_tbm_client_uninit(struct wl_tbm* wl_tbm)
{
   struct wayland_tbm *tbm;

   if (!wl_tbm)
      return;

   tbm = wl_tbm_get_user_data(wl_tbm);
   if (tbm == NULL) {
      printf("Failed to uninit\n");
   }

   if (tbm->device)
      free(tbm->device);

   if (tbm->bufmgr)
      tbm_bufmgr_deinit(tbm->bufmgr);

   if (tbm->info_device)
      free(tbm->info_device);

   if (tbm->info_fd)
      close(tbm->info_fd);
      
   free(tbm);
   wl_tbm_set_user_data(wl_tbm, NULL);
   wl_tbm_destroy(wl_tbm);
}

const char*
wayland_tbm_client_get_device_name(struct wl_tbm* wl_tbm)
{
   struct wayland_tbm *tbm;

   if (!wl_tbm)
      return NULL;
   
   tbm = wl_tbm_get_user_data(wl_tbm);
   if (!tbm->bufmgr)
      return NULL;
      
   return (const char*)tbm->device;
}

uint32_t
wayland_tbm_client_get_capability(struct wl_tbm* wl_tbm)
{
   struct wayland_tbm *tbm;

   if (!wl_tbm)
      return 0;

   tbm = wl_tbm_get_user_data(wl_tbm);
   if (!tbm->bufmgr)
      return 0;
   
   return tbm->capabilities;
}

void*
wayland_tbm_client_get_bufmgr(struct wl_tbm* wl_tbm)
{
   struct wayland_tbm *tbm;

   if (!wl_tbm)
      return NULL;

   tbm = wl_tbm_get_user_data(wl_tbm);
   if (!tbm->bufmgr)
      return NULL;
   
   return (void*)tbm->bufmgr;
}

struct wl_buffer*
wayland_tbm_client_create_buffer(struct wl_tbm* wl_tbm, tbm_surface_h surface)
{
   int ret;
   tbm_surface_info_s info;
   int num_buf;
   tbm_bo bos[TBM_SURF_PLANE_MAX] = {NULL,};
   int bufs[TBM_SURF_PLANE_MAX] = {0,};
   int is_fd=-1;
   struct wl_buffer* wl_buffer = NULL;
   int i;

   if (!wl_tbm) {
      printf("invalid wl_tbm\n");
      return NULL;
   }

   if (!surface) {
      printf("invalid surface\n");
      return NULL;
   }
   
   ret = tbm_surface_get_info(surface, &info);
   if (ret != TBM_SURFACE_ERROR_NONE) {
      printf("Failed to create buffer from surface\n");
      return NULL;
   }

   if (info.num_planes > 3)
   {
      printf("invalid num_planes(%d)\n", info.num_planes);
      return NULL;
   }

   num_buf = tbm_surface_internal_get_num_bos(surface);
   for (i=0; i<num_buf; i++)
   {
      bos[i] = tbm_surface_internal_get_bo(surface, i);
      if (bos[i] == NULL) {
         printf("Failed to get bo from surface\n");
         goto err;
      }

      //check support export fd
      if (is_fd == -1)
      {
         bufs[i] = tbm_bo_export_fd(bos[i]);
         if (bufs[i])
            is_fd = 1;
         else
         {
            is_fd = 0;
            bufs[i] = tbm_bo_export(bos[i]);
         }
      }
      else if(is_fd == 1)
      {
         bufs[i] = tbm_bo_export_fd(bos[i]);
      }
      else
         bufs[i] = tbm_bo_export(bos[i]);

      if (!bufs[i])
      {
         printf("Failed to export(is_fd:%d)\n", is_fd);
         goto err;
      }
   }

   if (num_buf == 0 || is_fd == -1)
   {
      printf("Failed to export(is_fd:%d)\n", is_fd);
      goto err;
   }
   
   if (is_fd == 1)
      wl_buffer = wl_tbm_create_buffer_with_fd(wl_tbm,
                                 info.width, info.height, info.format, info.num_planes,
                                 tbm_surface_internal_get_plane_bo_idx(surface, 0),
                                 info.planes[0].offset, info.planes[0].stride,
                                 tbm_surface_internal_get_plane_bo_idx(surface, 1),
                                 info.planes[1].offset, info.planes[1].stride,
                                 tbm_surface_internal_get_plane_bo_idx(surface, 2),
                                 info.planes[2].offset, info.planes[2].stride,
                                 0,
                                 num_buf, bufs[0], bufs[1], bufs[2]);
   else
      wl_buffer = wl_tbm_create_buffer(wl_tbm,
                                 info.width, info.height, info.format, info.num_planes,
                                 tbm_surface_internal_get_plane_bo_idx(surface, 0),
                                 info.planes[0].offset, info.planes[0].stride,
                                 tbm_surface_internal_get_plane_bo_idx(surface, 1),
                                 info.planes[1].offset, info.planes[1].stride,
                                 tbm_surface_internal_get_plane_bo_idx(surface, 2),
                                 info.planes[2].offset, info.planes[2].stride,
                                 0,
                                 num_buf, bufs[0], bufs[1], bufs[2]);
   
   if (!wl_buffer) {
      printf("Failed to create wl_buffer\n");
      goto err;
   }
   wl_buffer_set_user_data(wl_buffer, surface);

   return wl_buffer;
   
err:
   for (i=0; i<TBM_SURF_PLANE_MAX; i++)
   {
      if (bos[i])
         tbm_bo_unref(bos[i]);
      if (is_fd == 1 && (bufs[i] > 0))
         close(bufs[i]);
   }

   return NULL;
}

void
wayland_tbm_client_destroy_buffer(struct wl_buffer* buffer)
{
   tbm_surface_h surface;

   if (!buffer) {
      printf("invalid wl_buffer\n");
      return;
   }

   surface = (tbm_surface_h)wl_buffer_get_user_data(buffer);
   wl_buffer_set_user_data(buffer, NULL);
   wl_buffer_destroy(buffer);
   if (surface)
       tbm_surface_destroy(surface);
}

tbm_surface_h
wayland_tbm_client_get_surface(struct wl_buffer* buffer)
{
   if (!buffer) {
      printf("invalid wl_buffer\n");
      return NULL;
   }

   return (tbm_surface_h)wl_buffer_get_user_data(buffer);
}

