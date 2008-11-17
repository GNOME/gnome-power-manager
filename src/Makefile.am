SUBDIRS = dbus

## We require new-style dependency handling.
AUTOMAKE_OPTIONS = 1.7

NULL =

EXTRA_DIST =						\
	gpm-marshal.list				\
	$(NULL)

INCLUDES =						\
	$(GLIB_CFLAGS)					\
	$(HAL_CFLAGS)					\
	$(DBUS_CFLAGS)					\
	$(GNOME_CFLAGS)					\
	$(XRANDR_CFLAGS)				\
	$(LIBNOTIFY_CFLAGS)				\
	$(UNIQUE_CFLAGS)				\
	$(GSTREAMER_CFLAGS)				\
	$(POLKIT_CFLAGS)				\
	$(POLKIT_GNOME_CFLAGS)				\
	-DBINDIR=\"$(bindir)\"			 	\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"	\
	-DDATADIR=\"$(datadir)\"			\
	-DPREFIX=\""$(prefix)"\" 			\
	-DSYSCONFDIR=\""$(sysconfdir)"\" 		\
	-DLIBDIR=\""$(libdir)"\" 			\
	-DVERSION="\"$(VERSION)\"" 			\
	-DGPM_DATA=\"$(pkgdatadir)\"			\
	-DEGG_LOG_FILE=\""$(PK_LOG_DIR)/gnome-power-manager"\"	\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/libunique			\
	-I$(top_srcdir)/libhal-glib			\
	-I$(top_srcdir)/libdbus-glib			\
	$(NULL)

LOCAL_LIBHAL_LIBS =					\
	$(top_builddir)/libhal-glib/libhal-gdevice.la	\
	$(top_builddir)/libhal-glib/libhal-gdevicestore.la	\
	$(top_builddir)/libhal-glib/libhal-gmanager.la	\
	$(top_builddir)/libhal-glib/libhal-gpower.la

LOCAL_LIBDBUS_LIBS =						\
	$(top_builddir)/libdbus-glib/libdbus-monitor-session.la	\
	$(top_builddir)/libdbus-glib/libdbus-monitor-system.la	\
	$(top_builddir)/libdbus-glib/libdbus-watch.la		\
	$(top_builddir)/libdbus-glib/libdbus-proxy.la

LOCAL_LIBUNIQUE_LIBS =						\
	$(top_builddir)/libunique/libunique.la

bin_PROGRAMS =						\
	gnome-power-manager				\
	gnome-power-preferences				\
	gnome-power-statistics				\
	$(NULL)

check_PROGRAMS =					\
	gnome-power-self-test

gnome_power_statistics_SOURCES =			\
	egg-color.c					\
	egg-color.h					\
	egg-debug.h					\
	egg-debug.c					\
	gpm-statistics.c				\
	gpm-statistics-core.h				\
	gpm-statistics-core.c				\
	gpm-array.h					\
	gpm-array.c					\
	gpm-array-float.h				\
	gpm-array-float.c				\
	gpm-graph-widget.h				\
	gpm-graph-widget.c				\
	gpm-marshal.h					\
	gpm-marshal.c					\
	gpm-conf.h					\
	gpm-conf.c					\
	gpm-common.h					\
	gpm-common.c					\
	$(NULL)

gnome_power_statistics_LDADD =				\
	$(GLIB_LIBS)					\
	$(GNOME_LIBS)					\
	$(DBUS_LIBS)					\
	$(LOCAL_LIBDBUS_LIBS)				\
	$(LOCAL_LIBUNIQUE_LIBS)				\
	$(NULL)

if HAVE_UNIQUE
gnome_power_statistics_LDADD +=				\
	$(UNIQUE_LIBS)
endif

gnome_power_preferences_SOURCES =			\
	egg-debug.h					\
	egg-debug.c					\
	gpm-dpms.h					\
	gpm-dpms.c					\
	gpm-conf.h					\
	gpm-conf.c					\
	gpm-idle.h					\
	gpm-idle.c					\
	gpm-load.h					\
	gpm-load.c					\
	gpm-prefs.h					\
	gpm-prefs.c					\
	gpm-prefs-core.h				\
	gpm-prefs-core.c				\
	gpm-marshal.h					\
	gpm-marshal.c					\
	gpm-common.h					\
	gpm-common.c					\
	gpm-screensaver.h				\
	gpm-screensaver.c				\
	$(NULL)

gnome_power_preferences_LDADD =				\
	$(GLIB_LIBS)					\
	$(GNOME_LIBS)					\
	$(HAL_LIBS)					\
	$(DBUS_LIBS)					\
	$(POLKIT_GNOME_LIBS)				\
	$(GPM_EXTRA_LIBS)				\
	$(LOCAL_LIBHAL_LIBS)				\
	$(LOCAL_LIBDBUS_LIBS)				\
	$(LOCAL_LIBUNIQUE_LIBS)				\
	$(NULL)

if HAVE_UNIQUE
gnome_power_preferences_LDADD +=			\
	$(UNIQUE_LIBS)
endif

gnome_power_manager_SOURCES =				\
	egg-color.c					\
	egg-color.h					\
	egg-debug.h					\
	egg-debug.c					\
	egg-console-kit.h				\
	egg-console-kit.c				\
	gpm-dpms.h					\
	gpm-dpms.c					\
	gpm-notify.h					\
	gpm-notify.c					\
	gpm-engine.h					\
	gpm-engine.c					\
	gpm-phone.h					\
	gpm-phone.c					\
	gpm-cell.h					\
	gpm-cell.c					\
	gpm-cell-unit.h					\
	gpm-cell-unit.c					\
	gpm-cell-array.h				\
	gpm-cell-array.c				\
	gpm-warnings.h					\
	gpm-warnings.c					\
	gpm-backlight.h					\
	gpm-backlight.c					\
	gpm-prefs-server.h				\
	gpm-prefs-server.c				\
	gpm-srv-brightness-kbd.h			\
	gpm-srv-brightness-kbd.c			\
	gpm-srv-screensaver.h				\
	gpm-srv-screensaver.c				\
	gpm-conf.h					\
	gpm-conf.c					\
	gpm-idle.h					\
	gpm-idle.c					\
	gpm-load.h					\
	gpm-load.c					\
	gpm-profile.h					\
	gpm-profile.c					\
	gpm-control.h					\
	gpm-control.c					\
	gpm-refcount.h					\
	gpm-refcount.c					\
	gpm-button.h					\
	gpm-button.c					\
	gpm-brightness.h				\
	gpm-brightness.c				\
	gpm-brightness-hal.h				\
	gpm-brightness-hal.c				\
	gpm-brightness-xrandr.h				\
	gpm-brightness-xrandr.c				\
	gpm-brightness-kbd.h				\
	gpm-brightness-kbd.c				\
	gpm-light-sensor.h				\
	gpm-light-sensor.c				\
	gpm-main.c					\
	gpm-manager.h					\
	gpm-manager.c					\
	gpm-ac-adapter.h				\
	gpm-ac-adapter.c				\
	gpm-tray-icon.h					\
	gpm-tray-icon.c					\
	gpm-marshal.h					\
	gpm-marshal.c					\
	gpm-common.h					\
	gpm-common.c					\
	gpm-screensaver.h				\
	gpm-screensaver.c				\
	gpm-sound.h					\
	gpm-sound.c					\
	gpm-networkmanager.h				\
	gpm-networkmanager.c				\
	gpm-stock-icons.h				\
	gpm-info.h					\
	gpm-info.c					\
	gpm-array.h					\
	gpm-array.c					\
	gpm-array-float.h				\
	gpm-array-float.c				\
	gpm-inhibit.h					\
	gpm-inhibit.c					\
	gpm-graph-widget.h				\
	gpm-graph-widget.c				\
	gpm-feedback-widget.h				\
	gpm-feedback-widget.c				\
	$(NULL)

gnome_power_manager_LDADD =				\
	$(GLIB_LIBS)					\
	$(GSTREAMER_LIBS)				\
	$(GNOME_LIBS)					\
	$(HAL_LIBS)					\
	$(POLKIT_LIBS)					\
	$(DBUS_LIBS)					\
	$(XRANDR_LIBS)					\
	$(GPM_EXTRA_LIBS)				\
	$(LOCAL_LIBHAL_LIBS)				\
	$(LOCAL_LIBDBUS_LIBS)				\
	$(NULL)

if HAVE_LIBNOTIFY
gnome_power_manager_LDADD +=				\
	$(LIBNOTIFY_LIBS)				\
	$(NULL)
endif

gnome_power_self_test_SOURCES =				\
	egg-color.c					\
	egg-color.h					\
	gpm-ac-adapter.h				\
	gpm-ac-adapter.c				\
	gpm-prefs-server.h				\
	gpm-prefs-server.c				\
	gpm-control.h					\
	gpm-control.c					\
	gpm-self-test.c					\
	gpm-self-test.h					\
	gpm-networkmanager.h				\
	gpm-networkmanager.c				\
	gpm-dpms.h					\
	gpm-dpms.c					\
	gpm-button.h					\
	gpm-button.c					\
	egg-debug.h					\
	egg-debug.c					\
	gpm-notify.h					\
	gpm-notify.c					\
	gpm-screensaver.h				\
	gpm-screensaver.c				\
	gpm-engine.h					\
	gpm-engine.c					\
	gpm-phone.h					\
	gpm-phone.c					\
	gpm-cell.h					\
	gpm-cell.c					\
	gpm-cell-unit.h					\
	gpm-cell-unit.c					\
	gpm-cell-array.h				\
	gpm-cell-array.c				\
	gpm-warnings.h					\
	gpm-warnings.c					\
	gpm-conf.h					\
	gpm-conf.c					\
	gpm-idle.h					\
	gpm-idle.c					\
	gpm-load.h					\
	gpm-load.c					\
	gpm-profile.h					\
	gpm-profile.c					\
	gpm-marshal.h					\
	gpm-marshal.c					\
	gpm-common.h					\
	gpm-common.c					\
	gpm-sound.h					\
	gpm-sound.c					\
	gpm-info.h					\
	gpm-info.c					\
	gpm-array.h					\
	gpm-array.c					\
	gpm-array-float.h				\
	gpm-array-float.c				\
	gpm-inhibit.h					\
	gpm-inhibit.c					\
	gpm-graph-widget.h				\
	gpm-graph-widget.c				\
	$(NULL)

gnome_power_self_test_LDADD =				\
	$(GLIB_LIBS)					\
	$(GNOME_LIBS)					\
	$(GSTREAMER_LIBS)				\
	$(POLKIT_GNOME_LIBS)				\
	$(DBUS_LIBS)					\
	$(LIBNOTIFY_LIBS)				\
	$(LOCAL_LIBDBUS_LIBS)				\
	$(LOCAL_LIBHAL_LIBS)				\
	$(GPM_EXTRA_LIBS)				\
	$(NULL)

gnome_power_self_test_CPPFLAGS=	\
	-DGPM_BUILD_TESTS

BUILT_SOURCES = 					\
	gpm-marshal.c					\
	gpm-marshal.h					\
	$(NULL)

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
	Makefile.in					\
	$(NULL)

