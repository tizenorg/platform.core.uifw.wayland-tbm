#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <xf86drm.h>
#include <wayland-server.h>
#include <wayland-tbm-server.h>

int
main(int argc, char *argv[])
{
   struct wl_display *dpy;
   struct wl_tbm *tbm;
   int fd;
   const char *dev_name;
   const char *dpy_name;

   dpy = wl_display_create();
   if (!dpy) {
      printf("[SRV] failed to create display\n");
      return -1;
   }

   dpy_name = wl_display_add_socket_auto(dpy);
   printf("[SRV] wl_display : %s\n", dpy_name);

   fd = drmOpen("exynos", NULL);
   if (fd < 0) {      
      printf("[SRV] failed to open drm\n");
      wl_display_destroy(dpy);
      return -1;
   }

   dev_name = drmGetDeviceNameFromFd(fd);
   printf("[SRV] drm_device: %s\n", dev_name);

   tbm = wayland_tbm_server_init(dpy, dev_name, fd, 0);
   if (!tbm) {
      printf("[SRV] failed to tbm_server init\n");
      wl_display_destroy(dpy);
      close(fd);
      return -1;
   }

   wl_display_run(dpy);

   return 0;
}
