#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <wayland-server.h>
#include <wayland-tbm-server.h>

int
main(int argc, char *argv[])
{
	struct wl_display *dpy;
	struct wayland_tbm_server *tbm_server;

	const char *dpy_name;

	dpy = wl_display_create();
	if (!dpy) {
		printf("[SRV] failed to create display\n");
		return -1;
	}

	dpy_name = wl_display_add_socket_auto(dpy);
	printf("[SRV] wl_display : %s\n", dpy_name);

	tbm_server = wayland_tbm_server_init(dpy, NULL, -1, 0);
	if (!tbm_server) {
		printf("[SRV] failed to tbm_server init\n");
		wl_display_destroy(dpy);
		return -1;
	}

	wl_display_run(dpy);

	return 0;
}
