## We require new-style dependency handling.
AUTOMAKE_OPTIONS = 1.7

NULL =

EXTRA_DIST =						\
	gpm-interface-manager.xml			\
	gpm-interface-statistics.xml			\
	gpm-marshal.list				\
	$(NULL)

INCLUDES =						\
	$(GLIB_CFLAGS)					\
	$(HAL_CFLAGS)					\
	$(DBUS_CFLAGS)					\
	$(GNOME_CFLAGS)					\
	$(LIBNOTIFY_CFLAGS)				\
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
	gnome-power-statistics				\
	gnome-power-manager				\
	gnome-power-preferences				\
	gnome-power-inhibit-test			\
	$(NULL)

gnome_power_statistics_SOURCES =			\
	gpm-graph.c					\
	gpm-graph-core.h				\
	gpm-graph-core.c				\
	gpm-graph-widget.h				\
	gpm-graph-widget.c				\
	gpm-marshal.h					\
	gpm-marshal.c					\
	gpm-dbus-monitor.h				\
	gpm-dbus-monitor.c				\
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
	gpm-debug.h					\
	gpm-debug.c					\
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
	gpm-hal-cpufreq.h				\
	gpm-hal-cpufreq.c				\
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

gnome_power_manager_SOURCES =				\
	gpm-dpms.h					\
	gpm-dpms-x11.c					\
	gpm-hal-monitor.h				\
	gpm-hal-monitor.c				\
	gpm-dbus-monitor.h				\
	gpm-dbus-monitor.c				\
	gpm-debug.h					\
	gpm-debug.c					\
	gpm-idle.h					\
	gpm-idle.c					\
	gpm-proxy.h					\
	gpm-proxy.c					\
	gpm-polkit.h					\
	gpm-polkit.c					\
	gpm-hal-cpufreq.h				\
	gpm-hal-cpufreq.c				\
	gpm-hal-power.h					\
	gpm-hal-power.c					\
	gpm-hal-brightness.h				\
	gpm-hal-brightness.c				\
	gpm-main.c					\
	gpm-manager.h					\
	gpm-manager.c					\
	gpm-power.h					\
	gpm-power.c					\
	gpm-tray-icon.h					\
	gpm-tray-icon.c					\
	eggtrayicon.h					\
	eggtrayicon.c					\
	gpm-marshal.h					\
	gpm-marshal.c					\
	gpm-hal.h					\
	gpm-hal.c					\
	gpm-common.h					\
	gpm-common.c					\
	gpm-screensaver.h				\
	gpm-screensaver.c				\
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
	$(NULL)

if HAVE_LIBNOTIFY
gnome_power_manager_LDADD +=				\
	$(LIBNOTIFY_LIBS)				\
	$(NULL)
endif

BUILT_SOURCES = 					\
	gpm-interface-manager.h			\
	gpm-interface-statistics.h			\
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

clean-local:
	rm -f *~
	rm -f gpm-marshal.c gpm-marshal.h

CLEANFILES = $(BUILT_SOURCES)

MAINTAINERCLEANFILES =					\
	*~			      			\
	Makefile.in					\
	$(NULL)

