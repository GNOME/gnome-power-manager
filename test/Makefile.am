NULL =

INCLUDES =						\
	$(GLIB_CFLAGS)					\
	$(HAL_CFLAGS)					\
	$(DBUS_CFLAGS)					\
	$(GNOME_CFLAGS)					\
	$(GTKUNIQUE_CFLAGS)				\
	-DBINDIR=\"$(bindir)\"			 	\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"	\
	-DDATADIR=\"$(datadir)\"			\
	-DPREFIX=\""$(prefix)"\" 			\
	-DSYSCONFDIR=\""$(sysconfdir)"\" 		\
	-DLIBDIR=\""$(libdir)"\" 			\
	-DVERSION="\"$(VERSION)\"" 			\
	-DGPM_DATA=\""$(datadir)/gnome-power-manager/"\"\
	-I$(top_srcdir) -I$(top_srcdir)/libhal-glib	\
	$(NULL)

LIBHAL_LIBS =						\
	$(top_builddir)/libhal-glib/libhal-gdevice.la	\
	$(top_builddir)/libhal-glib/libhal-gdevicestore.la	\
	$(top_builddir)/libhal-glib/libhal-gmanager.la	\
	$(top_builddir)/libhal-glib/libhal-gpower.la

noinst_PROGRAMS =					\
	gnome-power-inhibit-test			\
	gnome-power-self-test				\
	$(NULL)

gnome_power_inhibit_test_SOURCES =			\
	gpm-inhibit-test.c				\
	$(NULL)

gnome_power_inhibit_test_LDADD =			\
	$(GLIB_LIBS)					\
	$(GNOME_LIBS)					\
	$(DBUS_LIBS)					\
	$(NULL)

gnome_power_self_test_SOURCES =				\
	gpm-st-main.c					\
	gpm-st-main.h					\
	gpm-st-common.c					\
	gpm-st-array.c					\
	gpm-st-proxy.c					\
	gpm-st-inhibit.c				\
	gpm-st-hal-power.c				\
	gpm-st-hal-device.c				\
	gpm-st-hal-devicestore.c			\
	gpm-st-hal-manager.c				\
	../src/gpm-dbus-monitor.h			\
	../src/gpm-dbus-monitor.c			\
	../src/gpm-marshal.h				\
	../src/gpm-marshal.c				\
	../src/gpm-array.h				\
	../src/gpm-array.c				\
	../src/gpm-common.h				\
	../src/gpm-common.c				\
	../src/gpm-proxy.h				\
	../src/gpm-proxy.c				\
	../src/gpm-powermanager.h			\
	../src/gpm-powermanager.c			\
	../src/gpm-debug.h				\
	../src/gpm-debug.c				\
	$(NULL)

gnome_power_self_test_LDADD =				\
	$(GLIB_LIBS)					\
	$(GNOME_LIBS)					\
	$(DBUS_LIBS)					\
	$(LIBHAL_LIBS)					\
	$(NULL)

clean-local:
	rm -f *~

CLEANFILES = $(BUILT_SOURCES)

MAINTAINERCLEANFILES =					\
	*~			      			\
	Makefile.in					\
	$(NULL)

