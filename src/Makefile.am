SUBDIRS = pixmaps sounds icons

pkgdata_DATA =						\
	preferences.glade

EXTRA_DIST =						\
	$(pkgdata_DATA)

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
	gnome-power-preferences

noinst_PROGRAMS = 					\
	gnome-power-dbus-test

gnome_power_dbus_test_SOURCES =				\
	hal-glib.h					\
	hal-glib.c					\
	gpm-dbus-common.h				\
	gpm-dbus-common.c				\
	gpm-dbus-test.c

gnome_power_dbus_test_LDADD =				\
	$(GLIB_LIBS)					\
	$(GNOME_LIBS)					\
	$(HAL_LIBS)					\
	$(DBUS_LIBS)

gnome_power_preferences_SOURCES =			\
	gpm-prefs.h					\
	gpm-prefs.c					\
	gpm-common.h					\
	gpm-common.c

if HAVE_LIBNOTIFY
gnome_power_preferences_LDADD =				\
	$(LIBNOTIFY_LIBS) $(GLIB_LIBS) $(GNOME_LIBS) $(HAL_LIBS)
else
gnome_power_preferences_LDADD =				\
	$(GLIB_LIBS) $(GNOME_LIBS) $(HAL_LIBS)
endif

gnome_power_manager_SOURCES =				\
	eggtrayicon.h					\
	eggtrayicon.c					\
	hal-glib.h					\
	hal-glib.c					\
	gpm-dbus-server.h				\
	gpm-dbus-server.c				\
	gpm-dbus-common.h				\
	gpm-dbus-common.c				\
	gpm-common.h					\
	gpm-common.c					\
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
