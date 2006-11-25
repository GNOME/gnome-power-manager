## We require new-style dependency handling.
AUTOMAKE_OPTIONS = 1.7

NULL =

EXTRA_DIST =						\
	gpm-interface-manager.xml			\
	gpm-interface-statistics.xml			\
	gpm-interface-brightness-lcd.xml		\
	gpm-marshal.list				\
	$(NULL)

INCLUDES =						\
	$(GLIB_CFLAGS)					\
	$(HAL_CFLAGS)					\
	$(DBUS_CFLAGS)					\
	$(GNOME_CFLAGS)					\
	$(LIBNOTIFY_CFLAGS)				\
	$(GTKUNIQUE_CFLAGS)				\
	-DBINDIR=\"$(bindir)\"			 	\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"	\
	-DDATADIR=\"$(datadir)\"			\
	-DPREFIX=\""$(prefix)"\" 			\
	-DSYSCONFDIR=\""$(sysconfdir)"\" 		\
	-DLIBDIR=\""$(libdir)"\" 			\
	-DVERSION="\"$(VERSION)\"" 			\
	-DGPM_DATA=\""$(datadir)/gnome-power-manager/"\"\
	$(NULL)

bin_PROGRAMS =						\
	gnome-power-manager				\
	gnome-power-preferences				\
	gnome-power-statistics				\
	$(NULL)

noinst_PROGRAMS =					\
	gnome-power-inhibit-test			\
	$(NULL)

gnome_power_statistics_SOURCES =			\
	gpm-statistics.c				\
	gpm-statistics-core.h				\
	gpm-statistics-core.c				\
	gpm-info-data.h					\
	gpm-info-data.c					\
	gpm-graph-widget.h				\
	gpm-graph-widget.c				\
	gpm-marshal.h					\
	gpm-marshal.c					\
	gpm-dbus-monitor.h				\
	gpm-dbus-monitor.c				\
	gpm-conf.h					\
	gpm-conf.c					\
	gpm-proxy.h					\
	gpm-proxy.c					\
	gpm-debug.h					\
	gpm-debug.c					\
	$(NULL)

gnome_power_statistics_LDADD =				\
	$(GLIB_LIBS)					\
	$(GNOME_LIBS)					\
	$(DBUS_LIBS)					\
	$(NULL)

if HAVE_GTKUNIQUE
gnome_power_statistics_LDADD +=				\
	$(GTKUNIQUE_LIBS)
endif

gnome_power_inhibit_test_SOURCES =			\
	gpm-inhibit-test.c				\
	$(NULL)

gnome_power_inhibit_test_LDADD =			\
	$(GLIB_LIBS)					\
	$(GNOME_LIBS)					\
	$(DBUS_LIBS)					\
	$(NULL)

gnome_power_preferences_SOURCES =			\
	gpm-hal.h					\
	gpm-hal.c					\
	gpm-dpms.h					\
	gpm-dpms.c					\
	gpm-conf.h					\
	gpm-conf.c					\
	gpm-debug.h					\
	gpm-debug.c					\
	gpm-idle.h					\
	gpm-idle.c					\
	gpm-prefs.h					\
	gpm-prefs.c					\
	gpm-prefs-core.h				\
	gpm-prefs-core.c				\
	gpm-marshal.h					\
	gpm-marshal.c					\
	gpm-dbus-monitor.h				\
	gpm-dbus-monitor.c				\
	gpm-proxy.h					\
	gpm-proxy.c					\
	gpm-cpufreq.h					\
	gpm-cpufreq.c					\
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
	$(NULL)

if HAVE_GTKUNIQUE
gnome_power_preferences_LDADD +=			\
	$(GTKUNIQUE_LIBS)
endif

gnome_power_manager_SOURCES =				\
	gpm-dpms.h					\
	gpm-dpms.c					\
	gpm-battery.h					\
	gpm-battery.c					\
	gpm-dbus-monitor.h				\
	gpm-dbus-monitor.c				\
	gpm-debug.h					\
	gpm-debug.c					\
	gpm-notify.h					\
	gpm-notify.c					\
	gpm-warning.h					\
	gpm-warning.c					\
	gpm-srv-brightness-lcd.h			\
	gpm-srv-brightness-lcd.c			\
	gpm-srv-brightness-kbd.h			\
	gpm-srv-brightness-kbd.c			\
	gpm-srv-dpms.h					\
	gpm-srv-dpms.c					\
	gpm-srv-screensaver.h				\
	gpm-srv-screensaver.c				\
	gpm-srv-cpufreq.h				\
	gpm-srv-cpufreq.c				\
	gpm-conf.h					\
	gpm-conf.c					\
	gpm-idle.h					\
	gpm-idle.c					\
	gpm-policy.h					\
	gpm-policy.c					\
	gpm-proxy.h					\
	gpm-proxy.c					\
	gpm-refcount.h					\
	gpm-refcount.c					\
	gpm-button.h					\
	gpm-button.c					\
	gpm-polkit.h					\
	gpm-polkit.c					\
	gpm-cpufreq.h					\
	gpm-cpufreq.c					\
	gpm-brightness-lcd.h				\
	gpm-brightness-lcd.c				\
	gpm-brightness-kbd.h				\
	gpm-brightness-kbd.c				\
	gpm-light-sensor.h				\
	gpm-light-sensor.c				\
	gpm-main.c					\
	gpm-manager.h					\
	gpm-manager.c					\
	gpm-ac-adapter.h				\
	gpm-ac-adapter.c				\
	gpm-power.h					\
	gpm-power.c					\
	gpm-tray-icon.h					\
	gpm-tray-icon.c					\
	gpm-marshal.h					\
	gpm-marshal.c					\
	gpm-hal.h					\
	gpm-hal.c					\
	gpm-common.h					\
	gpm-common.c					\
	gpm-screensaver.h				\
	gpm-screensaver.c				\
	gpm-sound.h					\
	gpm-sound.c					\
	gpm-networkmanager.h				\
	gpm-networkmanager.c				\
	gpm-stock-icons.h				\
	gpm-stock-icons.c				\
	gpm-info.h					\
	gpm-info.c					\
	gpm-info-data.h					\
	gpm-info-data.c					\
	gpm-inhibit.h					\
	gpm-inhibit.c					\
	gpm-graph-widget.h				\
	gpm-graph-widget.c				\
	gpm-feedback-widget.h				\
	gpm-feedback-widget.c				\
	$(NULL)

gnome_power_manager_LDADD =				\
	$(GLIB_LIBS)					\
	$(GNOME_LIBS)					\
	$(HAL_LIBS)					\
	$(DBUS_LIBS)					\
	$(GPM_EXTRA_LIBS)				\
	$(NULL)

if HAVE_LIBNOTIFY
gnome_power_manager_LDADD +=				\
	$(LIBNOTIFY_LIBS)				\
	$(NULL)
endif

BUILT_SOURCES = 					\
	gpm-interface-manager.h				\
	gpm-interface-statistics.h			\
	gpm-interface-brightness-lcd.h			\
	gpm-marshal.c					\
	gpm-marshal.h					\
	$(NULL)

gpm-marshal.c: gpm-marshal.list
	echo "#include \"gpm-marshal.h\"" > $@ && \
	@GLIB_GENMARSHAL@ $< --prefix=gpm_marshal --body >> $@

gpm-marshal.h: gpm-marshal.list
	@GLIB_GENMARSHAL@ $< --prefix=gpm_marshal --header > $@

gpm-interface-manager.h: gpm-interface-manager.xml
	libtool --mode=execute dbus-binding-tool --prefix=gpm_manager --mode=glib-server --output=gpm-interface-manager.h $(srcdir)/gpm-interface-manager.xml

gpm-interface-statistics.h: gpm-interface-statistics.xml
	libtool --mode=execute dbus-binding-tool --prefix=gpm_statistics --mode=glib-server --output=gpm-interface-statistics.h $(srcdir)/gpm-interface-statistics.xml

gpm-interface-brightness-lcd.h: gpm-interface-brightness-lcd.xml
	libtool --mode=execute dbus-binding-tool --prefix=gpm_brightness_lcd --mode=glib-server --output=gpm-interface-brightness-lcd.h $(srcdir)/gpm-interface-brightness-lcd.xml

clean-local:
	rm -f *~
	rm -f gpm-marshal.c gpm-marshal.h

CLEANFILES = $(BUILT_SOURCES)

MAINTAINERCLEANFILES =					\
	*~			      			\
	Makefile.in					\
	$(NULL)

