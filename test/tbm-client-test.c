#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <wayland-client.h>
#include <wayland-tbm-client.h>
#include <tbm_surface.h>
#include <wayland-tbm-client-protocol.h>

int main(int argc, char* argv[])
{
   struct wl_display* dpy;
   const char *dpy_name = NULL;
   struct wl_tbm* tbm;
   tbm_surface_h tbm_surf;
   struct wl_buffer* buf;

   if (argc > 0) {
      dpy_name = argv[1];
      printf("[APP] Connect to %s\n", dpy_name);
   }
   dpy = wl_display_connect(dpy_name);
   if (!dpy) {
      printf("[APP] failed to connect server\n");      
   }

   tbm = wayland_tbm_client_init(dpy);
   if (!tbm) {
      printf("[APP] failed to client init\n");
   }

   tbm_surf = tbm_surface_create(100, 100, TBM_FORMAT_ABGR8888);
   if (!tbm_surf) {
      printf("[APP] failed to tbm_surface_create\n");
   }
   
   buf = wayland_tbm_client_create_buffer(tbm, tbm_surf);
   if (!buf) {
      printf("[APP] failed to wayland_tbm_client_create_buffer\n");
   }
   wl_display_flush(dpy);

   tbm_surf = tbm_surface_create(200, 200, TBM_FORMAT_NV12);
   if (!tbm_surf) {
      printf("[APP] failed to tbm_surface_create\n");
   }
   
   buf = wayland_tbm_client_create_buffer(tbm, tbm_surf);
   if (!buf) {
      printf("[APP] failed to wayland_tbm_client_create_buffer\n");
   }
   wl_display_flush(dpy);

   while (1) {
      if (0 > wl_display_dispatch(dpy))
         break;
   }

   wl_display_disconnect(dpy);
   return 0;
}
