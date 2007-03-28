INCLUDES =						\
	-I. -I$(srcdir) 				\
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
	-I$(top_srcdir)/libgpm-glib			\
	$(NULL)

LIBGPM_LIBS =						\
	$(top_builddir)/libgpm-glib/libgpm.la

bin_PROGRAMS=gnome-brightness-applet

SRCDEPS=../../src/gpm-proxy.c 		\
	../../src/gpm-debug.c 		\
	../../src/gpm-dbus-monitor.c 	\
	../../src/gpm-marshal.c

gnome_brightness_applet_SOURCES=brightness-applet.c brightness-applet.h $(SRCDEPS)
gnome_brightness_applet_LDADD=	\
	$(LIBGPM_LIBS)		\
	$(DBUS_LIBS)		\
	$(GNOME_LIBS)

serverdir       = $(libdir)/bonobo/servers
server_in_files = GNOME_BrightnessApplet.server.in
server_DATA     = $(server_in_files:.server.in=.server)

$(server_in_files): $(server_in_files:.server.in=.server.in.in)
	sed -e "s|\@BINDIR\@|$(bindir)|" $< > $@

@INTLTOOL_SERVER_RULE@

uidir   = $(datadir)/gnome-2.0/ui
ui_DATA = GNOME_BrightnessApplet.xml

EXTRA_DIST = GNOME_BrightnessApplet.server.in.in $(ui_DATA)

DISTCLEANFILES =						\
	$(server_DATA) $(server_DATA).in
