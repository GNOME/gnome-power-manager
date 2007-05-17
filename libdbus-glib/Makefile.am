INCLUDES = \
	$(GLIB_CFLAGS)						\
	$(DBUS_CFLAGS)						\
	-I$(top_srcdir)/libdbus-glib				\
	-DPACKAGE_DATA_DIR=\""$(datadir)"\"			\
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\"

noinst_LTLIBRARIES =						\
	libdbus-proxy.la					\
	libdbus-watch.la					\
	libdbus-monitor-session.la				\
	libdbus-monitor-system.la

libdbus_watch_la_SOURCES =					\
	libdbus-marshal.h					\
	libdbus-marshal.c					\
	libdbus-monitor-session.c				\
	libdbus-monitor-session.h				\
	libdbus-monitor-system.c				\
	libdbus-monitor-system.h				\
	libdbus-watch.c						\
	libdbus-watch.h
libdbus_watch_la_LIBADD = @DBUS_LIBS@ $(INTLLIBS) $(GLIB_LIBS)

libdbus_proxy_la_SOURCES =					\
	libdbus-marshal.h					\
	libdbus-marshal.c					\
	libdbus-monitor-session.c				\
	libdbus-monitor-session.h				\
	libdbus-monitor-system.c				\
	libdbus-monitor-system.h				\
	libdbus-proxy.c						\
	libdbus-proxy.h
libdbus_proxy_la_LIBADD = @DBUS_LIBS@ $(INTLLIBS) $(GLIB_LIBS)

libdbus_monitor_system_la_SOURCES =				\
	libdbus-marshal.h					\
	libdbus-marshal.c					\
	libdbus-monitor-system.c				\
	libdbus-monitor-system.h
libdbus_monitor_system_la_LIBADD = @DBUS_LIBS@ $(INTLLIBS) $(GLIB_LIBS)

libdbus_monitor_session_la_SOURCES =				\
	libdbus-marshal.h					\
	libdbus-marshal.c					\
	libdbus-monitor-session.c				\
	libdbus-monitor-session.h
libdbus_monitor_session_la_LIBADD = @DBUS_LIBS@ $(INTLLIBS) $(GLIB_LIBS)

EXTRA_DIST =							\
	libdbus-marshal.list

BUILT_SOURCES = 						\
	libdbus-marshal.c					\
	libdbus-marshal.h

libdbus-marshal.c: libdbus-marshal.list
	echo "#include \"libdbus-marshal.h\"" > $@ && \
	@GLIB_GENMARSHAL@ $< --prefix=libdbus_marshal --body >> $@

libdbus-marshal.h: libdbus-marshal.list
	@GLIB_GENMARSHAL@ $< --prefix=libdbus_marshal --header > $@

clean-local:
	rm -f *~
	rm -f libdbus-marshal.c libdbus-marshal.h

CLEANFILES = $(BUILT_SOURCES)

