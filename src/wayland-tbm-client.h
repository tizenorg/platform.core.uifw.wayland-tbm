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

#ifndef WAYLAND_TBM_CLIENT_H
#define WAYLAND_TBM_CLIENT_H

#ifdef  __cplusplus
extern "C" {
#endif

#include <wayland-client.h>
#include <tbm_surface.h>

struct wayland_tbm_client;

struct wayland_tbm_client *wayland_tbm_client_init(struct wl_display* display);
void                       wayland_tbm_client_deinit(struct wayland_tbm_client* tbm_client);

struct wl_buffer *wayland_tbm_client_create_buffer(struct wayland_tbm_client* tbm_client, tbm_surface_h surface);
void              wayland_tbm_client_destroy_buffer(struct wayland_tbm_client *tbm_client, struct wl_buffer* buffer);

const char     *wayland_tbm_client_get_device_name(struct wayland_tbm_client* tbm_client);
uint32_t        wayland_tbm_client_get_capability(struct wayland_tbm_client* tbm_client);
void           *wayland_tbm_client_get_bufmgr(struct wayland_tbm_client* tbm_client);
tbm_surface_h   wayland_tbm_client_get_surface(struct wayland_tbm_client *tbm_client, struct wl_buffer* buffer)
;

#ifdef  __cplusplus
}
#endif

#endif
