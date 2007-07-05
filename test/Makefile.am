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
	-I$(top_srcdir)/libhal-glib			\
	-I$(top_srcdir)/libdbus-glib			\
	$(NULL)

LOCAL_LIBHAL_LIBS =					\
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
	gpm-st-array-float.c				\
	gpm-st-cell-unit.c				\
	gpm-st-cell-array.c				\
	gpm-st-cell.c					\
	gpm-st-webcam.c					\
	gpm-st-proxy.c					\
	gpm-st-inhibit.c				\
	gpm-st-profile.c				\
	gpm-st-graph-widget.c				\
	gpm-st-hal-power.c				\
	gpm-st-hal-device.c				\
	gpm-st-hal-devicestore.c			\
	gpm-st-hal-manager.c				\
	../src/gpm-graph-widget.h			\
	../src/gpm-graph-widget.c			\
	../src/gpm-marshal.h				\
	../src/gpm-marshal.c				\
	../src/gpm-profile.h				\
	../src/gpm-profile.c				\
	../src/gpm-control.h				\
	../src/gpm-control.c				\
	../src/gpm-polkit.h				\
	../src/gpm-polkit.c				\
	../src/gpm-screensaver.h			\
	../src/gpm-screensaver.c			\
	../src/gpm-networkmanager.h			\
	../src/gpm-networkmanager.c			\
	../src/gpm-webcam.h				\
	../src/gpm-webcam.c				\
	../src/gpm-load.h				\
	../src/gpm-load.c				\
	../src/gpm-dpms.h				\
	../src/gpm-dpms.c				\
	../src/gpm-ac-adapter.h				\
	../src/gpm-ac-adapter.c				\
	../src/gpm-cell.h				\
	../src/gpm-cell.c				\
	../src/gpm-cell-unit.h				\
	../src/gpm-cell-unit.c				\
	../src/gpm-cell-array.h				\
	../src/gpm-cell-array.c				\
	../src/gpm-array.h				\
	../src/gpm-array.c				\
	../src/gpm-array-float.h			\
	../src/gpm-array-float.c			\
	../src/gpm-common.h				\
	../src/gpm-common.c				\
	../src/gpm-warnings.h				\
	../src/gpm-warnings.c				\
	../src/gpm-conf.h				\
	../src/gpm-conf.c				\
	../src/gpm-debug.h				\
	../src/gpm-debug.c				\
	$(NULL)

gnome_power_self_test_LDADD =				\
	$(GLIB_LIBS)					\
	$(GNOME_LIBS)					\
	$(GSTREAMER_LIBS)				\
	$(DBUS_LIBS)					\
	$(LOCAL_LIBHAL_LIBS)				\
	$(NULL)

clean-local:
	rm -f *~

CLEANFILES = $(BUILT_SOURCES)

MAINTAINERCLEANFILES =					\
	*~			      			\
	Makefile.in					\
	$(NULL)

