## We require new-style dependency handling.
AUTOMAKE_OPTIONS = 1.7

INCLUDES =						\
	$(GLIB_CFLAGS)					\
	$(GNOME_CFLAGS)					\
	$(X11_CFLAGS)					\
	$(NOTIFY_CFLAGS)				\
	$(GSTREAMER_CFLAGS)				\
	$(UPOWER_CFLAGS)				\
	-DBINDIR=\"$(bindir)\"			 	\
	-DSBINDIR=\"$(sbindir)\"			\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"	\
	-DDATADIR=\"$(datadir)\"			\
	-DPREFIX=\""$(prefix)"\" 			\
	-DSYSCONFDIR=\""$(sysconfdir)"\" 		\
	-DLIBDIR=\""$(libdir)"\" 			\
	-DVERSION="\"$(VERSION)\"" 			\
	-DGPM_DATA=\"$(pkgdatadir)\"			\
	-DGTKBUILDERDIR=\"$(pkgdatadir)\"		\
	-DUP_DISABLE_DEPRECATED				\
	-DG_LOG_DOMAIN=\"PowerManager\"			\
	-I$(top_srcdir)

bin_PROGRAMS =						\
	gnome-power-statistics

sbin_PROGRAMS =						\
	gnome-power-backlight-helper	\
	$(NULL)

check_PROGRAMS =					\
	gnome-power-self-test

noinst_LIBRARIES = libgpmshared.a
libgpmshared_a_SOURCES =				\
	gpm-debug.h					\
	gpm-debug.c					\
	gpm-array-float.c				\
	gpm-array-float.h				\
	gpm-idletime.h					\
	gpm-idletime.c					\
	egg-console-kit.h				\
	egg-console-kit.c				\
	gpm-common.h					\
	gpm-common.c					\
	gpm-upower.c					\
	gpm-upower.h

gnome_power_backlight_helper_SOURCES =			\
	gpm-backlight-helper.c

gnome_power_backlight_helper_LDADD =			\
	libgpmshared.a					\
	$(GLIB_LIBS)					\
	-lm

gnome_power_backlight_helper_CFLAGS =			\
	$(WARNINGFLAGS)

gnome_power_statistics_SOURCES =			\
	gpm-statistics.c				\
	gpm-point-obj.c					\
	gpm-point-obj.h					\
	gpm-graph-widget.h				\
	gpm-graph-widget.c

gnome_power_statistics_LDADD =				\
	libgpmshared.a					\
	$(GLIB_LIBS)					\
	$(X11_LIBS)					\
	$(UPOWER_LIBS)					\
	$(GNOME_LIBS)					\
	-lm

gnome_power_statistics_CFLAGS =				\
	$(WARNINGFLAGS)

gnome_power_self_test_SOURCES =				\
	gpm-idle.h					\
	gpm-idle.c					\
	gpm-common.h					\
	gpm-common.c					\
	gpm-idletime.h					\
	gpm-idletime.c					\
	gpm-array-float.h				\
	gpm-array-float.c				\
	gpm-self-test.c

gnome_power_self_test_LDADD =				\
	libgpmshared.a					\
	$(GLIB_LIBS)					\
	$(X11_LIBS)					\
	$(GNOME_LIBS)					\
	$(GSTREAMER_LIBS)				\
	$(UPOWER_LIBS)					\
	$(X11_LIBS)					\
	$(NOTIFY_LIBS)					\
	$(GPM_EXTRA_LIBS)				\
	-lm

gnome_power_self_test_CFLAGS = -DEGG_TEST $(AM_CFLAGS) $(WARNINGFLAGS)

clean-local:
	rm -f *~

CLEANFILES = $(BUILT_SOURCES)

TESTS = gnome-power-self-test

MAINTAINERCLEANFILES =					\
	*~			      			\
	Makefile.in

