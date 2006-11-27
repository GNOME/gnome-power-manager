INCLUDES=-I. -I$(srcdir) 					\
	$(GLIB_CFLAGS)	 					\
	$(DBUS_CFLAGS)	 					\
	$(GNOME_CFLAGS)  					\
	-DDATADIR=\""$(prefix)/$(DATADIRNAME)"\"		\
	-DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\"

bin_PROGRAMS=gnome-inhibit-applet

SRCDEPS=../../src/gpm-powermanager.c 	\
	../../src/gpm-proxy.c 		\
	../../src/gpm-debug.c 		\
	../../src/gpm-dbus-monitor.c 	\
	../../src/gpm-marshal.c

gnome_inhibit_applet_SOURCES=inhibit-applet.c inhibit-applet.h $(SRCDEPS)
gnome_inhibit_applet_LDADD=$(GNOME_LIBS)

serverdir       = $(libdir)/bonobo/servers
server_in_files = GNOME_InhibitApplet.server.in
server_DATA     = $(server_in_files:.server.in=.server)

$(server_in_files): $(server_in_files:.server.in=.server.in.in)
	sed -e "s|\@BINDIR\@|$(bindir)|" $< > $@

@INTLTOOL_SERVER_RULE@

uidir   = $(datadir)/gnome-2.0/ui
ui_DATA = GNOME_InhibitApplet.xml

EXTRA_DIST = GNOME_InhibitApplet.server.in.in $(ui_DATA)

DISTCLEANFILES =			\		
	$(server_DATA) $(server_DATA).in
