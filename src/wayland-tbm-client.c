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
#include <tbm_surface_internal.h>

#include "wayland-tbm-client.h"
#include "wayland-tbm-client-protocol.h"
#include "wayland-tbm-int.h"


struct wayland_tbm_client {
	struct wl_display *dpy;
	struct wl_event_queue *wl_queue;
	struct wl_tbm *wl_tbm;

	tbm_bufmgr bufmgr;
};

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
				0, num_buf, bufs[0],
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
						 0,
						 num_buf, bufs[0], bufs[1], bufs[2]);

	if (!wl_buffer) {
		WL_TBM_LOG("Failed to create wl_buffer\n");
		goto err;
	}

	for (i = 0; i < TBM_SURF_PLANE_MAX; i++) {
		if (is_fd == 1 && (bufs[i] >= 0))
			close(bufs[i]);
	}

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
				  struct wl_buffer *buffer)
{
	WL_TBM_RETURN_IF_FAIL(tbm_client != NULL);
	WL_TBM_RETURN_IF_FAIL(buffer != NULL);

	// TODO: valid check if the buffer is from this tbm_client???

	wl_buffer_set_user_data(buffer, NULL);
	wl_buffer_destroy(buffer);
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

