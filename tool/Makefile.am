
bin_PROGRAMS = wayland-tbm-monitor
#check_PROGRAMS = wayland-tbm-monitor

wayland_tbm_monitor_CFLAGS = \
    -I$(top_srcdir)/src/ \
	$(AM_CFLAGS) \
	$(WL_TBM_COMMON_CFLAGS) \
	$(WL_TBM_CLIENT_CFLAGS)

wayland_tbm_monitor_LDADD = \
	$(WL_TBM_COMMON_LIBS) \
	$(WL_TBM_CLIENT_LIBS) \
	../src/libwayland-tbm-client.la

wayland_tbm_monitor_SOURCES = \
	wayland-tbm-monitor.c

