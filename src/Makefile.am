## We require new-style dependency handling.
AUTOMAKE_OPTIONS = 1.7

NULL =

EXTRA_DIST =						\
	org.freedesktop.PowerManagement.xml		\
	org.freedesktop.PowerManagement.Inhibit.xml	\
	org.freedesktop.PowerManagement.Backlight.xml	\
	org.freedesktop.PowerManagement.Statistics.xml	\
	gpm-marshal.list				\
	$(NULL)

INCLUDES =						\
	$(GLIB_CFLAGS)					\
	$(HAL_CFLAGS)					\
	$(DBUS_CFLAGS)					\
	$(GNOME_CFLAGS)					\
	$(UNIQUE_CFLAGS)				\
	$(XRANDR_CFLAGS)				\
	$(LIBNOTIFY_CFLAGS)				\
	$(CANBERRA_CFLAGS)				\
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
	-DEGG_LOG_FILE=\""~/gnome-power-manager.log"\"	\
	-DEGG_VERBOSE="\"GPM_VERBOSE\""			\
	-DEGG_LOGGING="\"GPM_LOGGING\""			\
	-DEGG_CONSOLE="\"GPM_CONSOLE\""			\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/libdevkit-power			\
	-I$(top_srcdir)/libhal-glib			\
	$(NULL)

LOCAL_LIBHAL_LIBS =					\
	$(top_builddir)/libhal-glib/libhal.la

DEVKIT_POWER_LIBS =					\
	$(top_builddir)/libdevkit-power/libdevkit-power.la

bin_PROGRAMS =						\
	gnome-power-manager				\
	gnome-power-preferences				\
	$(NULL)

if HAVE_DK_POWER
bin_PROGRAMS +=						\
	gnome-power-statistics				\
	$(NULL)
endif

check_PROGRAMS =					\
	gnome-power-self-test

if HAVE_DK_POWER
gnome_power_statistics_SOURCES =			\
	egg-color.c					\
	egg-color.h					\
	egg-debug.h					\
	egg-debug.c					\
	egg-unique.h					\
	egg-unique.c					\
	egg-precision.h					\
	egg-precision.c					\
	egg-obj-list.c					\
	egg-obj-list.h					\
	egg-array-float.c				\
	egg-array-float.h				\
	gpm-common.h					\
	gpm-common.c					\
	gpm-statistics.c				\
	gpm-devicekit.c					\
	gpm-devicekit.h					\
	gpm-point-obj.c					\
	gpm-point-obj.h					\
	gpm-graph-widget.h				\
	gpm-graph-widget.c				\
	$(NULL)

gnome_power_statistics_LDADD =				\
	$(GLIB_LIBS)					\
	$(DEVKIT_POWER_LIBS)				\
	$(GNOME_LIBS)					\
	$(DBUS_LIBS)					\
	$(NULL)

if HAVE_UNIQUE
gnome_power_statistics_LDADD +=				\
	$(UNIQUE_LIBS)
	$(NULL)
endif

endif

gnome_power_preferences_SOURCES =			\
	egg-debug.h					\
	egg-debug.c					\
	egg-unique.h					\
	egg-unique.c					\
	egg-dbus-proxy.h				\
	egg-dbus-proxy.c				\
	egg-dbus-monitor.h				\
	egg-dbus-monitor.c				\
	gpm-dpms.h					\
	gpm-dpms.c					\
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
	$(NULL)

if HAVE_UNIQUE
gnome_power_preferences_LDADD +=			\
	$(UNIQUE_LIBS)
endif

gnome_power_manager_SOURCES =				\
	egg-debug.h					\
	egg-debug.c					\
	egg-dbus-monitor.h				\
	egg-dbus-monitor.c				\
	egg-dbus-proxy.h				\
	egg-dbus-proxy.c				\
	egg-precision.h					\
	egg-precision.c					\
	egg-discrete.h					\
	egg-discrete.c					\
	egg-console-kit.h				\
	egg-console-kit.c				\
	gpm-dpms.h					\
	gpm-dpms.c					\
	gpm-notify.h					\
	gpm-notify.c					\
	gpm-phone.h					\
	gpm-phone.c					\
	gpm-backlight.h					\
	gpm-backlight.c					\
	gpm-prefs-server.h				\
	gpm-prefs-server.c				\
	gpm-idle.h					\
	gpm-idle.c					\
	gpm-load.h					\
	gpm-load.c					\
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
	gpm-networkmanager.h				\
	gpm-networkmanager.c				\
	gpm-stock-icons.h				\
	gpm-inhibit.h					\
	gpm-inhibit.c					\
	gpm-feedback-widget.h				\
	gpm-feedback-widget.c				\
	$(NULL)

if HAVE_DK_POWER
gnome_power_manager_SOURCES +=				\
	gpm-engine.h					\
	gpm-engine.c					\
	gpm-devicekit.h					\
	gpm-devicekit.c					\
	$(NULL)
else
	egg-color.c					\
	egg-color.h					\
	gpm-profile.h					\
	gpm-profile.c					\
	gpm-array.h					\
	gpm-array.c					\
	gpm-array-float.h				\
	gpm-array-float.c				\
	gpm-engine-old.h				\
	gpm-engine-old.c				\
	gpm-cell.h					\
	gpm-cell.c					\
	gpm-cell-unit.h					\
	gpm-cell-unit.c					\
	gpm-cell-array.h				\
	gpm-cell-array.c				\
	gpm-warnings.h					\
	gpm-warnings.c					\
	$(NULL)
endif

gnome_power_manager_LDADD =				\
	$(GLIB_LIBS)					\
	$(GSTREAMER_LIBS)				\
	$(GNOME_LIBS)					\
	$(HAL_LIBS)					\
	$(POLKIT_LIBS)					\
	$(DBUS_LIBS)					\
	$(XRANDR_LIBS)					\
	$(CANBERRA_LIBS)				\
	$(GPM_EXTRA_LIBS)				\
	$(LOCAL_LIBHAL_LIBS)				\
	$(NULL)

if HAVE_LIBNOTIFY
gnome_power_manager_LDADD +=				\
	$(LIBNOTIFY_LIBS)				\
	$(NULL)
endif

if HAVE_DK_POWER
gnome_power_manager_LDADD +=				\
	$(DEVKIT_POWER_LIBS)
endif

gnome_power_self_test_SOURCES =				\
	egg-color.h					\
	egg-color.c					\
	egg-test.h					\
	egg-test.c					\
	egg-debug.h					\
	egg-debug.c					\
	egg-dbus-monitor.h				\
	egg-dbus-monitor.c				\
	egg-dbus-proxy.h				\
	egg-dbus-proxy.c				\
	egg-precision.h					\
	egg-precision.c					\
	egg-discrete.h					\
	egg-discrete.c					\
	gpm-ac-adapter.h				\
	gpm-ac-adapter.c				\
	gpm-prefs-server.h				\
	gpm-prefs-server.c				\
	gpm-control.h					\
	gpm-control.c					\
	gpm-self-test.c					\
	gpm-networkmanager.h				\
	gpm-networkmanager.c				\
	gpm-dpms.h					\
	gpm-dpms.c					\
	gpm-button.h					\
	gpm-button.c					\
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
	gpm-info.h					\
	gpm-info.c					\
	gpm-array.h					\
	gpm-array.c					\
	gpm-array-float.h				\
	gpm-array-float.c				\
	gpm-inhibit.h					\
	gpm-inhibit.c					\
	gpm-graph-widget-old.h				\
	gpm-graph-widget-old.c				\
	$(NULL)

gnome_power_self_test_LDADD =				\
	$(GLIB_LIBS)					\
	$(GNOME_LIBS)					\
	$(GSTREAMER_LIBS)				\
	$(POLKIT_GNOME_LIBS)				\
	$(DBUS_LIBS)					\
	$(LIBNOTIFY_LIBS)				\
	$(LOCAL_LIBHAL_LIBS)				\
	$(GPM_EXTRA_LIBS)				\
	$(NULL)

gnome_power_self_test_CFLAGS = -DEGG_TEST $(AM_CFLAGS)

BUILT_SOURCES = 					\
	org.freedesktop.PowerManagement.h		\
	org.freedesktop.PowerManagement.Inhibit.h	\
	org.freedesktop.PowerManagement.Backlight.h	\
	gpm-marshal.c					\
	gpm-marshal.h					\
	$(NULL)

if !HAVE_DK_POWER
BUILT_SOURCES += org.freedesktop.PowerManagement.Statistics.h
endif

gpm-marshal.c: gpm-marshal.list
	echo "#include \"gpm-marshal.h\"" > $@ && \
	@GLIB_GENMARSHAL@ $< --prefix=gpm_marshal --body >> $@

gpm-marshal.h: gpm-marshal.list
	@GLIB_GENMARSHAL@ $< --prefix=gpm_marshal --header > $@

org.freedesktop.PowerManagement.h: org.freedesktop.PowerManagement.xml
	libtool --mode=execute dbus-binding-tool	\
		--prefix=gpm_manager			\
		--mode=glib-server			\
		--output=org.freedesktop.PowerManagement.h		\
		$(srcdir)/org.freedesktop.PowerManagement.xml

org.freedesktop.PowerManagement.Inhibit.h: org.freedesktop.PowerManagement.Inhibit.xml
	libtool --mode=execute dbus-binding-tool	\
		--prefix=gpm_inhibit			\
		--mode=glib-server			\
		--output=org.freedesktop.PowerManagement.Inhibit.h	\
		$(srcdir)/org.freedesktop.PowerManagement.Inhibit.xml

if !HAVE_DK_POWER
org.freedesktop.PowerManagement.Statistics.h: org.freedesktop.PowerManagement.Statistics.xml
	libtool --mode=execute dbus-binding-tool	\
		--prefix=gpm_statistics			\
		--mode=glib-server			\
		--output=org.freedesktop.PowerManagement.Statistics.h	\
		$(srcdir)/org.freedesktop.PowerManagement.Statistics.xml
endif

org.freedesktop.PowerManagement.Backlight.h: org.freedesktop.PowerManagement.Backlight.xml
	libtool --mode=execute dbus-binding-tool	\
		--prefix=gpm_backlight			\
		--mode=glib-server			\
		--output=org.freedesktop.PowerManagement.Backlight.h	\
		$(srcdir)/org.freedesktop.PowerManagement.Backlight.xml

clean-local:
	rm -f *~
	rm -f gpm-marshal.c gpm-marshal.h

CLEANFILES = $(BUILT_SOURCES)

TESTS = gnome-power-self-test

MAINTAINERCLEANFILES =					\
	*~			      			\
	Makefile.in					\
	$(NULL)

