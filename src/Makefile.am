SUBDIRS = icons

pkgdata_DATA =						\
	gpm-prefs.glade

EXTRA_DIST =						\
	$(pkgdata_DATA)					\
	gnome-power-service.xml				\
	gpm_marshal.list

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
	gnome-power-preferences				\
	gnome-power-console

gnome_power_preferences_SOURCES =			\
	glibhal-main.h					\
	glibhal-main.c					\
	gpm-hal.h					\
	gpm-hal.c					\
	gpm-dbus-common.h				\
	gpm-dbus-common.c				\
	gpm-dbus-client.h				\
	gpm-dbus-client.c				\
	gpm-screensaver.h				\
	gpm-screensaver.c				\
	gpm-libnotify.h					\
	gpm-libnotify.c					\
	gpm-prefs.h					\
	gpm-prefs.c					\
	gpm-common.h					\
	gpm-common.c

if HAVE_LIBNOTIFY
gnome_power_preferences_LDADD =				\
	$(LIBNOTIFY_LIBS) $(GLIB_LIBS) $(GNOME_LIBS) $(HAL_LIBS) $(DBUS_LIBS)
else
gnome_power_preferences_LDADD =				\
	$(GLIB_LIBS) $(GNOME_LIBS) $(HAL_LIBS) $(DBUS_LIBS)
endif

gnome_power_manager_SOURCES =				\
	eggtrayicon.h					\
	eggtrayicon.c					\
	gpm_marshal.h					\
	gpm_marshal.c					\
	glibhal-main.h					\
	glibhal-main.c					\
	glibhal-callback.h				\
	glibhal-callback.c				\
	gpm-hal.h					\
	gpm-hal.c					\
	gpm-dbus-common.h				\
	gpm-dbus-common.c				\
	gpm-dbus-server.h				\
	gpm-dbus-server.c				\
	gpm-common.h					\
	gpm-common.c					\
	gpm-screensaver.h				\
	gpm-screensaver.c				\
	gpm-networkmanager.h				\
	gpm-networkmanager.c				\
	gpm-libnotify.h					\
	gpm-libnotify.c					\
	gpm-idle.h					\
	gpm-idle.c					\
	gpm-notification.h				\
	gpm-notification.c				\
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

gnome_power_console_SOURCES =				\
	gpm_marshal.h					\
	gpm_marshal.c					\
	glibhal-main.h					\
	glibhal-main.c					\
	glibhal-callback.h				\
	glibhal-callback.c				\
	gpm-hal.h					\
	gpm-hal.c					\
	gpm-dbus-common.h				\
	gpm-dbus-common.c				\
	gpm-dbus-server.h				\
	gpm-dbus-server.c				\
	gpm-common.h					\
	gpm-common.c					\
	gpm-core.h					\
	gpm-core.c					\
	gpm-console.c					\
	gpm-sysdev.h					\
	gpm-sysdev.c

gnome_power_console_LDADD =				\
	$(GLIB_LIBS) $(GNOME_LIBS) $(HAL_LIBS) $(DBUS_LIBS)

BUILT_SOURCES = \
	gnome-power-glue.h				\
	gpm_marshal.c					\
	gpm_marshal.h

gpm_marshal.h: gpm_marshal.list
	cd $(srcdir) \
	&& glib-genmarshal --prefix=gpm_marshal gpm_marshal.list --header >> xgen-gmh \
	&& (cmp -s xgen-gmh gpm_marshal.h || cp xgen-gmh gpm_marshal.h) \
	&& rm -f xgen-gmh xgen-gmh~

gpm_marshal.c: gpm_marshal.list
	cd $(srcdir) \
	&& glib-genmarshal --prefix=gpm_marshal gpm_marshal.list --body >> xgen-gmc \
	&& (cmp -s xgen-gmc gpm_marshal.c || cp xgen-gmc gpm_marshal.c) \
	&& rm -f xgen-gmc xgen-gmc~

gnome-power-glue.h: gnome-power-service.xml
	libtool --mode=execute dbus-binding-tool --prefix=gpm_object --mode=glib-server --output=gnome-power-glue.h $(srcdir)/gnome-power-service.xml

clean-local:
	rm -f *~
	rm -f gpm_marshal.c gpm_marshal.h

DISTCLEANFILES =					\
	gnome-power-glue.h
