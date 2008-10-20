INCLUDES = \
	$(GLIB_CFLAGS)						\
	$(DBUS_CFLAGS)						\
	-DEGG_LOG_FILE=\""$(PK_LOG_DIR)/gnome-power-manager"\"	\
	-DPACKAGE_DATA_DIR=\""$(datadir)"\"			\
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\"

noinst_LTLIBRARIES =						\
	libhal-gmanager.la					\
	libhal-gdevice.la					\
	libhal-gdevicestore.la					\
	libhal-gpower.la

libhal_gmanager_la_SOURCES =					\
	egg-dbus-proxy.h					\
	egg-dbus-proxy.c					\
	egg-dbus-monitor.h					\
	egg-dbus-monitor.c					\
	egg-debug.h						\
	egg-debug.c						\
	libhal-marshal.h					\
	libhal-marshal.c					\
	libhal-gdevice.c					\
	libhal-gdevice.h					\
	libhal-gmanager.c					\
	libhal-gmanager.h
libhal_gmanager_la_LIBADD =					\
	$(DBUS_LIBS)						\
	$(INTLLIBS)						\
	$(GLIB_LIBS)

libhal_gdevice_la_SOURCES =					\
	egg-dbus-proxy.h					\
	egg-dbus-proxy.c					\
	egg-dbus-monitor.h					\
	egg-dbus-monitor.c					\
	egg-debug.h						\
	egg-debug.c						\
	libhal-marshal.h					\
	libhal-marshal.c					\
	libhal-gdevice.c					\
	libhal-gdevice.h
libhal_gdevice_la_LIBADD =					\
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
	$(DBUS_LIBS)						\
	$(INTLLIBS)						\
	$(GLIB_LIBS)

libhal_gpower_la_SOURCES =					\
	egg-dbus-proxy.h					\
	egg-dbus-proxy.c					\
	egg-dbus-monitor.h					\
	egg-dbus-monitor.c					\
	egg-debug.h						\
	egg-debug.c						\
	libhal-marshal.h					\
	libhal-marshal.c					\
	libhal-gpower.c						\
	libhal-gpower.h
libhal_gpower_la_LIBADD =					\
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
