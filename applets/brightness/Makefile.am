INCLUDES =							\
	-I. -I$(srcdir) 					\
	$(GLIB_CFLAGS)						\
	$(HAL_CFLAGS)						\
	$(DBUS_CFLAGS)						\
	$(GNOME_CFLAGS)						\
	$(LIBNOTIFY_CFLAGS)					\
	$(GTKUNIQUE_CFLAGS)					\
	-DBINDIR=\"$(bindir)\"			 		\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"	 	\
	-DDATADIR=\"$(datadir)\"				\
	-DPREFIX=\""$(prefix)"\" 				\
	-DSYSCONFDIR=\""$(sysconfdir)"\" 			\
	-DLIBDIR=\""$(libdir)"\" 				\
	-DVERSION="\"$(VERSION)\"" 				\
	-DGPM_DATA=\"$(pkgdatadir)\"				\
	-I$(top_srcdir)/libdbus-glib				\
	$(NULL)

LOCAL_LIBDBUS_LIBS =						\
	$(top_builddir)/libdbus-glib/libdbus-monitor-session.la	\
	$(top_builddir)/libdbus-glib/libdbus-monitor-system.la	\
	$(top_builddir)/libdbus-glib/libdbus-watch.la

libexec_PROGRAMS=gnome-brightness-applet

gnome_brightness_applet_SOURCES = brightness-applet.c
gnome_brightness_applet_LDADD =					\
	$(LOCAL_LIBDBUS_LIBS)					\
	$(DBUS_LIBS)						\
	$(GNOME_LIBS)

serverdir       = $(libdir)/bonobo/servers
server_in_files = GNOME_BrightnessApplet.server.in
server_DATA     = $(server_in_files:.server.in=.server)

$(server_in_files): $(server_in_files:.server.in=.server.in.in)
	sed -e "s|\@LIBEXECDIR\@|$(libexecdir)|" -e "s|\@VERSION\@|$(VERSION)|" $< > $@

@INTLTOOL_SERVER_RULE@

uidir   = $(datadir)/gnome-2.0/ui
ui_DATA = GNOME_BrightnessApplet.xml

EXTRA_DIST = GNOME_BrightnessApplet.server.in.in $(ui_DATA)

DISTCLEANFILES =						\
	$(server_DATA) $(server_DATA).in
