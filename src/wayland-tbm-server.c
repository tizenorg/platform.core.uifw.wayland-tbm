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

#define WL_TBM_SERVER_DEBUG

#define MIN(x,y) (((x)<(y))?(x):(y))

struct wayland_tbm_server {
	struct wl_display *display;
	struct wl_global *wl_tbm_global;

	struct wl_list client_resource_list;

	tbm_bufmgr bufmgr;

	/*Scanout*/
	struct wl_list queue_list;
	struct wl_list server_queue_list;
};

struct wl_tbm_buffer {
	struct wl_resource *resource;
	tbm_surface_h tbm_surface;
	tbm_surface_queue_h tbm_queue;
	int flags;

	const struct wl_buffer_interface *impl;
	void *user_data;
};

struct wayland_tbm_client_resource {
	struct wl_resource *resource;
	pid_t pid;
	char *app_name;
	struct wl_list link;
};

struct wayland_tbm_server_queue {
	struct wl_list link;
	struct wayland_tbm_server *tbm_srv;
	tbm_surface_queue_h tbm_queue;
	uint32_t flags;

	struct wayland_tbm_client_queue *client_queue;
};

struct wayland_tbm_client_queue {
	struct wl_list link;

	struct wl_resource *resource;
	struct wl_resource *wl_tbm;
	struct wl_resource *surface;
	struct wayland_tbm_server_queue *server_queue;

	int num_attached;
	struct wl_list attached_list;
};

static const int key_wl_tbm_queue;
#define KEY_WL_TBM_QUEUE ((unsigned long)&key_wl_tbm_queue)

const static int key_wl_tbm_buffer;
#define KEY_WL_TBM_BUFFER	((unsigned long)&key_wl_tbm_buffer)

static void _create_tbm_queue(struct wl_client *client,
			      struct wl_resource *resource,
			      uint32_t surface_queue,
			      struct wl_resource *surface);

static void _destory_tbm_queue(struct wl_resource *resource);

static void _buffer_destroy(struct wl_client *client,
			    struct wl_resource *resource);

static const struct wl_buffer_interface _wayland_tbm_buffer_impementation = {
	_buffer_destroy
};

static void
_destroy_buffer(struct wl_resource *resource)
{
	struct wl_tbm_buffer *buffer = wl_resource_get_user_data(resource);

	tbm_surface_internal_delete_user_data(buffer->tbm_surface, KEY_WL_TBM_BUFFER);

	if (buffer->tbm_queue) {
		tbm_surface_queue_release(buffer->tbm_queue, buffer->tbm_surface);
		WL_TBM_S_LOG("Release to queue(%p), tbm_surface:%p\n", buffer->tbm_queue,
			     buffer->tbm_surface);
	}

	tbm_surface_destroy(buffer->tbm_surface);

	free(buffer);
}

static void
_buffer_destroy(struct wl_client *client, struct wl_resource *resource)
{
	struct wl_tbm_buffer *buffer = wl_resource_get_user_data(resource);

	if (buffer->impl) {
		wl_resource_set_user_data(resource, buffer->user_data);
		buffer->impl->destroy(client, resource);
		wl_resource_set_user_data(resource, buffer);
	}

	wl_resource_destroy(resource);
}

static struct wl_tbm_buffer *
_create_wl_buffer(struct wl_client *client, struct wl_resource *resource,
		  uint id, tbm_surface_h tbm_buffer, int flags)
{
	struct wayland_tbm_server *tbm_srv = wl_resource_get_user_data(resource);
	struct wayland_tbm_client_resource *c_res = NULL, *tmp_res = NULL;
	struct wl_tbm_buffer *buffer;

	buffer = calloc(1, sizeof * buffer);
	if (buffer == NULL) {
		wl_resource_post_no_memory(resource);
		return NULL;
	}

	buffer->flags = flags;
	buffer->tbm_surface = (void *)tbm_buffer;
	if (buffer->tbm_surface == NULL) {
		wl_resource_post_error(resource,
				       WL_TBM_ERROR_INVALID_NAME,
				       "tbm_surface is NULL");
		free(buffer);
		return NULL;
	}

	/* set the debug_pid to the surface for debugging */
	if (!wl_list_empty(&tbm_srv->client_resource_list)) {
		wl_list_for_each_safe(c_res, tmp_res, &tbm_srv->client_resource_list, link) {
			if (c_res->resource == resource) {
				tbm_surface_internal_set_debug_pid(tbm_buffer, c_res->pid);
				break;
			}
		}
	}

	buffer->resource = wl_resource_create(client, &wl_buffer_interface, 1, id);
	if (!buffer->resource) {
		wl_resource_post_no_memory(resource);
		free(buffer);
		return NULL;
	}

	wl_resource_set_implementation(buffer->resource,
				       (void (* *)(void)) &_wayland_tbm_buffer_impementation,
				       buffer, _destroy_buffer);
	tbm_surface_internal_add_user_data(tbm_buffer, KEY_WL_TBM_BUFFER, NULL);
	tbm_surface_internal_set_user_data(tbm_buffer, KEY_WL_TBM_BUFFER,
					   (void *)buffer);

	return buffer;
}

static void
_create_buffer(struct wl_client *client, struct wl_resource *resource,
	       uint32_t id,
	       tbm_surface_info_s *info,
	       int32_t is_fd, int32_t *names, int32_t num_name, uint32_t flags)
{
	struct wayland_tbm_server *tbm_srv = wl_resource_get_user_data(resource);
	struct wl_tbm_buffer *buffer;
	tbm_surface_h surface;
	tbm_bo bos[TBM_SURF_PLANE_MAX];
	int i;

	for (i = 0; i < num_name; i++) {
		if (is_fd) {
			bos[i] = tbm_bo_import_fd(tbm_srv->bufmgr, names[i]);
		} else {
			bos[i] = tbm_bo_import(tbm_srv->bufmgr, names[i]);
		}
	}

	surface = tbm_surface_internal_create_with_bos(info, bos, num_name);
	for (i = 0; i < num_name; i++)
		tbm_bo_unref(bos[i]);

	buffer = _create_wl_buffer(client, resource, id, surface, flags);
	if (!buffer) {
		WL_TBM_S_LOG("failed to create wl_buffer id:%d\n", id);
		tbm_surface_destroy(surface);
		return;
	}

	return;
}

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

#ifdef WL_TBM_SERVER_DEBUG
	WL_TBM_LOG("[%s]: command=%d, trace_command=%d, target=%d, pid=%d.\n", __func__,
		   command, trace_command, target, pid);
#endif

	if (command == WL_TBM_MONITOR_COMMAND_LIST) {
		WL_TBM_DEBUG("==================  app list	 =======================\n");
		WL_TBM_DEBUG("no pid  app_name\n");

		if (!wl_list_empty(&tbm_srv->client_resource_list)) {
			wl_list_for_each_safe(c_res, tmp_res, &tbm_srv->client_resource_list, link) {
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
		if (!wl_list_empty(&tbm_srv->client_resource_list)) {
			wl_list_for_each_safe(c_res, tmp_res, &tbm_srv->client_resource_list, link) {
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
			if (!wl_list_empty(&tbm_srv->client_resource_list)) {
				wl_list_for_each_safe(c_res, tmp_res, &tbm_srv->client_resource_list, link) {
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
				       struct wl_resource *resource,
				       uint32_t id,
				       int32_t width, int32_t height, uint32_t format, int32_t num_plane,
				       int32_t buf_idx0, int32_t offset0, int32_t stride0,
				       int32_t buf_idx1, int32_t offset1, int32_t stride1,
				       int32_t buf_idx2, int32_t offset2, int32_t stride2,
				       uint32_t flags,
				       int32_t num_buf, uint32_t buf0, uint32_t buf1, uint32_t buf2)
{
	int32_t names[TBM_SURF_PLANE_MAX] = { -1, -1, -1, -1};
	tbm_surface_info_s info;
	int bpp;
	int numPlane, numName = 0;

#ifdef WL_TBM_SERVER_DEBUG
	WL_TBM_LOG("[%s]: trying.\n", __func__);
#endif

	bpp = tbm_surface_internal_get_bpp(format);
	numPlane = tbm_surface_internal_get_num_planes(format);
	if (numPlane != num_plane) {
		wl_resource_post_error(resource, WL_TBM_ERROR_INVALID_FORMAT,
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
	numName = num_buf;
	names[0] = buf0;
	names[1] = buf1;
	names[2] = buf2;

	_create_buffer(client, resource, id, &info, 0, names, numName, flags);
}

static void
_wayland_tbm_server_impl_create_buffer_with_fd(struct wl_client *client,
		struct wl_resource *resource,
		uint32_t id,
		int32_t width, int32_t height, uint32_t format, int num_plane,
		int32_t buf_idx0, int32_t offset0, int32_t stride0,
		int32_t buf_idx1, int32_t offset1, int32_t stride1,
		int32_t buf_idx2, int32_t offset2, int32_t stride2,
		uint32_t flags,
		int32_t num_buf, int32_t buf0, int32_t buf1, int32_t buf2)
{
	int32_t names[TBM_SURF_PLANE_MAX] = {0, 0, 0, 0};
	tbm_surface_info_s info;
	int bpp;
	int numPlane, numName = 0;

#ifdef WL_TBM_SERVER_DEBUG
	WL_TBM_LOG("[%s]: trying.\n", __func__);
#endif

	bpp = tbm_surface_internal_get_bpp(format);
	numPlane = tbm_surface_internal_get_num_planes(format);
	if (numPlane != num_plane) {
		wl_resource_post_error(resource, WL_TBM_ERROR_INVALID_FORMAT,
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
	numName = num_buf;
	names[0] = buf0;
	names[1] = buf1;
	names[2] = buf2;

	_create_buffer(client, resource, id, &info, 1, names, numName, flags);

	close(buf0);
	close(buf1);
	close(buf2);
}

static void
_wayland_tbm_server_impl_create_surface_queue(struct wl_client *client,
		struct wl_resource *resource,
		uint32_t surface_queue,
		struct wl_resource *surface)
{
	_create_tbm_queue(client, resource, surface_queue, surface);
}

static const struct wl_tbm_interface _wayland_tbm_server_implementation = {
	_wayland_tbm_server_impl_create_buffer,
	_wayland_tbm_server_impl_create_buffer_with_fd,
	_wayland_tbm_server_impl_request_tbm_monitor,
	_wayland_tbm_server_impl_create_surface_queue,
};

static void
_wayland_tbm_server_destroy_resource (struct wl_resource *resource)
{
	struct wayland_tbm_server *tbm_srv = NULL;
	struct wayland_tbm_client_resource *c_res = NULL, *tmp_res;

	/* remove the client resources to the list */
	tbm_srv = wl_resource_get_user_data(resource);
	if (!wl_list_empty(&tbm_srv->client_resource_list)) {
		wl_list_for_each_safe(c_res, tmp_res, &tbm_srv->client_resource_list, link) {
			if (c_res->resource == resource) {
#ifdef WL_TBM_SERVER_DEBUG
				WL_TBM_LOG("[%s]: resource,%p pid,%d \n", __func__, c_res->resource,
					   c_res->pid);
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

	struct wl_resource *resource;
	pid_t pid = 0;
	uid_t uid = 0;
	gid_t gid = 0;


	resource = wl_resource_create(client, &wl_tbm_interface, MIN(version, 1), id);
	if (!resource) {
		wl_client_post_no_memory(client);
		return;
	}

	wl_resource_set_implementation(resource,
				       &_wayland_tbm_server_implementation,
				       data,
				       _wayland_tbm_server_destroy_resource);

	/* add the client resources to the list */
	tbm_srv = wl_resource_get_user_data(resource);
	wl_client_get_credentials(client, &pid, &uid, &gid);

#ifdef WL_TBM_SERVER_DEBUG
	WL_TBM_LOG("[%s]: resource,%p pid,%d \n", __func__, resource, pid);
#endif

	c_res = calloc (1, sizeof(struct wayland_tbm_client_resource));
	c_res->pid = pid;
	c_res->resource = resource;
	wl_list_insert(&tbm_srv->client_resource_list, &c_res->link);
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

	/* init the client resource list */
	wl_list_init(&tbm_srv->client_resource_list);
	wl_list_init(&tbm_srv->server_queue_list);

	//init bufmgr
	tbm_srv->bufmgr = tbm_bufmgr_init(-1);
	if (!tbm_srv->bufmgr) {
		free(tbm_srv);
		return NULL;
	}

	if (!getenv("WL_TBM_EMBEDDED_SERVER")) {
		WL_TBM_S_LOG("Bind start\n");
		tbm_bufmgr_bind_native_display(tbm_srv->bufmgr, (void *)display);
	}
	tbm_srv->wl_tbm_global = wl_global_create(display, &wl_tbm_interface, 1,
				 tbm_srv, _wayland_tbm_server_bind_cb);

	//init wayland_tbm_client_queue
	wl_list_init(&tbm_srv->queue_list);

	return tbm_srv;
}

void
wayland_tbm_server_deinit(struct wayland_tbm_server *tbm_srv)
{
	wl_global_destroy(tbm_srv->wl_tbm_global);

	tbm_bufmgr_deinit(tbm_srv->bufmgr);

	free(tbm_srv);
}

tbm_surface_h
wayland_tbm_server_get_surface(struct wayland_tbm_server *tbm_srv,
			       struct wl_resource *resource)
{
	struct wl_tbm_buffer *wl_buffer;

	if (resource == NULL)
		return NULL;

	if (wl_resource_instance_of(resource, &wl_buffer_interface,
				    &_wayland_tbm_buffer_impementation)) {
		wl_buffer = wl_resource_get_user_data(resource);
		return wl_buffer->tbm_surface;
	}

	return NULL;
}

void *
wayland_tbm_server_get_bufmgr(struct wayland_tbm_server *tbm_srv)
{
	if (tbm_srv == NULL)
		return NULL;

	return (void *)tbm_srv->bufmgr;
}

static void
_wayland_tbm_queue_impl_destroy(struct wl_client *client,
				struct wl_resource *resource)
{
	_destory_tbm_queue(resource);
}

static void
_wayland_tbm_queue_impl_detach_buffer(struct wl_client *client,
				      struct wl_resource *resource,
				      struct wl_resource *buffer)
{
}

static const struct wl_tbm_queue_interface _wayland_tbm_queue_impementation = {
	_wayland_tbm_queue_impl_destroy,
	_wayland_tbm_queue_impl_detach_buffer,
};

struct wayland_tbm_client_queue *
_find_tbm_queue(struct wayland_tbm_server *tbm_srv, struct wl_resource *surface)
{
	struct wayland_tbm_client_queue *queue = NULL;

	wl_list_for_each(queue, &tbm_srv->queue_list, link) {
		if (queue && queue->surface == surface)
			return queue;
	}

	return NULL;
}

static void
_destory_tbm_queue(struct wl_resource *resource)
{
	struct wayland_tbm_client_queue *queue = wl_resource_get_user_data(resource);

	if (queue) {
		if (queue->server_queue) {
			struct wayland_tbm_server_queue *server_queue = queue->server_queue;

			server_queue->client_queue = NULL;
		}

		wl_list_remove(&queue->link);
		free(queue);
		wl_resource_set_user_data(resource, NULL);
	}
}

static void
_create_tbm_queue(struct wl_client *client,
		  struct wl_resource *resource,
		  uint32_t surface_queue,
		  struct wl_resource *surface)
{
	struct wayland_tbm_server *tbm_srv = wl_resource_get_user_data(resource);
	struct wayland_tbm_client_queue *queue;

	queue = calloc(1, sizeof * queue);
	if (queue == NULL) {
		wl_resource_post_no_memory(resource);
		return;
	}

	wl_list_init(&queue->link);
	queue->wl_tbm = resource;
	queue->surface = surface;
	queue->resource = wl_resource_create(client, &wl_tbm_queue_interface, 1,
					     surface_queue);
	if (!queue->resource) {
		wl_resource_post_no_memory(resource);
		free(queue);
		return;
	}

	wl_resource_set_implementation(queue->resource,
				       (void (* *)(void)) &_wayland_tbm_queue_impementation,
				       queue, _destory_tbm_queue);
	wl_list_insert(&tbm_srv->queue_list, &queue->link);
}

uint32_t
wayland_tbm_server_get_buffer_flags(struct wayland_tbm_server *tbm_srv, struct wl_resource *wl_buffer)
{
	struct wl_tbm_buffer *buffer;
	WL_TBM_RETURN_VAL_IF_FAIL(wl_buffer != NULL, 0);

	if (wl_resource_instance_of(wl_buffer, &wl_buffer_interface,
				    &_wayland_tbm_buffer_impementation)) {
		buffer = wl_resource_get_user_data(wl_buffer);
		return buffer->flags;
	}

	return 0;
}

void
wayland_tbm_server_set_buffer_implementation(struct wl_resource *wl_buffer, const struct wl_buffer_interface* impl, void* user_data)
{
	struct wl_tbm_buffer *buffer = wl_resource_get_user_data(wl_buffer);
	WL_TBM_RETURN_IF_FAIL(buffer != NULL);

	buffer->impl = impl;
	buffer->user_data = user_data;
}

static struct wl_resource*
_export_buffer_to_client_queue(struct wayland_tbm_client_queue *client_queue,
			    tbm_surface_h buffer, uint32_t flags)
{
	struct wl_tbm_buffer *wl_tbm_buffer = NULL;
	tbm_surface_info_s info;
	int num_buf;
	int bufs[TBM_SURF_PLANE_MAX] = { -1, -1, -1, -1};
	int is_fd = -1;
	int ret = -1, i;

	wl_tbm_buffer = _create_wl_buffer(wl_resource_get_client(client_queue->resource),
					  client_queue->wl_tbm,
					  0, buffer, flags);
	WL_TBM_RETURN_VAL_IF_FAIL(wl_tbm_buffer != NULL, NULL);
	tbm_surface_internal_ref(buffer);

	ret = tbm_surface_get_info(buffer, &info);
	if (ret != TBM_SURFACE_ERROR_NONE) {
		WL_TBM_S_LOG("Failed to create buffer from surface\n");
		_destroy_buffer(wl_tbm_buffer->resource);
		return NULL;
	}

	if (info.num_planes > 3) {
		WL_TBM_S_LOG("invalid num_planes(%d)\n", info.num_planes);
		_destroy_buffer(wl_tbm_buffer->resource);
		return NULL;
	}

	num_buf = tbm_surface_internal_get_num_bos(buffer);
	if (num_buf == 0) {
		WL_TBM_S_LOG("surface doesn't have any bo.\n");
		_destroy_buffer(wl_tbm_buffer->resource);
		goto err;
	}

	for (i = 0; i < num_buf; i++) {
		tbm_bo bo = tbm_surface_internal_get_bo(buffer, i);
		if (bo == NULL) {
			WL_TBM_S_LOG("Failed to get bo from surface\n");
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
			WL_TBM_S_LOG("Failed to export(is_fd:%d, bufs:%d)\n", is_fd, bufs[i]);
			goto err;
		}
	}

	if (is_fd == 1)
		wl_tbm_queue_send_buffer_attached_with_fd(client_queue->resource,
				wl_tbm_buffer->resource,
				info.width, info.height, info.format, info.num_planes,
				tbm_surface_internal_get_plane_bo_idx(buffer, 0),
				info.planes[0].offset, info.planes[0].stride,
				tbm_surface_internal_get_plane_bo_idx(buffer, 1),
				info.planes[1].offset, info.planes[1].stride,
				tbm_surface_internal_get_plane_bo_idx(buffer, 2),
				info.planes[2].offset, info.planes[2].stride,
				flags, num_buf, bufs[0],
				(bufs[1] == -1) ? bufs[0] : bufs[1],
				(bufs[2] == -1) ? bufs[0] : bufs[2]);
	else
		wl_tbm_queue_send_buffer_attached_with_fd(client_queue->resource,
				wl_tbm_buffer->resource,
				info.width, info.height, info.format, info.num_planes,
				tbm_surface_internal_get_plane_bo_idx(buffer, 0),
				info.planes[0].offset, info.planes[0].stride,
				tbm_surface_internal_get_plane_bo_idx(buffer, 1),
				info.planes[1].offset, info.planes[1].stride,
				tbm_surface_internal_get_plane_bo_idx(buffer, 2),
				info.planes[2].offset, info.planes[2].stride,
				flags, num_buf, bufs[0],
				(bufs[1] == -1) ? bufs[0] : bufs[1],
				(bufs[2] == -1) ? bufs[0] : bufs[2]);


	for (i = 0; i < TBM_SURF_PLANE_MAX; i++) {
		if (is_fd == 1 && (bufs[i] >= 0))
			close(bufs[i]);
	}

	WL_TBM_S_LOG("Release wl_tbm_queue tbm_surface(%p).\n", wl_tbm_buffer->tbm_surface);

	return wl_tbm_buffer->resource;
err:
	for (i = 0; i < TBM_SURF_PLANE_MAX; i++) {
		if (is_fd == 1 && (bufs[i] >= 0))
			close(bufs[i]);
	}

	if (wl_tbm_buffer) {
		_destroy_buffer(wl_tbm_buffer->resource);
		tbm_surface_internal_unref(buffer);
	}

	return NULL;
}

struct wayland_tbm_client_queue *
wayland_tbm_server_client_queue_get(struct wayland_tbm_server *tbm_srv, struct wl_resource *surface)
{
	struct wayland_tbm_client_queue *client_queue = NULL;
	WL_TBM_RETURN_VAL_IF_FAIL(tbm_srv != NULL, NULL);
	WL_TBM_RETURN_VAL_IF_FAIL(surface != NULL, NULL);

	wl_list_for_each(client_queue, &tbm_srv->queue_list, link) {
		if (client_queue && client_queue->surface == surface)
			return client_queue;
	}

	return NULL;
}

void
wayland_tbm_server_client_queue_activate(struct wayland_tbm_client_queue *client_queue, uint32_t usage)
{
	WL_TBM_RETURN_IF_FAIL(client_queue != NULL);

	wl_tbm_queue_send_active(client_queue->resource, usage);
}

void
wayland_tbm_server_client_queue_deactivate(struct wayland_tbm_client_queue *client_queue)
{
	WL_TBM_RETURN_IF_FAIL(client_queue != NULL);

	wl_tbm_queue_send_deactive(client_queue->resource);
}

struct wl_resource *
wayland_tbm_server_client_queue_export_buffer(struct wayland_tbm_client_queue *client_queue, tbm_surface_h surface, uint32_t flags)
{
	void *data;
	struct wl_resource *wl_buffer = NULL;

	WL_TBM_RETURN_VAL_IF_FAIL(client_queue != NULL, wl_buffer);
	WL_TBM_RETURN_VAL_IF_FAIL(surface != NULL, wl_buffer);
#if 0
	tbm_surface_internal_get_user_data(surface, KEY_WL_TBM_BUFFER, &data);
	if (data != NULL) {
		struct wl_tbm_buffer *buffer = (struct wl_tbm_buffer *)data;

        WL_TBM_S_LOG("tbm_surface_queue(%p) tbm_surface(%p) flags(%d)\n",
buffer->tbm_queue, buffer->flags, buffer->tbm_surface);
		WL_TBM_S_LOG("WARNING...surface(%p) is already export\n", surface);
		return NULL;
	}
#endif
	wl_buffer = _export_buffer_to_client_queue(client_queue, surface, flags);

	return wl_buffer;
}
