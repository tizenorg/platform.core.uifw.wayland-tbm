#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <wayland-client.h>
#include <wayland-tbm-client.h>
#include <tbm_surface.h>
#include <wayland-tbm-client-protocol.h>

int
main(int argc, char* argv[])
{
   struct wl_display *dpy = NULL;
   const char *dpy_name = NULL;
   struct wayland_tbm_client *tbm_client = NULL;
   tbm_surface_h tbm_surf = NULL;
   struct wl_buffer *buf = NULL;
   int res = -1;

   if (argc > 0) {
      dpy_name = argv[1];
      printf("[APP] Connect to %s\n", dpy_name);
   }

   dpy = wl_display_connect(dpy_name);
   if (!dpy) {
      printf("[APP] failed to connect server\n");
      return -1;
   }

   tbm_client = wayland_tbm_client_init(dpy);
   if (!tbm_client) {
      printf("[APP] failed to client init\n");
      goto finish;
   }

   tbm_surf = tbm_surface_create(100, 100, TBM_FORMAT_ABGR8888);
   if (!tbm_surf) {
      printf("[APP] failed to tbm_surface_create\n");
      goto finish;
   }

   buf = wayland_tbm_client_create_buffer(tbm_client, tbm_surf);
   if (!buf) {
      printf("[APP] failed to wayland_tbm_client_create_buffer\n");
      goto finish;
   }
   wl_display_flush(dpy);

   tbm_surface_destroy(tbm_surf);
   wayland_tbm_client_destroy_buffer(tbm_client, buf);

   tbm_surf = NULL;
   buf = NULL;

   tbm_surf = tbm_surface_create(200, 200, TBM_FORMAT_NV12);
   if (!tbm_surf) {
      printf("[APP] failed to tbm_surface_create\n");
      goto finish;
   }

   buf = wayland_tbm_client_create_buffer(tbm_client, tbm_surf);
   if (!buf) {
      printf("[APP] failed to wayland_tbm_client_create_buffer\n");
      goto finish;
   }
   wl_display_flush(dpy);

   while (1) {
      if (0 > wl_display_dispatch(dpy))
         break;
   }

   res = 0;

finish:
   if (buf) wayland_tbm_client_destroy_buffer(tbm_client, buf);
   if (tbm_surf) tbm_surface_destroy(tbm_surf);
   if (tbm_client) wayland_tbm_client_deinit(tbm_client);
   if (dpy) wl_display_disconnect(dpy);

   return res;
}
