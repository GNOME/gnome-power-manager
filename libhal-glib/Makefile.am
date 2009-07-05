INCLUDES = \
	$(GLIB_CFLAGS)						\
	$(DBUS_CFLAGS)						\
	-DEGG_LOG_FILE=\""~/gnome-power-manager.log"\"		\
	-DEGG_VERBOSE="\"GPM_VERBOSE\""				\
	-DEGG_LOGGING="\"GPM_LOGGING\""				\
	-DEGG_CONSOLE="\"GPM_CONSOLE\""				\
	-DPACKAGE_DATA_DIR=\""$(datadir)"\"			\
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\"

noinst_LTLIBRARIES =						\
	libhal.la

libhal_la_SOURCES =						\
	egg-dbus-proxy.h					\
	egg-dbus-proxy.c					\
	egg-dbus-monitor.h					\
	egg-dbus-monitor.c					\
	egg-debug.h						\
	egg-debug.c						\
	hal-marshal.h						\
	hal-marshal.c						\
	hal-device.c						\
	hal-device.h						\
	hal-manager.c						\
	hal-manager.h

libhal_la_LIBADD =						\
	$(DBUS_LIBS)						\
	$(INTLLIBS)						\
	$(GLIB_LIBS)

EXTRA_DIST =							\
	hal-marshal.list

BUILT_SOURCES = 						\
	hal-marshal.c						\
	hal-marshal.h

hal-marshal.c: hal-marshal.list
	echo "#include \"hal-marshal.h\"" > $@ && \
	@GLIB_GENMARSHAL@ $< --prefix=hal_marshal --body >> $@

hal-marshal.h: hal-marshal.list
	@GLIB_GENMARSHAL@ $< --prefix=hal_marshal --header > $@

clean-local:
	rm -f *~
	rm -f hal-marshal.c hal-marshal.h

CLEANFILES = $(BUILT_SOURCES)

