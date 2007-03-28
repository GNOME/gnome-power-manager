INCLUDES = \
	$(GLIB_CFLAGS)						\
	$(DBUS_CFLAGS)						\
	-DPACKAGE_DATA_DIR=\""$(datadir)"\"			\
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\"

noinst_LTLIBRARIES =						\
	libgpm.la

libgpm_la_SOURCES =						\
	libgpm.c						\
	libgpm.h
libgpm_la_LIBADD = @DBUS_LIBS@ $(INTLLIBS) $(GLIB_LIBS)

clean-local:
	rm -f *~

CLEANFILES = $(BUILT_SOURCES)

