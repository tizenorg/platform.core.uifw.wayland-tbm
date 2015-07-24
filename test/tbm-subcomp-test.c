#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define WL_HIDE_DEPRECATED

#include <xf86drm.h>
#include <wayland-server.h>
#include <wayland-tbm-server.h>

#include <wayland-client.h>
#include <wayland-tbm-client.h>
#include <tbm_surface.h>
#include <wayland-tbm-client-protocol.h>

static struct wl_display* 
connect_remote(int argc, char* argv[])
{
   struct wl_display* dpy;
   const char *dpy_name = NULL;

   if (argc > 0) {
      dpy_name = argv[1];
      printf("[APP] Connect to %s\n", dpy_name);
   }

   dpy = wl_display_connect(dpy_name);
   if (!dpy) {
      printf("[APP] failed to connect server\n");      
   }

   return dpy;
}

int main(int argc, char* argv[])
{
   struct wl_display *dpy;
   struct wl_display *host;
   struct wl_tbm* tbm;
   const char *dpy_name;
   
   printf("Hello world\n");
   dpy = wl_display_create();
   if (!dpy) {
      printf("[SRV] failed to create display\n");
      return -1;
   }
   dpy_name = wl_display_add_socket_auto(dpy);
   printf("[SRV] wl_display : %s\n", dpy_name);

   host = connect_remote(argc, argv);
   if (!host) {
      printf("[SRV] failed to connect host\n");
      return -1;
   }

   tbm = wayland_tbm_embedded_server_init(dpy, host);
   printf("[SRV] bufmgr:%p\n", wayland_tbm_server_get_bufmgr(tbm));
   
   wl_display_run(dpy);
   return 0;
}
