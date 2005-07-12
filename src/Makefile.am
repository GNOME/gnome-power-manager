INCLUDES =						\
	$(GLIB_CFLAGS)					\
	$(HAL_CFLAGS)					\
	$(DBUS_CFLAGS)					\
	$(GNOME_CFLAGS)					\
	-DBINDIR=\"$(bindir)\"			   	\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"  	\
	-DDATADIR=\"$(datadir)\"			\
	-DGPM_DATA=\""$(datadir)/gnome-power/"\"


bin_PROGRAMS = gnome-power-manager gnome-power-preferences gnome-power-dbus-test

gnome_power_dbus_test_SOURCES = \
	gpm-dbus-common.h	\
	gpm-dbus-common.c	\
	gpm-dbus-test.c
gnome_power_dbus_test_LDADD = $(GLIB_LIBS) $(GNOME_LIBS) $(HAL_LIBS) $(DBUS_LIBS)

gnome_power_preferences_SOURCES = \
	gpm-prefs.h		\
	gpm-prefs.c		\
	gpm-common.h		\
	gpm-common.c
gnome_power_preferences_LDADD = $(GLIB_LIBS) $(GNOME_LIBS) $(HAL_LIBS)

gnome_power_manager_SOURCES = 	\
	eggtrayicon.h		\
	eggtrayicon.c		\
	gpm-dbus-server.h	\
	gpm-dbus-server.c	\
	gpm-dbus-common.h	\
	gpm-dbus-common.c	\
	gpm-common.h		\
	gpm-common.c		\
	gpm-notification.h	\
	gpm-notification.c	\
	gpm-main.h		\
	gpm-main.c			
gnome_power_manager_LDADD = $(GLIB_LIBS) $(HAL_LIBS) $(GNOME_LIBS) $(DBUS_LIBS)

EXTRA_DIST =			\
	$(Data_DATA)

Datadir = $(datadir)/gnome-power/

Data_DATA = 									\
	pixmaps/acpower-48.png						\
	pixmaps/battery-48.png						\
	pixmaps/emblem-ac.png						\
	pixmaps/emblem-bar.png						\
	pixmaps/brightness-000.png					\
	pixmaps/brightness-025.png					\
	pixmaps/brightness-050.png					\
	pixmaps/brightness-075.png					\
	pixmaps/brightness-100.png					\
	pixmaps/gnome-dev-acadapter.png				\
	pixmaps/gnome-dev-keyboard.png				\
	pixmaps/gnome-dev-mouse-optical.png			\
	pixmaps/gnome-dev-pda.png					\
	pixmaps/gnome-dev-battery.png				\
	pixmaps/gnome-dev-ups.png					\
	pixmaps/gnome-reboot.png					\
	pixmaps/gnome-shutdown.png					\
	pixmaps/gnome-suspend.png					\
	pixmaps/gnome-hibernate.png					\
	pixmaps/gnome-power-system-0-of-8.png		\
	pixmaps/gnome-power-system-1-of-8.png		\
	pixmaps/gnome-power-system-2-of-8.png		\
	pixmaps/gnome-power-system-3-of-8.png		\
	pixmaps/gnome-power-system-4-of-8.png		\
	pixmaps/gnome-power-system-5-of-8.png		\
	pixmaps/gnome-power-system-6-of-8.png		\
	pixmaps/gnome-power-system-7-of-8.png		\
	pixmaps/gnome-power-system-8-of-8.png		\
	pixmaps/gnome-power-system-ac-0-of-8.png	\
	pixmaps/gnome-power-system-ac-1-of-8.png	\
	pixmaps/gnome-power-system-ac-2-of-8.png	\
	pixmaps/gnome-power-system-ac-3-of-8.png	\
	pixmaps/gnome-power-system-ac-4-of-8.png	\
	pixmaps/gnome-power-system-ac-5-of-8.png	\
	pixmaps/gnome-power-system-ac-6-of-8.png	\
	pixmaps/gnome-power-system-ac-7-of-8.png	\
	pixmaps/gnome-power-system-ac-8-of-8.png
