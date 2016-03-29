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

#define WL_HIDE_DEPRECATED

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <unistd.h>
#include <fcntl.h>

#include <tbm_surface.h>
#include <tbm_surface_internal.h>
#include <wayland-server.h>

#include "wayland-tbm-server.h"
#include "wayland-tbm-server-protocol.h"

#include "wayland-tbm-int.h"

//#define WL_TBM_SERVER_DEBUG
#define DEBUG_TRACE
#ifdef DEBUG_TRACE
#define WL_TBM_TRACE(fmt, ...)   fprintf (stderr, "[WL_TBM_S(%d):%s] " fmt, getpid(), __func__, ##__VA_ARGS__)
#else
#define WL_TBM_TRACE(fmt, ...)
#endif


#define MIN(x,y) (((x)<(y))?(x):(y))

struct wayland_tbm_server {
	struct wl_display *display;
	struct wl_global *wl_tbm_global;

	tbm_bufmgr bufmgr;

	struct wl_list cqueue_list; /* for scanout buffer */
	struct wl_list cresource_list; /* for tbm monitor */
};

struct wayland_tbm_buffer {
	struct wl_resource *wl_buffer;
	tbm_surface_h surface;
	int flags;
	struct wl_client *client;

	wayland_tbm_server_surface_destroy_cb destroy_cb;
	void *user_data;
};

struct wayland_tbm_client_resource {
	struct wl_resource *resource;
	pid_t pid;
	char *app_name;

	struct wl_list link;
};

struct wayland_tbm_client_queue {
	struct wl_resource *wl_tbm;
	struct wl_resource *wl_tbm_queue;
	struct wl_resource *wl_surface;
	pid_t pid;

	struct wl_list link;
};

static const int key_wl_tbm_queue;
#define KEY_WL_TBM_QUEUE ((unsigned long)&key_wl_tbm_queue)

const static int key_tbm_buffer;
#define KEY_TBM_BUFFER	((unsigned long)&key_tbm_buffer)

static void _wayland_tbm_server_tbm_buffer_destroy(struct wayland_tbm_buffer *tbm_buffer);

static void
_wayland_tbm_server_buffer_destory(struct wl_resource *wl_buffer)
{
	struct wayland_tbm_buffer *tbm_buffer = wl_resource_get_user_data(wl_buffer);

#ifdef DEBUG_TRACE
	pid_t pid = 0; uid_t uid = 0; gid_t gid = 0;
	wl_client_get_credentials(tbm_buffer->client, &pid, &uid, &gid);
	WL_TBM_TRACE("            pid:%d tbm_surface:%p\n", pid, tbm_buffer->surface);
#endif
	_wayland_tbm_server_tbm_buffer_destroy(tbm_buffer);
}

static void
_wayland_tbm_server_tbm_buffer_impl_destroy(struct wl_client *client, struct wl_resource *wl_buffer)
{
#ifdef DEBUG_TRACE
	pid_t pid = 0; uid_t uid = 0; gid_t gid = 0;
	wl_client_get_credentials(client, &pid, &uid, &gid);
	WL_TBM_TRACE("   pid:%d wl_buffer destoroy.\n", pid);
#endif

    wl_resource_destroy(wl_buffer);
}

static const struct wl_buffer_interface _wayland_tbm_buffer_impementation = {
	_wayland_tbm_server_tbm_buffer_impl_destroy
};

static void
_wayland_tbm_server_tbm_buffer_destroy(struct wayland_tbm_buffer *tbm_buffer)
{
	if (tbm_buffer->destroy_cb)
        tbm_buffer->destroy_cb(tbm_buffer->surface, tbm_buffer->user_data);

	tbm_surface_internal_delete_user_data(tbm_buffer->surface, KEY_TBM_BUFFER);
	tbm_surface_internal_unref(tbm_buffer->surface);

	free(tbm_buffer);
}

static struct wayland_tbm_buffer *
_wayland_tbm_server_tbm_buffer_create(struct wl_resource *wl_tbm,
			struct wl_client *client, tbm_surface_h surface, uint id, int flags)
{
	struct wayland_tbm_server *tbm_srv = wl_resource_get_user_data(wl_tbm);
	struct wayland_tbm_client_resource *c_res = NULL, *tmp_res = NULL;
	struct wayland_tbm_buffer *tbm_buffer;

	tbm_buffer = calloc(1, sizeof * tbm_buffer);
	if (tbm_buffer == NULL) {
		WL_TBM_S_LOG("Error. fail to allocate a tbm_buffer.\n");
		return NULL;
	}

	/* create a wl_buffer resource */
	tbm_buffer->wl_buffer = wl_resource_create(client, &wl_buffer_interface, 1, id);
	if (!tbm_buffer->wl_buffer) {
		WL_TBM_S_LOG("Error. fail to create wl_buffer resource.\n");
		free(tbm_buffer);
		return NULL;
	}

	wl_resource_set_implementation(tbm_buffer->wl_buffer,
				       (void (* *)(void)) &_wayland_tbm_buffer_impementation,
				       tbm_buffer, _wayland_tbm_server_buffer_destory);

	tbm_buffer->flags = flags;
	tbm_buffer->surface = surface;
	tbm_buffer->client = client;

	tbm_surface_internal_add_user_data(surface, KEY_TBM_BUFFER, NULL);
	tbm_surface_internal_set_user_data(surface, KEY_TBM_BUFFER,
					   (void *)tbm_buffer);

	/* set the debug_pid to the surface for debugging */
	if (!wl_list_empty(&tbm_srv->cresource_list)) {
		wl_list_for_each_safe(c_res, tmp_res, &tbm_srv->cresource_list, link) {
			if (c_res->resource == wl_tbm) {
				tbm_surface_internal_set_debug_pid(surface, c_res->pid);
				break;
			}
		}
	}

	return tbm_buffer;
}

static void
_destroy_tbm_queue(struct wl_resource *wl_tbm_queue)
{
	struct wayland_tbm_client_queue *cqueue = wl_resource_get_user_data(wl_tbm_queue);

	if (cqueue) {
#ifdef DEBUG_TRACE
		WL_TBM_TRACE("destory queue. pid:%d\n", cqueue->pid);
#endif

		wl_list_remove(&cqueue->link);
		free(cqueue);

		wl_resource_set_user_data(wl_tbm_queue, NULL);
	}
}

static void
_wayland_tbm_server_queue_impl_destroy(struct wl_client *client,
				struct wl_resource *wl_tbm_queue)
{
#ifdef DEBUG_TRACE
	pid_t pid = 0; uid_t uid = 0; gid_t gid = 0;
	wl_client_get_credentials(client, &pid, &uid, &gid);
	WL_TBM_TRACE("wl_tbm_queue destory. pid:%d\n", pid);
#endif

	wl_resource_destroy(wl_tbm_queue);
}

static void
_wayland_tbm_server_queue_impl_detach_buffer(struct wl_client *client,
				      struct wl_resource *wl_tbm_queue,
				      struct wl_resource *wl_buffer)
{
#ifdef DEBUG_TRACE
	pid_t pid = 0; uid_t uid = 0; gid_t gid = 0;
	wl_client_get_credentials(client, &pid, &uid, &gid);
	WL_TBM_TRACE("detach buffer. pid:%d\n", pid);
#endif

}

static const struct wl_tbm_queue_interface _wayland_tbm_queue_impementation = {
	_wayland_tbm_server_queue_impl_destroy,
	_wayland_tbm_server_queue_impl_detach_buffer,
};

static void
_wayland_tbm_server_impl_request_tbm_monitor(struct wl_client *client,
		struct wl_resource *resource,
		int32_t command,
		int32_t trace_command,
		int32_t target,
		int32_t pid)
{
	struct wayland_tbm_server *tbm_srv = wl_resource_get_user_data(resource);
	struct wayland_tbm_client_resource *c_res = NULL, *tmp_res = NULL;
	int i = 0;

#ifdef DEBUG_TRACE
	WL_TBM_TRACE("command=%d, trace_command=%d, target=%d, pid=%d.\n",
		   command, trace_command, target, pid);
#endif

	if (command == WL_TBM_MONITOR_COMMAND_LIST) {
		WL_TBM_DEBUG("==================  app list	 =======================\n");
		WL_TBM_DEBUG("no pid  app_name\n");

		if (!wl_list_empty(&tbm_srv->cresource_list)) {
			wl_list_for_each_safe(c_res, tmp_res, &tbm_srv->cresource_list, link) {
				/* skip the requestor (wayland-tbm-monitor */
				if (c_res->resource == resource)
					continue;

				if (!c_res->app_name) {
					c_res->app_name = (char *) calloc(1, 255 * sizeof(char));

					_wayland_tbm_util_get_appname_from_pid(c_res->pid, c_res->app_name);
					_wayland_tbm_util_get_appname_brief(c_res->app_name);
				}

				WL_TBM_DEBUG("%-3d%-5d%s\n", ++i, c_res->pid, c_res->app_name);
			}
		}

		WL_TBM_DEBUG("======================================================\n");

		return;
	}

	if (target == WL_TBM_MONITOR_TARGET_CLIENT) {
		if (pid < 1) {
			wl_resource_post_error(resource, WL_TBM_ERROR_INVALID_FORMAT, "invalid format");
			return;
		}
		/* send the events to all client containing wl_tbm resource except for the wayland-tbm-monitor(requestor). */
		if (!wl_list_empty(&tbm_srv->cresource_list)) {
			wl_list_for_each_safe(c_res, tmp_res, &tbm_srv->cresource_list, link) {
				/* skip the requestor (wayland-tbm-monitor */
				if (c_res->resource == resource)
					continue;

				wl_tbm_send_monitor_client_tbm_bo(c_res->resource, command, trace_command,
								  target, pid);
			}
		}
	} else if (target == WL_TBM_MONITOR_TARGET_SERVER) {
		if (command == WL_TBM_MONITOR_COMMAND_SHOW) {
			tbm_bufmgr_debug_show(tbm_srv->bufmgr);
		} else if (command == WL_TBM_MONITOR_COMMAND_TRACE) {
			WL_TBM_LOG("[%s]: TRACE NOT IMPLEMENTED.\n", __func__);
		} else
			wl_resource_post_error(resource, WL_TBM_ERROR_INVALID_FORMAT,
					       "invalid format");
	} else if (target == WL_TBM_MONITOR_TARGET_ALL) {
		if (command == WL_TBM_MONITOR_COMMAND_SHOW) {
			/* send the events to all client containing wl_tbm resource except for the wayland-tbm-monitor(requestor). */
			if (!wl_list_empty(&tbm_srv->cresource_list)) {
				wl_list_for_each_safe(c_res, tmp_res, &tbm_srv->cresource_list, link) {
					/* skip the requestor (wayland-tbm-monitor */
					if (c_res->resource == resource)
						continue;

					wl_tbm_send_monitor_client_tbm_bo(c_res->resource, command, trace_command,
									  target, pid);
				}
			}
			tbm_bufmgr_debug_show(tbm_srv->bufmgr);
		} else if (command == WL_TBM_MONITOR_COMMAND_TRACE) {
			wl_tbm_send_monitor_client_tbm_bo(resource, command, trace_command, target,
							  pid);
			WL_TBM_LOG("[%s]: TRACE NOT IMPLEMENTED.\n", __func__);
		} else
			wl_resource_post_error(resource, WL_TBM_ERROR_INVALID_FORMAT,
					       "invalid format");
	} else
		wl_resource_post_error(resource, WL_TBM_ERROR_INVALID_FORMAT,
				       "invalid format");

}


static void
_wayland_tbm_server_impl_create_buffer(struct wl_client *client,
				       struct wl_resource *wl_tbm,
				       uint32_t id,
				       int32_t width, int32_t height, uint32_t format, int32_t num_plane,
				       int32_t buf_idx0, int32_t offset0, int32_t stride0,
				       int32_t buf_idx1, int32_t offset1, int32_t stride1,
				       int32_t buf_idx2, int32_t offset2, int32_t stride2,
				       uint32_t flags,
				       int32_t num_buf, uint32_t buf0, uint32_t buf1, uint32_t buf2)
{
	struct wayland_tbm_server *tbm_srv = wl_resource_get_user_data(wl_tbm);
	struct wayland_tbm_buffer *tbm_buffer = NULL;
	tbm_surface_h surface = NULL;
	tbm_surface_info_s info;
	tbm_bo bos[TBM_SURF_PLANE_MAX];
	int32_t names[TBM_SURF_PLANE_MAX] = { -1, -1, -1, -1};
	int bpp;
	int numPlane;
	int i;

	bpp = tbm_surface_internal_get_bpp(format);
	numPlane = tbm_surface_internal_get_num_planes(format);
	if (numPlane != num_plane) {
		wl_resource_post_error(wl_tbm, WL_TBM_ERROR_INVALID_FORMAT,
				       "invalid format");
		return;
	}

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
	names[0] = buf0;
	names[1] = buf1;
	names[2] = buf2;

	for (i = 0; i < num_buf; i++) {
		bos[i] = tbm_bo_import(tbm_srv->bufmgr, names[i]);
	}

	surface = tbm_surface_internal_create_with_bos(&info, bos, num_buf);
	for (i = 0; i < num_buf; i++)
		tbm_bo_unref(bos[i]);

	tbm_buffer = _wayland_tbm_server_tbm_buffer_create(wl_tbm, client, surface, id, flags);
	if (tbm_buffer == NULL) {
		tbm_surface_destroy(surface);
		wl_resource_post_no_memory(wl_tbm);
		return;
	}

#ifdef DEBUG_TRACE
	pid_t pid = 0; uid_t uid = 0; gid_t gid = 0;
	wl_client_get_credentials(client, &pid, &uid, &gid);
	WL_TBM_TRACE("pid:%d tbm_surface:%p\n", pid, surface);
#endif

}

static void
_wayland_tbm_server_impl_create_buffer_with_fd(struct wl_client *client,
		struct wl_resource *wl_tbm,
		uint32_t id,
		int32_t width, int32_t height, uint32_t format, int num_plane,
		int32_t buf_idx0, int32_t offset0, int32_t stride0,
		int32_t buf_idx1, int32_t offset1, int32_t stride1,
		int32_t buf_idx2, int32_t offset2, int32_t stride2,
		uint32_t flags,
		int32_t num_buf, int32_t buf0, int32_t buf1, int32_t buf2)
{
	struct wayland_tbm_server *tbm_srv = wl_resource_get_user_data(wl_tbm);
	struct wayland_tbm_buffer *tbm_buffer = NULL;
	tbm_surface_h surface = NULL;
	tbm_surface_info_s info;
	tbm_bo bos[TBM_SURF_PLANE_MAX];
	int32_t names[TBM_SURF_PLANE_MAX] = { -1, -1, -1, -1};
	int bpp;
	int numPlane;
	int i;

	bpp = tbm_surface_internal_get_bpp(format);
	numPlane = tbm_surface_internal_get_num_planes(format);
	if (numPlane != num_plane) {
		wl_resource_post_error(wl_tbm, WL_TBM_ERROR_INVALID_FORMAT,
				       "invalid format");
		return;
	}

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
	names[0] = buf0;
	names[1] = buf1;
	names[2] = buf2;

	for (i = 0; i < num_buf; i++) {
		bos[i] = tbm_bo_import_fd(tbm_srv->bufmgr, names[i]);
	}

	surface = tbm_surface_internal_create_with_bos(&info, bos, num_buf);
	for (i = 0; i < num_buf; i++)
		tbm_bo_unref(bos[i]);

	tbm_buffer = _wayland_tbm_server_tbm_buffer_create(wl_tbm, client, surface, id, flags);
	if (tbm_buffer == NULL) {
		tbm_surface_destroy(surface);
		wl_resource_post_no_memory(wl_tbm);
		return;
	}

#ifdef DEBUG_TRACE
	pid_t pid = 0; uid_t uid = 0; gid_t gid = 0;
	wl_client_get_credentials(client, &pid, &uid, &gid);
	WL_TBM_TRACE("pid:%d tbm_surface:%p\n", pid, surface);
#endif

	close(buf0);
	close(buf1);
	close(buf2);
}

static void
_wayland_tbm_server_impl_create_surface_queue(struct wl_client *client,
		struct wl_resource *wl_tbm,
		uint32_t surface_queue,
		struct wl_resource *wl_surface)
{
	struct wayland_tbm_server *tbm_srv = wl_resource_get_user_data(wl_tbm);
	struct wayland_tbm_client_queue *cqueue = NULL;
	pid_t pid = 0;
	uid_t uid = 0;
	gid_t gid = 0;

	cqueue = calloc(1, sizeof(struct wayland_tbm_client_queue));
	if (!cqueue) {
		wl_resource_post_no_memory(wl_tbm);
		return;
	}

	cqueue->wl_tbm = wl_tbm;
	cqueue->wl_surface = wl_surface;
	cqueue->wl_tbm_queue = wl_resource_create(client, &wl_tbm_queue_interface, 1,
										surface_queue);
	if (!cqueue->wl_tbm_queue) {
		wl_resource_post_no_memory(wl_tbm);
		free(cqueue);
		return;
	}

	wl_client_get_credentials(client, &pid, &uid, &gid);
	cqueue->pid = pid;

	wl_resource_set_implementation(cqueue->wl_tbm_queue,
				       (void (* *)(void)) &_wayland_tbm_queue_impementation,
				       cqueue, _destroy_tbm_queue);

	wl_list_init(&cqueue->link);

	/* add a cqueue to the list */
	wl_list_insert(&tbm_srv->cqueue_list, &cqueue->link);

#ifdef DEBUG_TRACE
	WL_TBM_TRACE(" pid:%d \n", pid);
#endif

}

static const struct wl_tbm_interface _wayland_tbm_server_implementation = {
	_wayland_tbm_server_impl_create_buffer,
	_wayland_tbm_server_impl_create_buffer_with_fd,
	_wayland_tbm_server_impl_request_tbm_monitor,
	_wayland_tbm_server_impl_create_surface_queue,
};

static void
_wayland_tbm_server_destroy_resource(struct wl_resource *wl_tbm)
{
	struct wayland_tbm_server *tbm_srv = NULL;
	struct wayland_tbm_client_resource *c_res = NULL, *tmp_res;

	/* remove the client resources to the list */
	tbm_srv = wl_resource_get_user_data(wl_tbm);
	if (!wl_list_empty(&tbm_srv->cresource_list)) {
		wl_list_for_each_safe(c_res, tmp_res, &tbm_srv->cresource_list, link) {
			if (c_res->resource == wl_tbm) {
#ifdef DEBUG_TRACE
				WL_TBM_TRACE("pid:%d resource:%p\n", c_res->pid, c_res->resource);
#endif
				wl_list_remove(&c_res->link);
				if (c_res->app_name)
					free(c_res->app_name);
				free(c_res);
				break;
			}
		}
	}
}

static void
_wayland_tbm_server_bind_cb(struct wl_client *client, void *data,
			    uint32_t version,
			    uint32_t id)
{
	struct wayland_tbm_server *tbm_srv = NULL;
	struct wayland_tbm_client_resource *c_res = NULL;
	struct wl_resource *wl_tbm;

	pid_t pid = 0;
	uid_t uid = 0;
	gid_t gid = 0;

	wl_tbm = wl_resource_create(client, &wl_tbm_interface, MIN(version, 1), id);
	if (!wl_tbm) {
		wl_client_post_no_memory(client);
		return;
	}

	wl_resource_set_implementation(wl_tbm,
				       &_wayland_tbm_server_implementation,
				       data,
				       _wayland_tbm_server_destroy_resource);

	/* add the client resources to the list */
	tbm_srv = wl_resource_get_user_data(wl_tbm);
	wl_client_get_credentials(client, &pid, &uid, &gid);

#ifdef DEBUG_TRACE
	WL_TBM_TRACE("wl_tbm bind pid:%d\n", pid);
#endif

	c_res = calloc (1, sizeof(struct wayland_tbm_client_resource));
	c_res->pid = pid;
	c_res->resource = wl_tbm;
	wl_list_insert(&tbm_srv->cresource_list, &c_res->link);
}

struct wayland_tbm_server *
wayland_tbm_server_init(struct wl_display *display, const char *device_name,
			int fd,
			uint32_t flags)
{
	struct wayland_tbm_server *tbm_srv;

	tbm_srv = calloc(1, sizeof(struct wayland_tbm_server));
	WL_TBM_RETURN_VAL_IF_FAIL(tbm_srv != NULL, NULL);

	tbm_srv->display = display;

	//init bufmgr
	tbm_srv->bufmgr = tbm_bufmgr_init(-1);
	if (!tbm_srv->bufmgr) {
		free(tbm_srv);
		return NULL;
	}

	tbm_srv->wl_tbm_global = wl_global_create(display, &wl_tbm_interface, 1,
				 tbm_srv, _wayland_tbm_server_bind_cb);


	//init wayland_tbm_client_queue
	wl_list_init(&tbm_srv->cqueue_list);

	/* init the client resource list */
	wl_list_init(&tbm_srv->cresource_list);


	return tbm_srv;
}

void
wayland_tbm_server_deinit(struct wayland_tbm_server *tbm_srv)
{
	WL_TBM_RETURN_IF_FAIL(tbm_srv != NULL);

	wl_global_destroy(tbm_srv->wl_tbm_global);

	tbm_bufmgr_deinit(tbm_srv->bufmgr);

	free(tbm_srv);
}

tbm_surface_h
wayland_tbm_server_get_surface(struct wayland_tbm_server *tbm_srv,
			       struct wl_resource *wl_buffer)
{
	struct wayland_tbm_buffer *tbm_buffer  = NULL;

//	WL_TBM_RETURN_VAL_IF_FAIL(tbm_srv != NULL, NULL);
	WL_TBM_RETURN_VAL_IF_FAIL(wl_buffer != NULL, NULL);

	if (wl_resource_instance_of(wl_buffer, &wl_buffer_interface,
				    &_wayland_tbm_buffer_impementation)) {
		tbm_buffer = wl_resource_get_user_data(wl_buffer);
		return tbm_buffer->surface;
	}

	return NULL;
}

void *
wayland_tbm_server_get_bufmgr(struct wayland_tbm_server *tbm_srv)
{
	WL_TBM_RETURN_VAL_IF_FAIL(tbm_srv != NULL, NULL);

	return (void *)tbm_srv->bufmgr;
}

uint32_t
wayland_tbm_server_get_buffer_flags(struct wayland_tbm_server *tbm_srv,
					struct wl_resource *wl_buffer)
{
	struct wayland_tbm_buffer *tbm_buffer  = NULL;

	WL_TBM_RETURN_VAL_IF_FAIL(wl_buffer != NULL, 0);

	if (wl_resource_instance_of(wl_buffer, &wl_buffer_interface,
				    &_wayland_tbm_buffer_impementation)) {
		tbm_buffer = wl_resource_get_user_data(wl_buffer);
		return tbm_buffer->flags;
	}

	return 0;
}

struct wayland_tbm_client_queue *
wayland_tbm_server_client_queue_get(struct wayland_tbm_server *tbm_srv, struct wl_resource *wl_surface)
{
	struct wayland_tbm_client_queue *cqueue = NULL;
	WL_TBM_RETURN_VAL_IF_FAIL(tbm_srv != NULL, NULL);
	WL_TBM_RETURN_VAL_IF_FAIL(wl_surface != NULL, NULL);

	wl_list_for_each(cqueue, &tbm_srv->cqueue_list, link) {
		if (cqueue && cqueue->wl_surface == wl_surface)
			return cqueue;
	}

	return NULL;
}

void
wayland_tbm_server_client_queue_activate(struct wayland_tbm_client_queue *cqueue, uint32_t usage)
{
	WL_TBM_RETURN_IF_FAIL(cqueue != NULL);
	WL_TBM_RETURN_IF_FAIL(cqueue->wl_tbm_queue != NULL);

#ifdef DEBUG_TRACE
	WL_TBM_TRACE("      pid:%d\n", cqueue->pid);
#endif
	wl_tbm_queue_send_active(cqueue->wl_tbm_queue, usage);
}

void
wayland_tbm_server_client_queue_deactivate(struct wayland_tbm_client_queue *cqueue)
{
	WL_TBM_RETURN_IF_FAIL(cqueue != NULL);
	WL_TBM_RETURN_IF_FAIL(cqueue->wl_tbm_queue != NULL);

#ifdef DEBUG_TRACE
	WL_TBM_TRACE("    pid:%d\n", cqueue->pid);
#endif

	wl_tbm_queue_send_deactive(cqueue->wl_tbm_queue);
}


int
_wayland_tbm_server_wl_tbm_queue_send_surface(struct wayland_tbm_client_queue *cqueue,
					struct wl_resource *wl_buffer, tbm_surface_h surface,
					uint32_t flags)
{
	struct wl_resource *wl_tbm_queue = cqueue->wl_tbm_queue;
	tbm_surface_info_s info;
	int num_buf;
	int bufs[TBM_SURF_PLANE_MAX] = { -1, -1, -1, -1};
	int is_fd = -1;
	int ret = -1, i;

	ret = tbm_surface_get_info(surface, &info);
	if (ret != TBM_SURFACE_ERROR_NONE) {
		WL_TBM_S_LOG("Failed to create buffer from surface\n");
		return 0;
	}

	if (info.num_planes > 3) {
		WL_TBM_S_LOG("invalid num_planes(%d)\n", info.num_planes);
		return 0;
	}

	num_buf = tbm_surface_internal_get_num_bos(surface);
	if (num_buf == 0) {
		WL_TBM_S_LOG("surface doesn't have any bo.\n");
		goto err;
	}

	for (i = 0; i < num_buf; i++) {
		tbm_bo bo = tbm_surface_internal_get_bo(surface, i);
		if (bo == NULL) {
			goto err;
		}

#ifdef DEBUG_TRACE
		WL_TBM_TRACE(" pid:%d tbm_surface:%p name:%d\n", cqueue->pid, surface, tbm_bo_export(bo));
#endif

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
			WL_TBM_S_LOG("Failed to export(is_fd:%d, bufs:%d)\n", is_fd, bufs[i]);
			goto err;
		}
	}

	if (is_fd == 1)
		wl_tbm_queue_send_buffer_attached_with_fd(wl_tbm_queue,
				wl_buffer,
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
		wl_tbm_queue_send_buffer_attached_with_fd(wl_tbm_queue,
				wl_buffer,
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


	for (i = 0; i < TBM_SURF_PLANE_MAX; i++) {
		if (is_fd == 1 && (bufs[i] >= 0))
			close(bufs[i]);
	}

	return 1;
err:
	for (i = 0; i < TBM_SURF_PLANE_MAX; i++) {
		if (is_fd == 1 && (bufs[i] >= 0))
			close(bufs[i]);
	}

	return 0;
}

int
wayland_tbm_server_client_queue_export_buffer(struct wayland_tbm_client_queue *cqueue,
			tbm_surface_h surface, uint32_t flags,
			wayland_tbm_server_surface_destroy_cb destroy_cb, void *user_data)
{
	struct wl_resource *wl_tbm = NULL;
	struct wayland_tbm_buffer *tbm_buffer = NULL;
	struct wl_client *client = NULL;
	void *data = NULL;

	WL_TBM_RETURN_VAL_IF_FAIL(cqueue != NULL, 0);
	WL_TBM_RETURN_VAL_IF_FAIL(cqueue->wl_tbm_queue != NULL, 0);
	WL_TBM_RETURN_VAL_IF_FAIL(surface != NULL, 0);

    /* return if the surface is already exported to the client on the cqueue */
	tbm_surface_internal_get_user_data(surface, KEY_TBM_BUFFER, &data);
	if (data != NULL) {
		//WL_TBM_S_LOG("WARNING...surface(%p) is already export\n", surface);
		return 0;
	}

	wl_tbm = cqueue->wl_tbm;
	client = wl_resource_get_client(cqueue->wl_tbm_queue);

	tbm_surface_internal_ref(surface);
	tbm_buffer = _wayland_tbm_server_tbm_buffer_create(wl_tbm, client, surface, 0, flags);
	if (tbm_buffer == NULL) {
		tbm_surface_internal_unref(surface);
		return 0;
	}

	tbm_buffer->destroy_cb = destroy_cb;
	tbm_buffer->user_data = user_data;

	if(!_wayland_tbm_server_wl_tbm_queue_send_surface(cqueue,
				tbm_buffer->wl_buffer, surface, flags)) {
		WL_TBM_S_LOG("Failed to send the surface to the wl_tbm_queue\n");
		_wayland_tbm_server_tbm_buffer_destroy(tbm_buffer);
		tbm_surface_internal_unref(surface);
		return 0;
	}

	return 1;
}

