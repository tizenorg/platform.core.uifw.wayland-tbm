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

#include <tbm_surface.h>
#include <tbm_surface_queue.h>
#include <tbm_surface_internal.h>

#include "wayland-tbm-client.h"
#include "wayland-tbm-client-protocol.h"
#include "wayland-tbm-int.h"


struct wayland_tbm_client {
	struct wl_display *dpy;
	struct wl_event_queue *wl_queue;
	struct wl_tbm *wl_tbm;

	tbm_bufmgr bufmgr;

	struct wl_list queue_info_list;
};

struct wayland_tbm_buffer {
	struct wl_buffer *wl_buffer;
	tbm_surface_h tbm_surface;
	uint32_t flags;
	int allocated;

	struct wl_tbm_queue *wl_tbm_queue;
	struct wl_list link;
};

struct wayland_tbm_surface_queue {
	struct wl_tbm_queue *wl_tbm_queue;
	tbm_bufmgr bufmgr;

	int width;
	int height;
	int format;
	int flag;
	uint num_bufs;
	struct wl_surface *wl_surface;

	int is_active;
	int usage;
	struct wl_list attach_bufs;

	tbm_surface_queue_h tbm_queue;
	struct wl_list link;
};

//#define DEBUG_TRACE
#ifdef DEBUG_TRACE
#define WL_TBM_TRACE(fmt, ...)   fprintf (stderr, "[WL_TBM_C(%d):%s] " fmt, getpid(), __func__, ##__VA_ARGS__)
#else
#define WL_TBM_TRACE(fmt, ...)
#endif

static void
handle_tbm_monitor_client_tbm_bo(void *data,
				 struct wl_tbm *wl_tbm,
				 int32_t command,
				 int32_t trace_command,
				 int32_t target,
				 int32_t pid)
{
	struct wayland_tbm_client *tbm_client = (struct wayland_tbm_client *)data;

	if (command == WL_TBM_MONITOR_COMMAND_SHOW) {
		if (target == WL_TBM_MONITOR_TARGET_CLIENT) {
			if (getpid() == pid)
				tbm_bufmgr_debug_show(tbm_client->bufmgr);
		} else if (target == WL_TBM_MONITOR_TARGET_ALL) {
			tbm_bufmgr_debug_show(tbm_client->bufmgr);
		} else {
			WL_TBM_LOG("[%s]: Error target is not available. target = %d\n", __func__,
				   target);
		}
	} else if (command == WL_TBM_MONITOR_COMMAND_TRACE) {
		WL_TBM_LOG("[%s]: TRACE is not implemented.\n", __func__);
	} else {
		WL_TBM_LOG("[%s]: Error command is not available. command = %d\n", __func__,
			   command);
	}

}

static const struct wl_tbm_listener wl_tbm_client_listener = {
	handle_tbm_monitor_client_tbm_bo
};

static void
_wayland_tbm_client_registry_handle_global(void *data,
		struct wl_registry *registry, uint32_t name, const char *interface,
		uint32_t version)
{
	struct wayland_tbm_client *tbm_client = (struct wayland_tbm_client *)data;

	if (!strcmp(interface, "wl_tbm")) {
		tbm_client->wl_tbm = wl_registry_bind(registry, name, &wl_tbm_interface,
						      version);
		WL_TBM_RETURN_IF_FAIL(tbm_client->wl_tbm != NULL);

		wl_tbm_add_listener(tbm_client->wl_tbm, &wl_tbm_client_listener, tbm_client);
		wl_proxy_set_queue((struct wl_proxy *)tbm_client->wl_tbm, tbm_client->wl_queue);

		tbm_client->bufmgr = tbm_bufmgr_init(-1);
		if (!tbm_client->bufmgr) {
			WL_TBM_LOG("failed to get auth_info\n");

			tbm_client->wl_tbm = NULL;
			return;
		}
	}
}

static const struct wl_registry_listener registry_listener = {
	_wayland_tbm_client_registry_handle_global,
	NULL
};

struct wayland_tbm_client *
wayland_tbm_client_init(struct wl_display *display)
{
	WL_TBM_RETURN_VAL_IF_FAIL(display != NULL, NULL);

	struct wayland_tbm_client *tbm_client = NULL;
	struct wl_registry *wl_registry;

	_wayland_tbm_check_dlog_enable();

	tbm_client = calloc(1, sizeof(struct wayland_tbm_client));
	WL_TBM_RETURN_VAL_IF_FAIL(tbm_client != NULL, NULL);

	tbm_client->dpy = display;

	tbm_client->wl_queue = wl_display_create_queue(display);
	if (!tbm_client->wl_queue) {
		WL_TBM_LOG("Failed to create queue.\n");

		free(tbm_client);
		return NULL;
	}

	wl_registry = wl_display_get_registry(display);
	if (!wl_registry) {
		WL_TBM_LOG("Failed to get registry\n");

		wl_event_queue_destroy(tbm_client->wl_queue);
		free(tbm_client);
		return NULL;
	}

	wl_proxy_set_queue((struct wl_proxy *)wl_registry, tbm_client->wl_queue);

	wl_registry_add_listener(wl_registry, &registry_listener, tbm_client);
	wl_display_roundtrip_queue(display, tbm_client->wl_queue);

	wl_event_queue_destroy(tbm_client->wl_queue);
	wl_registry_destroy(wl_registry);

	/* check wl_tbm */
	if (!tbm_client->wl_tbm) {
		WL_TBM_LOG("failed to create wl_tbm\n");
		wayland_tbm_client_deinit(tbm_client);
		return NULL;
	}

	/* wl_tbm's queue is destroyed above. We should make wl_tbm's queue to
	* display's default_queue.
	*/
	wl_proxy_set_queue((struct wl_proxy *)tbm_client->wl_tbm, NULL);

	/* queue_info list */
	wl_list_init(&tbm_client->queue_info_list);

	return tbm_client;
}

void
wayland_tbm_client_deinit(struct wayland_tbm_client *tbm_client)
{
	if (!tbm_client)
		return;

	if (tbm_client->wl_tbm) {
		wl_tbm_set_user_data(tbm_client->wl_tbm, NULL);
		wl_tbm_destroy(tbm_client->wl_tbm);
	}

	if (tbm_client->bufmgr)
		tbm_bufmgr_deinit(tbm_client->bufmgr);

	free(tbm_client);
}

struct wl_buffer *
wayland_tbm_client_create_buffer(struct wayland_tbm_client *tbm_client,
				 tbm_surface_h surface)
{
	WL_TBM_RETURN_VAL_IF_FAIL(tbm_client != NULL, NULL);
	WL_TBM_RETURN_VAL_IF_FAIL(surface != NULL, NULL);

	int ret = -1;
	tbm_surface_info_s info;
	int num_buf;
	int bufs[TBM_SURF_PLANE_MAX] = { -1, -1, -1, -1};
	int is_fd = -1;
	struct wl_buffer *wl_buffer = NULL;
	int i;
	uint32_t flags = 0;
	struct wayland_tbm_buffer *buffer, *tmp;
	struct wayland_tbm_surface_queue *queue_info = NULL, *tmp_info = NULL;

	/* if the surface is the attached surface from display server,
	* return the wl_buffer of the attached surface
	*/
	if (!wl_list_empty(&tbm_client->queue_info_list)) {
		wl_list_for_each_safe(queue_info, tmp_info, &tbm_client->queue_info_list, link) {
			wl_list_for_each_safe(buffer, tmp, &queue_info->attach_bufs, link) {
				if (buffer->tbm_surface == surface) {
					return buffer->wl_buffer;
				}
			}
		}
	}

	ret = tbm_surface_get_info(surface, &info);
	if (ret != TBM_SURFACE_ERROR_NONE) {
		WL_TBM_LOG("Failed to create buffer from surface\n");
		return NULL;
	}

	if (info.num_planes > 3) {
		WL_TBM_LOG("invalid num_planes(%d)\n", info.num_planes);
		return NULL;
	}

	num_buf = tbm_surface_internal_get_num_bos(surface);
	if (num_buf == 0) {
		WL_TBM_LOG("surface doesn't have any bo.\n");
		goto err;
	}

	for (i = 0; i < num_buf; i++) {
		tbm_bo bo = tbm_surface_internal_get_bo(surface, i);
		if (bo == NULL) {
			WL_TBM_LOG("Failed to get bo from surface\n");
			goto err;
		}

		/* try to get fd first */
		if (is_fd == -1 || is_fd == 1) {
			bufs[i] = tbm_bo_export_fd(bo);
			if (bufs[i] >= 0)
				is_fd = 1;
		}

		/* if fail to get fd, try to get name second */
		if (is_fd == -1 || is_fd == 0) {
			bufs[i] = tbm_bo_export(bo);
			if (bufs[i] > 0)
				is_fd = 0;
		}

		if (is_fd == -1 ||
		    (is_fd == 1 && bufs[i] < 0) ||
		    (is_fd == 0 && bufs[i] <= 0)) {
			WL_TBM_LOG("Failed to export(is_fd:%d, bufs:%d)\n", is_fd, bufs[i]);
			goto err;
		}
	}

	if (is_fd == 1)
		wl_buffer = wl_tbm_create_buffer_with_fd(tbm_client->wl_tbm,
				info.width, info.height, info.format, info.num_planes,
				tbm_surface_internal_get_plane_bo_idx(surface, 0),
				info.planes[0].offset, info.planes[0].stride,
				tbm_surface_internal_get_plane_bo_idx(surface, 1),
				info.planes[1].offset, info.planes[1].stride,
				tbm_surface_internal_get_plane_bo_idx(surface, 2),
				info.planes[2].offset, info.planes[2].stride,
				flags, num_buf, bufs[0],
				(bufs[1] == -1) ? bufs[0] : bufs[1],
				(bufs[2] == -1) ? bufs[0] : bufs[2]);
	else
		wl_buffer = wl_tbm_create_buffer(tbm_client->wl_tbm,
						 info.width, info.height, info.format, info.num_planes,
						 tbm_surface_internal_get_plane_bo_idx(surface, 0),
						 info.planes[0].offset, info.planes[0].stride,
						 tbm_surface_internal_get_plane_bo_idx(surface, 1),
						 info.planes[1].offset, info.planes[1].stride,
						 tbm_surface_internal_get_plane_bo_idx(surface, 2),
						 info.planes[2].offset, info.planes[2].stride,
						 flags,
						 num_buf, bufs[0], bufs[1], bufs[2]);

	if (!wl_buffer) {
		WL_TBM_LOG("Failed to create wl_buffer\n");
		goto err;
	}

	for (i = 0; i < TBM_SURF_PLANE_MAX; i++) {
		if (is_fd == 1 && (bufs[i] >= 0))
			close(bufs[i]);
	}

    wl_buffer_set_user_data(wl_buffer, surface);

#ifdef DEBUG_TRACE
	WL_TBM_TRACE("        pid:%d wl_buffer:%p tbm_surface:%p\n", getpid(), wl_buffer, surface);
#endif

	return wl_buffer;

err:
	for (i = 0; i < TBM_SURF_PLANE_MAX; i++) {
		if (is_fd == 1 && (bufs[i] >= 0))
			close(bufs[i]);
	}

	return NULL;
}

void
wayland_tbm_client_destroy_buffer(struct wayland_tbm_client *tbm_client,
				  struct wl_buffer *wl_buffer)
{
	WL_TBM_RETURN_IF_FAIL(tbm_client != NULL);
	WL_TBM_RETURN_IF_FAIL(wl_buffer != NULL);

	// TODO: valid check if the buffer is from this tbm_client???

#ifdef DEBUG_TRACE
	WL_TBM_TRACE("       pid:%d wl_buffer:%p\n", getpid(), wl_buffer);
#endif

	wl_buffer_set_user_data(wl_buffer, NULL);
	wl_buffer_destroy(wl_buffer);
}

void *
wayland_tbm_client_get_bufmgr(struct wayland_tbm_client *tbm_client)
{
	WL_TBM_RETURN_VAL_IF_FAIL(tbm_client != NULL, NULL);

	return (void *)tbm_client->bufmgr;
}

struct wl_tbm *
_wayland_tbm_client_get_wl_tbm(struct wayland_tbm_client *tbm_client)
{
	WL_TBM_RETURN_VAL_IF_FAIL(tbm_client != NULL, NULL);

	return tbm_client->wl_tbm;
}

static void
_wayland_tbm_client_queue_destory_attach_bufs(struct wayland_tbm_surface_queue *queue_info)
{
	struct wayland_tbm_buffer *buffer, *tmp;

	wl_list_for_each_safe(buffer, tmp, &queue_info->attach_bufs, link) {
#ifdef DEBUG_TRACE
		WL_TBM_TRACE("pid:%d wl_buffer:%p tbm_surface:%p\n", getpid(), buffer->wl_buffer, buffer->tbm_surface);
#endif

		if (!buffer->allocated) {
#ifdef DEBUG_TRACE
			WL_TBM_TRACE("destroy the wl_buffer:%p\n", buffer->wl_buffer);
#endif
			wl_buffer_destroy(buffer->wl_buffer);
			buffer->wl_buffer = NULL;
		}
		tbm_surface_internal_unref(buffer->tbm_surface);
		wl_list_remove(&buffer->link);
		free(buffer);
	}

}

static void
_wayland_tbm_client_surface_queue_flush(struct wayland_tbm_surface_queue *queue_info)
{
#ifdef DEBUG_TRACE
	WL_TBM_TRACE("pid:%d\n", getpid());
#endif
	_wayland_tbm_client_queue_destory_attach_bufs(queue_info);
	tbm_surface_queue_flush(queue_info->tbm_queue);
}

static tbm_surface_h
_wayland_tbm_client_create_surface_from_param(tbm_bufmgr bufmgr,
			 int is_fd,
			 int32_t	 width,
			 int32_t height,
			 uint32_t format,
			 int32_t num_plane,
			 int32_t buf_idx0,
			 int32_t offset0,
			 int32_t stride0,
			 int32_t buf_idx1,
			 int32_t offset1,
			 int32_t stride1,
			 int32_t buf_idx2,
			 int32_t offset2,
			 int32_t stride2,
			 uint32_t flags,
			 int32_t num_buf,
			 uint32_t buf0,
			 uint32_t buf1,
			 uint32_t buf2)
{
	tbm_surface_h tbm_surface;

	int32_t names[TBM_SURF_PLANE_MAX] = { -1, -1, -1, -1};
	tbm_bo bos[TBM_SURF_PLANE_MAX];
	tbm_surface_info_s info;
	int bpp;
	int numPlane, numName = 0;
	int i;

	bpp = tbm_surface_internal_get_bpp(format);
	numPlane = tbm_surface_internal_get_num_planes(format);
	WL_TBM_RETURN_VAL_IF_FAIL(numPlane == num_plane, NULL);

	memset(&info, 0x0, sizeof(tbm_surface_info_s));

	info.width = width;
	info.height = height;
	info.format = format;
	info.bpp = bpp;
	info.num_planes = numPlane;

	/*Fill plane info*/
	if (numPlane > 0) {
		info.planes[0].offset = offset0;
		info.planes[0].stride = stride0;
		numPlane--;
	}

	if (numPlane > 0) {
		info.planes[1].offset = offset1;
		info.planes[1].stride = stride1;
		numPlane--;
	}

	if (numPlane > 0) {
		info.planes[2].offset = offset2;
		info.planes[2].stride = stride2;
		numPlane--;
	}

	/*Fill buffer*/
	numName = num_buf;
	names[0] = buf0;
	names[1] = buf1;
	names[2] = buf2;


	for (i = 0; i < numName; i++) {
		if (is_fd) {
			bos[i] = tbm_bo_import_fd(bufmgr, names[i]);
		} else {
			bos[i] = tbm_bo_import(bufmgr, names[i]);
		}
	}
	tbm_surface = tbm_surface_internal_create_with_bos(&info, bos, numName);
	WL_TBM_RETURN_VAL_IF_FAIL(tbm_surface != NULL, NULL);

	if (is_fd) {
		close(buf0);
		close(buf1);
		close(buf2);
	}

	for (i = 0; i < numName; i++) {
		tbm_bo_unref(bos[i]);
	}

	return tbm_surface;
}

static tbm_surface_h
__wayland_tbm_client_surface_alloc_cb(tbm_surface_queue_h surface_queue, void *data)
{
	struct wayland_tbm_surface_queue *queue_info = data;
	struct wayland_tbm_buffer *buffer, *tmp;
	tbm_surface_h surface = NULL;

	if (queue_info->is_active) {
		wl_list_for_each_safe(buffer, tmp, &queue_info->attach_bufs, link) {
			if (!buffer->allocated) {
				surface = buffer->tbm_surface;
				/* ref.. pair of __wayland_tbm_client_surface_free_cb */
				 tbm_surface_internal_ref(surface);
				buffer->allocated = 1;
#ifdef DEBUG_TRACE
				WL_TBM_TRACE("	 pid:%d wl_buffer:%p tbm_surface:%p ACTIVE\n", getpid(), buffer->wl_buffer, buffer->tbm_surface);
#endif
				break;
			}
		}
		if (!surface)
			WL_TBM_C_LOG("Error no available attached buffers\n");
	} else {
		int width = tbm_surface_queue_get_width(queue_info->tbm_queue);
		int height = tbm_surface_queue_get_height(queue_info->tbm_queue);
		int format = tbm_surface_queue_get_format(queue_info->tbm_queue);

        /* ref.. pair of __wayland_tbm_client_surface_free_cb */
		surface = tbm_surface_internal_create_with_flags(width,
							height,
							format,
							queue_info->flag);

#ifdef DEBUG_TRACE
		WL_TBM_TRACE("   pid:%d tbm_surface:%p DEACTIVE\n", getpid(), surface);
#endif
	}

	return surface;
}

static void
__wayland_tbm_client_surface_free_cb(tbm_surface_queue_h surface_queue, void *data,
		      tbm_surface_h surface)
{
#ifdef DEBUG_TRACE
	WL_TBM_TRACE("    pid:%d tbm_surface:%p\n", getpid(), surface);
#endif

    /* unref.. pair of __wayland_tbm_client_surface_alloc_cb */
	tbm_surface_internal_unref(surface);
}

static void
handle_tbm_queue_info(void *data,
		      struct wl_tbm_queue *wl_tbm_queue,
		      int32_t width,
		      int32_t height,
		      int32_t format,
		      uint32_t flags,
		      uint32_t num_buffers)
{
	struct wayland_tbm_surface_queue *queue_info = data;

	if (queue_info->tbm_queue) {
		WL_TBM_C_LOG("INFO cur(%dx%d fmt:0x%x num:%d) new(%dx%d fmt:0x%x num:%d)\n",
			queue_info->width, queue_info->height,
			queue_info->format, queue_info->num_bufs,
			width, height, format, num_buffers);
		return;
	}

	queue_info->width = width;
	queue_info->height = height;
	queue_info->format = format;
	queue_info->flag = flags;
	queue_info->num_bufs = num_buffers;

	queue_info->tbm_queue = tbm_surface_queue_sequence_create(num_buffers,
								width,
								height,
								format,
								flags);

	if (!queue_info->tbm_queue) {
		WL_TBM_C_LOG("failed to create_surface %dx%d format:0x%x flags:%d, num_bufs:%d",
			     width, height, format, flags, num_buffers);
	}

	tbm_surface_queue_set_alloc_cb(queue_info->tbm_queue,
				       __wayland_tbm_client_surface_alloc_cb,
				       __wayland_tbm_client_surface_free_cb,
				       queue_info);

#ifdef DEBUG_TRACE
	WL_TBM_TRACE("pid:%d\n", getpid());
#endif

}

static void
handle_tbm_queue_buffer_attached_with_id(void *data,
		struct wl_tbm_queue *wl_tbm_queue,
		struct wl_buffer *wl_buffer,
		int32_t width,
		int32_t height,
		uint32_t format,
		int32_t num_plane,
		int32_t buf_idx0,
		int32_t offset0,
		int32_t stride0,
		int32_t buf_idx1,
		int32_t offset1,
		int32_t stride1,
		int32_t buf_idx2,
		int32_t offset2,
		int32_t stride2,
		uint32_t flags,
		int32_t num_buf,
		uint32_t buf0,
		uint32_t buf1,
		uint32_t buf2)
{
	struct wayland_tbm_surface_queue *queue_info = data;
	struct wayland_tbm_buffer *buffer;

	buffer = calloc(1, sizeof(struct wayland_tbm_buffer));
	wl_list_init(&buffer->link);

	buffer->wl_tbm_queue = wl_tbm_queue;
	buffer->wl_buffer = wl_buffer;
	buffer->allocated = 0;

	WL_TBM_GOTO_IF_FAIL(buffer->wl_buffer != NULL, fail);

	buffer->tbm_surface = _wayland_tbm_client_create_surface_from_param(queue_info->bufmgr, 0,
			      width, height, format,
			      num_plane,
			      buf_idx0, offset0, stride0,
			      buf_idx1, offset1, stride1,
			      buf_idx2, offset2, stride2,
			      queue_info->flag,
			      num_buf,
			      buf0, buf1, buf2);
	WL_TBM_GOTO_IF_FAIL(buffer->tbm_surface != NULL, fail);
	buffer->flags = flags;

	wl_list_insert(&queue_info->attach_bufs, &buffer->link);

#ifdef DEBUG_TRACE
	WL_TBM_TRACE("pid:%d wl_buffer:%p tbm_surface:%p\n", getpid(), buffer->wl_buffer, buffer->tbm_surface);
#endif

	return;

fail:
	if (buffer->wl_buffer)
		wl_buffer_destroy(buffer->wl_buffer);

	if (buffer->tbm_surface)
		tbm_surface_destroy(buffer->tbm_surface);
	free(buffer);
}

static void
handle_tbm_queue_buffer_attached_with_fd(void *data,
		struct wl_tbm_queue *wl_tbm_queue,
		struct wl_buffer *wl_buffer,
		int32_t width,
		int32_t height,
		uint32_t format,
		int32_t num_plane,
		int32_t buf_idx0,
		int32_t offset0,
		int32_t stride0,
		int32_t buf_idx1,
		int32_t offset1,
		int32_t stride1,
		int32_t buf_idx2,
		int32_t offset2,
		int32_t stride2,
		uint32_t flags,
		int32_t num_buf,
		int32_t buf0,
		int32_t buf1,
		int32_t buf2)
{
	struct wayland_tbm_surface_queue *queue_info = data;
	struct wayland_tbm_buffer *buffer;

	buffer = calloc(1, sizeof(struct wayland_tbm_buffer));
	wl_list_init(&buffer->link);

	buffer->wl_tbm_queue = wl_tbm_queue;
	buffer->wl_buffer = wl_buffer;
	buffer->allocated = 0;

	WL_TBM_GOTO_IF_FAIL(buffer->wl_buffer != NULL, fail);

	buffer->tbm_surface = _wayland_tbm_client_create_surface_from_param(queue_info->bufmgr, 1,
			      width, height, format,
			      num_plane,
			      buf_idx0, offset0, stride0,
			      buf_idx1, offset1, stride1,
			      buf_idx2, offset2, stride2,
			      queue_info->flag,
			      num_buf,
			      buf0, buf1, buf2);
	WL_TBM_GOTO_IF_FAIL(buffer->tbm_surface != NULL, fail);
	buffer->flags = flags;

	wl_list_insert(&queue_info->attach_bufs, &buffer->link);

#ifdef DEBUG_TRACE
	WL_TBM_TRACE("pid:%d wl_buffer:%p tbm_surface:%p\n", getpid(), buffer->wl_buffer, buffer->tbm_surface);
#endif

	return;
fail:
	if (buffer->wl_buffer)
		wl_buffer_destroy(buffer->wl_buffer);

	if (buffer->tbm_surface)
		tbm_surface_destroy(buffer->tbm_surface);
	free(buffer);
}

static void
handle_tbm_queue_active(void *data,
			struct wl_tbm_queue *wl_tbm_queue,
			uint32_t usage)
{
	struct wayland_tbm_surface_queue *queue_info = data;

	if (queue_info->is_active) {
		WL_TBM_C_LOG("warning: queue_info is already activated\n");
		return;
	}
#ifdef DEBUG_TRACE
	WL_TBM_TRACE("                  pid:%d\n", getpid());
#endif

	queue_info->is_active = 1;
	queue_info->usage = usage;

	/* flush the allocated surfaces at the client */
	tbm_surface_queue_flush(queue_info->tbm_queue);
}

static void
handle_tbm_queue_deactive(void *data,
			  struct wl_tbm_queue *wl_tbm_queue)
{
	struct wayland_tbm_surface_queue *queue_info = data;

#ifdef DEBUG_TRACE
	WL_TBM_TRACE("                  pid:%d\n", getpid());
#endif

	if (!queue_info->is_active) {
		WL_TBM_C_LOG("warning: queue_info is already deactivated\n");
		return;
	}

	queue_info->is_active = 0;

	/* flush the attached surfaces */
	_wayland_tbm_client_surface_queue_flush(queue_info);
}

static void
handle_tbm_queue_flush(void *data,
		       struct wl_tbm_queue *wl_tbm_queue)
{
	struct wayland_tbm_surface_queue *queue_info = data;

#ifdef DEBUG_TRACE
	WL_TBM_TRACE("pid:%d\n", getpid());
#endif

	if (queue_info->is_active) {
		WL_TBM_C_LOG("warning: Cannot flush the tbm_surface_queueu. The queue is activate.\n");
		return;
	}

	/* flush the allocated surfaces at the client */
	tbm_surface_queue_flush(queue_info->tbm_queue);
}


const struct wl_tbm_queue_listener wl_tbm_queue_listener = {
	handle_tbm_queue_info,
	handle_tbm_queue_buffer_attached_with_id,
	handle_tbm_queue_buffer_attached_with_fd,
	handle_tbm_queue_active,
	handle_tbm_queue_deactive,
	handle_tbm_queue_flush
};

static struct wayland_tbm_surface_queue *
_wayland_tbm_client_find_queue_info_wl_surface(struct wayland_tbm_client *tbm_client,
					struct wl_surface *surface)
{
	struct wayland_tbm_surface_queue *queue_info = NULL, *tmp_info = NULL;

	/* set the debug_pid to the surface for debugging */
	if (!wl_list_empty(&tbm_client->queue_info_list)) {
		wl_list_for_each_safe(queue_info, tmp_info, &tbm_client->queue_info_list, link) {
			if (queue_info->wl_surface == surface) {
				return queue_info;
			}
		}
	}

	return NULL;
}

static void
handle_tbm_surface_queue_destroy_notify(tbm_surface_queue_h surface_queue,
		void *data)
{
	struct wayland_tbm_surface_queue *queue_info = data;

#ifdef DEBUG_TRACE
	WL_TBM_TRACE(" pid:%d\n", getpid());
#endif
;
	/* remove the attach_bufs int the queue_info */
	_wayland_tbm_client_queue_destory_attach_bufs(queue_info);

	if (queue_info->wl_tbm_queue)
		wl_tbm_queue_destroy(queue_info->wl_tbm_queue);

	wl_list_remove(&queue_info->link);
	free(queue_info);
}

tbm_surface_queue_h
wayland_tbm_client_create_surface_queue(struct wayland_tbm_client *tbm_client,
					struct wl_surface *surface,
					int queue_size,
					int width, int height, tbm_format format)
{
	struct wayland_tbm_surface_queue *queue_info;
	struct wl_tbm_queue *queue;

#ifdef DEBUG_TRACE
		WL_TBM_TRACE(" pid:%d\n", getpid());
#endif

	WL_TBM_RETURN_VAL_IF_FAIL(tbm_client != NULL, NULL);
	WL_TBM_RETURN_VAL_IF_FAIL(surface != NULL, NULL);

    /* find the queue_info associated with the wl_surface.
	 * client gets only one queue_info per a wl_surface.
	 */
	queue_info = _wayland_tbm_client_find_queue_info_wl_surface(tbm_client, surface);
	if (queue_info) {
#ifdef DEBUG_TRACE
		WL_TBM_TRACE(" wl_surface(%p) already get a tbm_surface_queue.\n", surface);
#endif
		return queue_info->tbm_queue;
	}

	queue_info = calloc(1, sizeof(struct wayland_tbm_surface_queue));
	WL_TBM_RETURN_VAL_IF_FAIL(queue_info != NULL, NULL);
	queue_info->bufmgr = tbm_client->bufmgr;
	queue_info->wl_surface = surface;
	wl_list_init(&queue_info->attach_bufs);

	queue = wl_tbm_create_surface_queue(tbm_client->wl_tbm, surface);
	WL_TBM_GOTO_IF_FAIL(queue != NULL, fail);
	queue_info->wl_tbm_queue = queue;

	wl_tbm_queue_add_listener(queue, &wl_tbm_queue_listener, queue_info);
	wl_display_roundtrip(tbm_client->dpy);

	queue_info->width = width;
	queue_info->height = height;
	queue_info->format = format;
	queue_info->flag = 0;

	queue_info->tbm_queue = tbm_surface_queue_sequence_create(queue_size,
				width, height, format, 0);

	WL_TBM_GOTO_IF_FAIL(queue_info->tbm_queue != NULL, fail);

	tbm_surface_queue_set_alloc_cb(queue_info->tbm_queue,
						__wayland_tbm_client_surface_alloc_cb,
						__wayland_tbm_client_surface_free_cb,
						queue_info);

	tbm_surface_queue_add_destroy_cb(queue_info->tbm_queue,
					handle_tbm_surface_queue_destroy_notify, queue_info);

#ifdef DEBUG_TRACE
	WL_TBM_C_LOG("INFO cur(%dx%d fmt:0x%x num:%d) new(%dx%d fmt:0x%x num:%d)\n",
		queue_info->width, queue_info->height,
		queue_info->format, queue_info->num_bufs,
		width, height, format, queue_size);
#endif

	/* add queue_info to the list */
	wl_list_insert(&tbm_client->queue_info_list, &queue_info->link);

	return queue_info->tbm_queue;
fail:
	if (queue_info) {
		if (queue_info->wl_tbm_queue)
			wl_tbm_queue_destroy(queue_info->wl_tbm_queue);

		free(queue_info);
	}
	return NULL;
}

struct wl_tbm_queue *
wayland_tbm_client_get_wl_tbm_queue(struct wayland_tbm_client *tbm_client, struct wl_surface *surface)
{
	struct wayland_tbm_surface_queue *queue_info = NULL;

	WL_TBM_RETURN_VAL_IF_FAIL(tbm_client != NULL, NULL);
	WL_TBM_RETURN_VAL_IF_FAIL(surface != NULL, NULL);

	queue_info = _wayland_tbm_client_find_queue_info_wl_surface(tbm_client, surface);
	WL_TBM_RETURN_VAL_IF_FAIL(queue_info != NULL, NULL);
	WL_TBM_RETURN_VAL_IF_FAIL(queue_info->wl_tbm_queue != NULL, NULL);

	return queue_info->wl_tbm_queue;
}

