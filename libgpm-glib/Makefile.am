INCLUDES = \
	$(GLIB_CFLAGS)						\
	$(DBUS_CFLAGS)						\
	-I$(top_srcdir)/libdbus-glib				\
	-I$(top_srcdir)/libgpm-glib				\
	-DPACKAGE_DATA_DIR=\""$(datadir)"\"			\
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\"

LOCAL_LIBDBUS_LIBS =						\
	$(top_builddir)/libdbus-glib/libdbus-monitor-session.la	\
	$(top_builddir)/libdbus-glib/libdbus-monitor-system.la	\
	$(top_builddir)/libdbus-glib/libdbus-proxy.la

noinst_LTLIBRARIES =						\
	libgpm.la

libgpm_la_SOURCES =						\
	libgpm.c						\
	libgpm.h

libgpm_la_LIBADD=						\
	$(LOCAL_LIBDBUS_LIBS)					\
	$(DBUS_LIBS)						\
	$(GLIB_LIBS)						\
	$(INTLLIBS_LIBS)

clean-local:
	rm -f *~

CLEANFILES = $(BUILT_SOURCES)

