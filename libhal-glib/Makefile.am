INCLUDES = \
	$(GLIB_CFLAGS)						\
	$(DBUS_CFLAGS)						\
	-I$(top_srcdir)/libdbus-glib				\
	-DPACKAGE_DATA_DIR=\""$(datadir)"\"			\
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\"

LOCAL_LIBDBUS_LIBS =						\
	$(top_builddir)/libdbus-glib/libdbus-monitor-session.la	\
	$(top_builddir)/libdbus-glib/libdbus-monitor-system.la	\
	$(top_builddir)/libdbus-glib/libdbus-proxy.la

noinst_LTLIBRARIES =						\
	libhal-gmanager.la					\
	libhal-gdevice.la					\
	libhal-gdevicestore.la					\
	libhal-gpower.la

libhal_gmanager_la_SOURCES =					\
	libhal-marshal.h					\
	libhal-marshal.c					\
	libhal-gdevice.c					\
	libhal-gdevice.h					\
	libhal-gmanager.c					\
	libhal-gmanager.h
libhal_gmanager_la_LIBADD =					\
	$(LOCAL_LIBDBUS_LIBS)					\
	$(DBUS_LIBS)						\
	$(INTLLIBS)						\
	$(GLIB_LIBS)

libhal_gdevice_la_SOURCES =					\
	libhal-marshal.h					\
	libhal-marshal.c					\
	libhal-gdevice.c					\
	libhal-gdevice.h
libhal_gdevice_la_LIBADD =					\
	$(LOCAL_LIBDBUS_LIBS)					\
	$(DBUS_LIBS)						\
	$(INTLLIBS)						\
	$(GLIB_LIBS)

libhal_gdevicestore_la_SOURCES =				\
	libhal-marshal.h					\
	libhal-marshal.c					\
	libhal-gdevice.c					\
	libhal-gdevice.h					\
	libhal-gdevicestore.c					\
	libhal-gdevicestore.h
libhal_gdevicestore_la_LIBADD =					\
	$(LOCAL_LIBDBUS_LIBS)					\
	$(DBUS_LIBS)						\
	$(INTLLIBS)						\
	$(GLIB_LIBS)

libhal_gpower_la_SOURCES =					\
	libhal-marshal.h					\
	libhal-marshal.c					\
	libhal-gpower.c						\
	libhal-gpower.h
libhal_gpower_la_LIBADD =					\
	$(LOCAL_LIBDBUS_LIBS)					\
	$(DBUS_LIBS)						\
	$(INTLLIBS)						\
	$(GLIB_LIBS)

EXTRA_DIST =							\
	libhal-marshal.list

BUILT_SOURCES = 						\
	libhal-marshal.c					\
	libhal-marshal.h

libhal-marshal.c: libhal-marshal.list
	echo "#include \"libhal-marshal.h\"" > $@ && \
	@GLIB_GENMARSHAL@ $< --prefix=libhal_marshal --body >> $@

libhal-marshal.h: libhal-marshal.list
	@GLIB_GENMARSHAL@ $< --prefix=libhal_marshal --header > $@

clean-local:
	rm -f *~
	rm -f libhal-marshal.c libhal-marshal.h

CLEANFILES = $(BUILT_SOURCES)
