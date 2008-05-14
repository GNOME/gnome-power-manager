INCLUDES = \
	$(GLIB_CFLAGS)						\
	$(UNIQUE_CFLAGS)					\
	-DPACKAGE_DATA_DIR=\""$(datadir)"\"			\
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\"

noinst_LTLIBRARIES =						\
	libunique.la

libunique_la_SOURCES =						\
	libunique.c						\
	libunique.h
libunique_la_LIBADD = $(DBUS_LIBS) $(INTLLIBS) $(GLIB_LIBS)

if HAVE_UNIQUE
libunique_la_LIBADD +=						\
	$(UNIQUE_LIBS)
endif

clean-local:
	rm -f *~

