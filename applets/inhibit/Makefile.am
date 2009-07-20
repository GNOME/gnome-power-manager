INCLUDES =							\
	-I. -I$(srcdir) 					\
	$(GLIB_CFLAGS)						\
	$(DBUS_CFLAGS)						\
	$(GNOME_CFLAGS)						\
	$(PANEL_CFLAGS)						\
	$(LIBNOTIFY_CFLAGS)					\
	$(GTKUNIQUE_CFLAGS)					\
	-DBINDIR=\"$(bindir)\"			 		\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"		\
	-DDATADIR=\"$(datadir)\"				\
	-DPREFIX=\""$(prefix)"\" 				\
	-DSYSCONFDIR=\""$(sysconfdir)"\" 			\
	-DLIBDIR=\""$(libdir)"\" 				\
	-DVERSION="\"$(VERSION)\"" 				\
	-DGPM_DATA=\"$(pkgdatadir)\"				\
	-DEGG_LOG_FILE=\""~/gnome-power-manager.log"\"		\
	-DEGG_VERBOSE="\"GPM_VERBOSE\""				\
	-DEGG_LOGGING="\"GPM_LOGGING\""				\
	-DEGG_CONSOLE="\"GPM_CONSOLE\""				\
	-I$(top_srcdir)/libdbus-glib				\
	$(NULL)

libexec_PROGRAMS=gnome-inhibit-applet

gnome_inhibit_applet_SOURCES =					\
	inhibit-applet.c					\
	egg-dbus-monitor.c					\
	egg-dbus-monitor.h					\
	egg-debug.c						\
	egg-debug.h						\
	gpm-common.c						\
	gpm-common.h

gnome_inhibit_applet_LDADD =					\
	$(DBUS_LIBS)						\
	$(GNOME_LIBS)						\
	$(PANEL_LIBS)

serverdir       = $(libdir)/bonobo/servers
server_in_files = GNOME_InhibitApplet.server.in
server_DATA     = $(server_in_files:.server.in=.server)

$(server_in_files): $(server_in_files:.server.in=.server.in.in)
	sed -e "s|\@LIBEXECDIR\@|$(libexecdir)|" -e "s|\@VERSION\@|$(VERSION)|" $< > $@

@INTLTOOL_SERVER_RULE@

uidir   = $(datadir)/gnome-2.0/ui
ui_DATA = GNOME_InhibitApplet.xml

EXTRA_DIST = GNOME_InhibitApplet.server.in.in $(ui_DATA)

DISTCLEANFILES =						\
	$(server_DATA) $(server_DATA).in
