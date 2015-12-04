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

#ifndef TBM_CLIENT_PROTOCOL_H
#define TBM_CLIENT_PROTOCOL_H

#ifdef  __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include "wayland-client.h"

struct wl_client;
struct wl_resource;

struct wl_buffer;
struct wl_tbm;

extern const struct wl_interface wl_tbm_interface;

#ifndef WL_TBM_ERROR_ENUM
#define WL_TBM_ERROR_ENUM
enum wl_tbm_error {
	WL_TBM_ERROR_AUTHENTICATE_FAIL = 0,
	WL_TBM_ERROR_INVALID_FORMAT = 1,
	WL_TBM_ERROR_INVALID_NAME = 2,
};
#endif /* WL_TBM_ERROR_ENUM */

struct wl_tbm_listener {
	/**
	 * authentication_info - (none)
	 * @device_name: (none)
	 * @capabilities: (none)
	 * @auth_fd: (none)
	 */
	void (*authentication_info)(void *data,
				    struct wl_tbm *wl_tbm,
				    const char *device_name,
				    uint32_t capabilities,
				    int32_t auth_fd);
	/**
	 * monitor_client_tbm_bo - (none)
	 * @command: (none)
	 * @trace_command: (none)
	 * @target: (none)
	 * @pid: (none)
	 */
	void (*monitor_client_tbm_bo)(void *data,
				      struct wl_tbm *wl_tbm,
				      int32_t command,
				      int32_t trace_command,
				      int32_t target,
				      int32_t pid);
};

static inline int
wl_tbm_add_listener(struct wl_tbm *wl_tbm,
		    const struct wl_tbm_listener *listener, void *data)
{
	return wl_proxy_add_listener((struct wl_proxy *) wl_tbm,
				     (void (**)(void)) listener, data);
}

#define WL_TBM_CREATE_BUFFER	0
#define WL_TBM_CREATE_BUFFER_WITH_FD	1
#define WL_TBM_GET_AUTHENTICATION_INFO	2
#define WL_TBM_REQUEST_TBM_MONITOR	3

static inline void
wl_tbm_set_user_data(struct wl_tbm *wl_tbm, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) wl_tbm, user_data);
}

static inline void *
wl_tbm_get_user_data(struct wl_tbm *wl_tbm)
{
	return wl_proxy_get_user_data((struct wl_proxy *) wl_tbm);
}

static inline void
wl_tbm_destroy(struct wl_tbm *wl_tbm)
{
	wl_proxy_destroy((struct wl_proxy *) wl_tbm);
}

static inline struct wl_buffer *
wl_tbm_create_buffer(struct wl_tbm *wl_tbm, int32_t width, int32_t height, uint32_t format, int32_t num_plane, int32_t buf_idx0, int32_t offset0, int32_t stride0, int32_t buf_idx1, int32_t offset1, int32_t stride1, int32_t buf_idx2, int32_t offset2, int32_t stride2, uint32_t flags, int32_t num_buf, uint32_t buf0, uint32_t buf1, uint32_t buf2)
{
	struct wl_proxy *id;

	id = wl_proxy_marshal_constructor((struct wl_proxy *) wl_tbm,
			 WL_TBM_CREATE_BUFFER, &wl_buffer_interface, NULL, width, height, format, num_plane, buf_idx0, offset0, stride0, buf_idx1, offset1, stride1, buf_idx2, offset2, stride2, flags, num_buf, buf0, buf1, buf2);

	return (struct wl_buffer *) id;
}

static inline struct wl_buffer *
wl_tbm_create_buffer_with_fd(struct wl_tbm *wl_tbm, int32_t width, int32_t height, uint32_t format, int32_t num_plane, int32_t buf_idx0, int32_t offset0, int32_t stride0, int32_t buf_idx1, int32_t offset1, int32_t stride1, int32_t buf_idx2, int32_t offset2, int32_t stride2, uint32_t flags, int32_t num_buf, int32_t buf0, int32_t buf1, int32_t buf2)
{
	struct wl_proxy *id;

	id = wl_proxy_marshal_constructor((struct wl_proxy *) wl_tbm,
			 WL_TBM_CREATE_BUFFER_WITH_FD, &wl_buffer_interface, NULL, width, height, format, num_plane, buf_idx0, offset0, stride0, buf_idx1, offset1, stride1, buf_idx2, offset2, stride2, flags, num_buf, buf0, buf1, buf2);

	return (struct wl_buffer *) id;
}

static inline void
wl_tbm_get_authentication_info(struct wl_tbm *wl_tbm)
{
	wl_proxy_marshal((struct wl_proxy *) wl_tbm,
			 WL_TBM_GET_AUTHENTICATION_INFO);
}

static inline void
wl_tbm_request_tbm_monitor(struct wl_tbm *wl_tbm, int32_t command, int32_t trace_command, int32_t target, int32_t pid)
{
	wl_proxy_marshal((struct wl_proxy *) wl_tbm,
			 WL_TBM_REQUEST_TBM_MONITOR, command, trace_command, target, pid);
}

#ifdef  __cplusplus
}
#endif

#endif
