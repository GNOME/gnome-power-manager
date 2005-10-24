SUBDIRS = pixmaps icons

pkgdata_DATA =						\
	gpm-info.glade					\
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
	-DGPM_DATA=\""$(datadir)/gnome-power-manager/"\"

bin_PROGRAMS =						\
	gnome-power-manager				\
	gnome-power-preferences				\
	gnome-power-info

noinst_PROGRAMS = 					\
	gnome-power-dbus-test

gnome_power_dbus_test_SOURCES =				\
	compiler.h					\
	glibhal-main.h					\
	glibhal-main.c					\
	glibhal-extras.h				\
	glibhal-extras.c				\
	gpm-dbus-common.h				\
	gpm-dbus-common.c				\
	gpm-common.h					\
	gpm-common.c					\
	gpm-dbus-test.c

gnome_power_dbus_test_LDADD =				\
	$(GLIB_LIBS)					\
	$(GNOME_LIBS)					\
	$(HAL_LIBS)					\
	$(DBUS_LIBS)

gnome_power_info_SOURCES =				\
	compiler.h					\
	glibhal-main.h					\
	glibhal-main.c					\
	glibhal-extras.h				\
	glibhal-extras.c				\
	gpm-gtk-utils.h					\
	gpm-gtk-utils.c					\
	gpm-dbus-common.h				\
	gpm-dbus-common.c				\
	gpm-screensaver.h				\
	gpm-screensaver.c				\
	gpm-common.h					\
	gpm-common.c					\
	gpm-info.c

gnome_power_info_LDADD =				\
	$(GLIB_LIBS)					\
	$(GNOME_LIBS)					\
	$(HAL_LIBS)					\
	$(DBUS_LIBS)

gnome_power_preferences_SOURCES =			\
	compiler.h					\
	glibhal-main.h					\
	glibhal-main.c					\
	glibhal-extras.h				\
	glibhal-extras.c				\
	gpm-dbus-common.h				\
	gpm-dbus-common.c				\
	gpm-dbus-client.h				\
	gpm-dbus-client.c				\
	gpm-screensaver.h				\
	gpm-screensaver.c				\
	gpm-libnotify.h					\
	gpm-libnotify.c					\
	gpm-gtk-utils.h					\
	gpm-gtk-utils.c					\
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
	glibhal-extras.h				\
	glibhal-extras.c				\
	gpm-dbus-common.h				\
	gpm-dbus-common.c				\
	gpm-dbus-server.h				\
	gpm-dbus-server.c				\
	gpm-common.h					\
	gpm-common.c					\
	gpm-screensaver.h				\
	gpm-screensaver.c				\
	gpm-libnotify.h					\
	gpm-libnotify.c					\
	gpm-idle.h					\
	gpm-idle.c					\
	gpm-notification.h				\
	gpm-notification.c				\
	gpm-main.h					\
	gpm-main.c			

if HAVE_LIBNOTIFY
gnome_power_manager_LDADD =				\
	$(LIBNOTIFY_LIBS) $(GLIB_LIBS) $(GNOME_LIBS) $(HAL_LIBS) $(DBUS_LIBS)
else
gnome_power_manager_LDADD =				\
	$(GLIB_LIBS) $(GNOME_LIBS) $(HAL_LIBS) $(DBUS_LIBS)
endif

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
	gnome-power-glue.h				\
	Makefile

MAINTAINERCLEANFILES =					\
	Makefile.in
