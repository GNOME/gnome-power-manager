INCLUDES = \
	$(GLIB_CFLAGS)						\
	$(DBUS_CFLAGS)						\
	-I$(top_srcdir)/libdevkit-power				\
	-DEGG_LOG_FILE=\""$(DKP_LOG_DIR)/DeviceKit-power"\"	\
	-DEGG_VERBOSE="\"DKP_VERBOSE\""				\
	-DEGG_LOGGING="\"DKP_LOGGING\""				\
	-DEGG_CONSOLE="\"DKP_CONSOLE\""				\
	-DPACKAGE_DATA_DIR=\""$(datadir)"\"			\
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\"

noinst_LTLIBRARIES =						\
	libdevkit-power.la

libdevkit_power_la_SOURCES =					\
	egg-debug.c						\
	egg-debug.h						\
	egg-string.c						\
	egg-string.h						\
	egg-obj-list.c						\
	egg-obj-list.h						\
	dkp-client.c						\
	dkp-client.h						\
	dkp-device.c					\
	dkp-device.h					\
	dkp-history-obj.h					\
	dkp-history-obj.c					\
	dkp-stats-obj.h						\
	dkp-stats-obj.c						\
	dkp-enum.c						\
	dkp-enum.h						\
	dkp-object.c						\
	dkp-object.h

libdevkit_power_la_LIBADD = $(INTLLIBS) $(GLIB_LIBS) $(DBUS_LIBS)


clean-local:
	rm -f *~

CLEANFILES = $(BUILT_SOURCES)

