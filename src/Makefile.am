SUBDIRS = icons

EXTRA_DIST =						\
	gpm-manager.xml					\
	gpm-marshal.list

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
	-DGPM_DATA=\""$(datadir)/gnome-power-manager/"\"

bin_PROGRAMS =						\
	gnome-power-manager				\
	gnome-power-preferences
#	gnome-power-console

gnome_power_preferences_SOURCES =			\
	gpm-hal.h					\
	gpm-hal.c					\
	gpm-dbus-common.h				\
	gpm-dbus-common.c				\
	gpm-dbus-client.h				\
	gpm-dbus-client.c				\
	gpm-screensaver.h				\
	gpm-screensaver.c				\
	gpm-prefs.h					\
	gpm-prefs.c					\
	gpm-common.h					\
	gpm-common.c

gnome_power_preferences_LDADD =				\
	$(GLIB_LIBS) $(GNOME_LIBS) $(HAL_LIBS) $(DBUS_LIBS)

gnome_power_manager_SOURCES =				\
	gpm-dpms.h					\
	gpm-dpms-x11.c					\
	gpm-hal-monitor.h				\
	gpm-hal-monitor.c				\
	gpm-idle.h					\
	gpm-idle.c					\
	gpm-manager.h					\
	gpm-manager.c					\
	gpm-tray-icon.h					\
	gpm-tray-icon.c					\
	eggtrayicon.h					\
	eggtrayicon.c					\
	gpm-marshal.h					\
	gpm-marshal.c					\
	gpm-hal.h					\
	gpm-hal.c					\
	gpm-hal-callback.h				\
	gpm-hal-callback.c				\
	gpm-dbus-common.h				\
	gpm-dbus-common.c				\
	gpm-dbus-signal-handler.h			\
	gpm-dbus-signal-handler.c			\
	gpm-common.h					\
	gpm-common.c					\
	gpm-screensaver.h				\
	gpm-screensaver.c				\
	gpm-networkmanager.h				\
	gpm-networkmanager.c				\
	gpm-core.h					\
	gpm-core.c					\
	gpm-main.h					\
	gpm-main.c					\
	gpm-sysdev.h					\
	gpm-sysdev.c					\
	gpm-stock-icons.h				\
	gpm-stock-icons.c

if HAVE_LIBNOTIFY
gnome_power_manager_LDADD =				\
	$(LIBNOTIFY_LIBS) $(GLIB_LIBS) $(GNOME_LIBS) $(HAL_LIBS) $(DBUS_LIBS)
else
gnome_power_manager_LDADD =				\
	$(GLIB_LIBS) $(GNOME_LIBS) $(HAL_LIBS) $(DBUS_LIBS)
endif

#gnome_power_console_SOURCES =				\
#	gpm-marshal.h					\
#	gpm-marshal.c					\
#	gpm-hal-callback.h				\
#	gpm-hal-callback.c				\
#	gpm-hal.h					\
#	gpm-hal.c					\
#	gpm-dbus-common.h				\
#	gpm-dbus-common.c				\
#	gpm-dbus-signal-handler.h			\
#	gpm-dbus-signal-handler.c			\
#	gpm-common.h					\
#	gpm-common.c					\
#	gpm-core.h					\
#	gpm-core.c					\
#	gpm-console.c					\
#	gpm-sysdev.h					\
#	gpm-sysdev.c
#
#gnome_power_console_LDADD =				\
#	$(GLIB_LIBS) $(GNOME_LIBS) $(HAL_LIBS) $(DBUS_LIBS)

BUILT_SOURCES = \
	gpm-manager-glue.h				\
	gpm-marshal.c					\
	gpm-marshal.h

#gpm-marshal.h: gpm-marshal.list
#	cd $(srcdir) \
#	&& glib-genmarshal --prefix=gpm_marshal gpm-marshal.list --header >> xgen-gmh \
#	&& (cmp -s xgen-gmh gpm-marshal.h || cp xgen-gmh gpm-marshal.h) \
#	&& rm -f xgen-gmh xgen-gmh~
#
#gpm-marshal.c: gpm-marshal.list
#	cd $(srcdir) \
#	&& glib-genmarshal --prefix=gpm_marshal gpm-marshal.list --body >> xgen-gmc \
#	&& (cmp -s xgen-gmc gpm-marshal.c || cp xgen-gmc gpm-marshal.c) \
#	&& rm -f xgen-gmc xgen-gmc~

gpm-marshal.h: gpm-marshal.list
	glib-genmarshal --prefix=gpm_marshal $(srcdir)/gpm-marshal.list --header > $(srcdir)/gpm-marshal.h

gpm-marshal.c: gpm-marshal.list
	glib-genmarshal --prefix=gpm_marshal $(srcdir)/gpm-marshal.list --body > $(srcdir)/gpm-marshal.c

gpm-manager-glue.h: gpm-manager.xml
	libtool --mode=execute dbus-binding-tool --prefix=gpm_manager --mode=glib-server --output=gpm-manager-glue.h $(srcdir)/gpm-manager.xml

clean-local:
	rm -f *~
	rm -f gpm-marshal.c gpm-marshal.h

DISTCLEANFILES =					\
	gpm-manager-glue.h
