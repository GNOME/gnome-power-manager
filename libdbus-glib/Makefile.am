INCLUDES = \
	$(GLIB_CFLAGS)						\
	$(DBUS_CFLAGS)						\
	-I$(top_srcdir)/libdbus-glib				\
	-DEGG_LOG_FILE=\""$(PK_LOG_DIR)/gnome-power-manager"\"	\
	-DPACKAGE_DATA_DIR=\""$(datadir)"\"			\
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\"

noinst_LTLIBRARIES =						\
	libdbus-proxy.la

libdbus_proxy_la_SOURCES =					\
	egg-debug.h						\
	egg-debug.c						\
	egg-dbus-monitor.h					\
	egg-dbus-monitor.c					\
	libdbus-proxy.c						\
	libdbus-proxy.h
libdbus_proxy_la_LIBADD = @DBUS_LIBS@ $(INTLLIBS) $(GLIB_LIBS)

clean-local:
	rm -f *~

CLEANFILES = $(BUILT_SOURCES)

