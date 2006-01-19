## We require new-style dependency handling.
AUTOMAKE_OPTIONS = 1.7

NULL =

EXTRA_DIST =						\
	gpm-manager.xml					\
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
	gnome-power-manager				\
	gnome-power-preferences				\
	$(NULL)

gnome_power_preferences_SOURCES =			\
	gpm-hal.h					\
	gpm-hal.c					\
	gpm-screensaver.h				\
	gpm-screensaver.c				\
	gpm-prefs.h					\
	gpm-prefs.c					\
	gpm-common.h					\
	gpm-common.c					\
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
	gpm-idle.h					\
	gpm-idle.c					\
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
	gpm-manager-glue.h				\
	gpm-marshal.c					\
	gpm-marshal.h					\
	$(NULL)

gpm-marshal.c: gpm-marshal.list
	echo "#include \"gpm-marshal.h\"" > $@ && \
	@GLIB_GENMARSHAL@ $< --prefix=gpm_marshal --body >> $@

gpm-marshal.h: gpm-marshal.list
	@GLIB_GENMARSHAL@ $< --prefix=gpm_marshal --header > $@

gpm-manager-glue.h: gpm-manager.xml
	libtool --mode=execute dbus-binding-tool --prefix=gpm_manager --mode=glib-server --output=gpm-manager-glue.h $(srcdir)/gpm-manager.xml

clean-local:
	rm -f *~
	rm -f gpm-marshal.c gpm-marshal.h

CLEANFILES = $(BUILT_SOURCES)

MAINTAINERCLEANFILES =					\
	*~                              		\
	Makefile.in					\
	$(NULL)

