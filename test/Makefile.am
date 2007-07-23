NULL =

INCLUDES =						\
	$(GLIB_CFLAGS)					\
	$(HAL_CFLAGS)					\
	$(DBUS_CFLAGS)					\
	$(GNOME_CFLAGS)					\
	$(GSTREAMER_CFLAGS)				\
	$(GTKUNIQUE_CFLAGS)				\
	-DBINDIR=\"$(bindir)\"			 	\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"	\
	-DDATADIR=\"$(datadir)\"			\
	-DPREFIX=\""$(prefix)"\" 			\
	-DSYSCONFDIR=\""$(sysconfdir)"\" 		\
	-DLIBDIR=\""$(libdir)"\" 			\
	-DVERSION="\"$(VERSION)\"" 			\
	-DGPM_DATA=\""$(datadir)/gnome-power-manager/"\"\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/libidletime			\
	-I$(top_srcdir)/libhal-glib			\
	-I$(top_srcdir)/libdbus-glib			\
	$(NULL)

noinst_PROGRAMS =					\
	gnome-power-inhibit-test			\
	$(NULL)

gnome_power_inhibit_test_SOURCES =			\
	gpm-inhibit-test.c				\
	$(NULL)

gnome_power_inhibit_test_LDADD =			\
	$(GLIB_LIBS)					\
	$(GNOME_LIBS)					\
	$(DBUS_LIBS)					\
	$(NULL)

clean-local:
	rm -f *~

CLEANFILES = $(BUILT_SOURCES)

MAINTAINERCLEANFILES =					\
	*~			      			\
	Makefile.in					\
	$(NULL)

