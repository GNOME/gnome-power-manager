INCLUDES = \
	$(GLIB_CFLAGS)						\
	$(GDK_CFLAGS)						\
	-DPACKAGE_DATA_DIR=\""$(datadir)"\"

noinst_LTLIBRARIES =						\
	libidletime.la

libidletime_la_SOURCES =					\
	libidletime.c						\
	libidletime.h
libidletime_la_LIBADD =						\
	$(INTLLIBS)						\
	$(GLIB_LIBS)						\
	$(GDK_LIBS)

clean-local:
	rm -f *~

CLEANFILES = $(BUILT_SOURCES)
