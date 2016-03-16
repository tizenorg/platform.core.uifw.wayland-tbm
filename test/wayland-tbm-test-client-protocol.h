/*
 * Copyright 2015 Samsung Electronics co., Ltd. All Rights Reserved.
 *
 * Permission to use, copy, modify, distribute, and sell this
 * software and its documentation for any purpose is hereby granted
 * without fee, provided that\n the above copyright notice appear in
 * all copies and that both that copyright notice and this permission
 * notice appear in supporting documentation, and that the name of
 * the copyright holders not be used in advertising or publicity
 * pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied
 * warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
 * ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF
 * THIS SOFTWARE.
 */

#ifndef TBM_TEST_CLIENT_PROTOCOL_H
#define TBM_TEST_CLIENT_PROTOCOL_H

#ifdef  __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include "wayland-client.h"

struct wl_client;
struct wl_resource;

struct wl_buffer;
struct wl_callback;
struct wl_tbm_test;
struct wl_test_surface;

extern const struct wl_interface wl_tbm_test_interface;
extern const struct wl_interface wl_test_surface_interface;

#define WL_TBM_TEST_CREATE_SURFACE	0
#define WL_TBM_TEST_SET_ACTIVE_QUEUE	1

#define WL_TBM_TEST_CREATE_SURFACE_SINCE_VERSION	1
#define WL_TBM_TEST_SET_ACTIVE_QUEUE_SINCE_VERSION	1

static inline void
wl_tbm_test_set_user_data(struct wl_tbm_test *wl_tbm_test, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) wl_tbm_test, user_data);
}

static inline void *
wl_tbm_test_get_user_data(struct wl_tbm_test *wl_tbm_test)
{
	return wl_proxy_get_user_data((struct wl_proxy *) wl_tbm_test);
}

static inline void
wl_tbm_test_destroy(struct wl_tbm_test *wl_tbm_test)
{
	wl_proxy_destroy((struct wl_proxy *) wl_tbm_test);
}

static inline struct wl_test_surface *
wl_tbm_test_create_surface(struct wl_tbm_test *wl_tbm_test)
{
	struct wl_proxy *surface;

	surface = wl_proxy_marshal_constructor((struct wl_proxy *) wl_tbm_test,
			 WL_TBM_TEST_CREATE_SURFACE, &wl_test_surface_interface, NULL);

	return (struct wl_test_surface *) surface;
}

static inline void
wl_tbm_test_set_active_queue(struct wl_tbm_test *wl_tbm_test, struct wl_test_surface *surface)
{
	wl_proxy_marshal((struct wl_proxy *) wl_tbm_test,
			 WL_TBM_TEST_SET_ACTIVE_QUEUE, surface);
}

#define WL_TEST_SURFACE_DESTROY	0
#define WL_TEST_SURFACE_ATTACH	1
#define WL_TEST_SURFACE_FRAME	2

#define WL_TEST_SURFACE_DESTROY_SINCE_VERSION	1
#define WL_TEST_SURFACE_ATTACH_SINCE_VERSION	1
#define WL_TEST_SURFACE_FRAME_SINCE_VERSION	1

static inline void
wl_test_surface_set_user_data(struct wl_test_surface *wl_test_surface, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) wl_test_surface, user_data);
}

static inline void *
wl_test_surface_get_user_data(struct wl_test_surface *wl_test_surface)
{
	return wl_proxy_get_user_data((struct wl_proxy *) wl_test_surface);
}

static inline void
wl_test_surface_destroy(struct wl_test_surface *wl_test_surface)
{
	wl_proxy_marshal((struct wl_proxy *) wl_test_surface,
			 WL_TEST_SURFACE_DESTROY);

	wl_proxy_destroy((struct wl_proxy *) wl_test_surface);
}

static inline void
wl_test_surface_attach(struct wl_test_surface *wl_test_surface, struct wl_buffer *buffer)
{
	wl_proxy_marshal((struct wl_proxy *) wl_test_surface,
			 WL_TEST_SURFACE_ATTACH, buffer);
}

static inline struct wl_callback *
wl_test_surface_frame(struct wl_test_surface *wl_test_surface)
{
	struct wl_proxy *callback;

	callback = wl_proxy_marshal_constructor((struct wl_proxy *) wl_test_surface,
			 WL_TEST_SURFACE_FRAME, &wl_callback_interface, NULL);

	return (struct wl_callback *) callback;
}

#ifdef  __cplusplus
}
#endif

#endif
