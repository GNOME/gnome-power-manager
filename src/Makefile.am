## We require new-style dependency handling.
AUTOMAKE_OPTIONS = 1.7

EXTRA_DIST =						\
	gpm-marshal.list

INCLUDES =						\
	$(GLIB_CFLAGS)					\
	$(GNOME_CFLAGS)					\
	$(X11_CFLAGS)					\
	$(NOTIFY_CFLAGS)				\
	$(CANBERRA_CFLAGS)				\
	$(GSTREAMER_CFLAGS)				\
	$(CONTROL_CENTER_CFLAGS)			\
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
	gnome-power-manager				\
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
	gpm-brightness.h				\
	gpm-brightness.c				\
	gpm-marshal.h					\
	gpm-marshal.c					\
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

gnome_power_manager_SOURCES =				\
	gpm-dpms.h					\
	gpm-dpms.c					\
	gpm-phone.h					\
	gpm-phone.c					\
	gpm-osd-dialog.h				\
	gpm-osd-dialog.c				\
	gpm-backlight.h					\
	gpm-backlight.c					\
	gpm-idle.h					\
	gpm-idle.c					\
	gpm-disks.h					\
	gpm-disks.c					\
	gpm-control.h					\
	gpm-control.c					\
	gpm-button.h					\
	gpm-button.c					\
	gpm-kbd-backlight.h				\
	gpm-kbd-backlight.c				\
	gpm-main.c					\
	gpm-manager.h					\
	gpm-manager.c					\
	gpm-tray-icon.h					\
	gpm-tray-icon.c					\
	gpm-screensaver.h				\
	gpm-screensaver.c				\
	gpm-stock-icons.h				\
	gsd-media-keys-window.h				\
	gsd-media-keys-window.c				\
	gsd-osd-window.h				\
	gsd-osd-window.c				\
	gpm-engine.h					\
	gpm-engine.c

gnome_power_manager_LDADD =				\
	libgpmshared.a					\
	$(GLIB_LIBS)					\
	$(X11_LIBS)					\
	$(GSTREAMER_LIBS)				\
	$(GNOME_LIBS)					\
	$(X11_LIBS)					\
	$(CANBERRA_LIBS)				\
	$(NOTIFY_LIBS)					\
	$(GPM_EXTRA_LIBS)				\
	$(UPOWER_LIBS)					\
	-lm

gnome_power_manager_CFLAGS =				\
	$(WARNINGFLAGS)

gnome_power_self_test_SOURCES =				\
	gpm-dpms.h					\
	gpm-dpms.c					\
	gpm-idle.h					\
	gpm-idle.c					\
	gpm-common.h					\
	gpm-common.c					\
	gpm-phone.h					\
	gpm-phone.c					\
	gpm-screensaver.h				\
	gpm-screensaver.c				\
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

BUILT_SOURCES = 					\
	gpm-marshal.c					\
	gpm-marshal.h

gpm-marshal.c: gpm-marshal.list
	echo "#include \"gpm-marshal.h\"" > $@ && \
	@GLIB_GENMARSHAL@ $< --prefix=gpm_marshal --body >> $@

gpm-marshal.h: gpm-marshal.list
	@GLIB_GENMARSHAL@ $< --prefix=gpm_marshal --header > $@

clean-local:
	rm -f *~
	rm -f gpm-marshal.c gpm-marshal.h

CLEANFILES = $(BUILT_SOURCES)

TESTS = gnome-power-self-test

MAINTAINERCLEANFILES =					\
	*~			      			\
	Makefile.in

