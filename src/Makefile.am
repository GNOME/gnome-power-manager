## We require new-style dependency handling.
AUTOMAKE_OPTIONS = 1.7

AM_CPPFLAGS =						\
	$(GLIB_CFLAGS)					\
	$(GNOME_CFLAGS)					\
	$(X11_CFLAGS)					\
	$(NOTIFY_CFLAGS)				\
	$(GSTREAMER_CFLAGS)				\
	$(UPOWER_CFLAGS)				\
	-DBINDIR=\"$(bindir)\"			 	\
	-DSBINDIR=\"$(sbindir)\"			\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"	\
	-DDATADIR=\"$(datadir)\"			\
	-DPREFIX=\""$(prefix)"\" 			\
	-DSYSCONFDIR=\""$(sysconfdir)"\" 		\
	-DLIBDIR=\""$(libdir)"\" 			\
	-DVERSION="\"$(VERSION)\"" 			\
	-DGPM_DATA=\"$(pkgdatadir)\"			\
	-DGTKBUILDERDIR=\"$(pkgdatadir)\"		\
	-DUP_DISABLE_DEPRECATED				\
	-DG_LOG_DOMAIN=\"PowerManager\"			\
	-I$(top_srcdir)

bin_PROGRAMS =						\
	gnome-power-statistics

check_PROGRAMS =					\
	gnome-power-self-test

gnome_power_statistics_SOURCES =			\
	gpm-array-float.c				\
	gpm-array-float.h				\
	gpm-statistics.c				\
	gpm-resources.c					\
	gpm-resources.h					\
	gpm-point-obj.c					\
	gpm-point-obj.h					\
	gpm-graph-widget.h				\
	gpm-graph-widget.c

gnome_power_statistics_LDADD =				\
	$(GLIB_LIBS)					\
	$(X11_LIBS)					\
	$(UPOWER_LIBS)					\
	$(GNOME_LIBS)					\
	-lm

gnome_power_statistics_CFLAGS =				\
	$(WARN_CFLAGS)

gnome_power_self_test_SOURCES =				\
	gpm-array-float.h				\
	gpm-array-float.c				\
	gpm-self-test.c

gnome_power_self_test_LDADD =				\
	$(GLIB_LIBS)					\
	$(X11_LIBS)					\
	$(GNOME_LIBS)					\
	$(GSTREAMER_LIBS)				\
	$(UPOWER_LIBS)					\
	$(X11_LIBS)					\
	$(NOTIFY_LIBS)					\
	$(GPM_EXTRA_LIBS)				\
	-lm

gnome_power_self_test_CFLAGS =				\
	$(WARN_CFLAGS)

gnome_power_self_test_CFLAGS = -DEGG_TEST $(AM_CFLAGS) $(WARNINGFLAGS)

gpm-resources.c: gnome-power-manager.gresource.xml gpm-statistics.ui
	glib-compile-resources --target=$@ --sourcedir=$(top_srcdir)/src --generate-source --c-name gpm $(srcdir)/gnome-power-manager.gresource.xml
gpm-resources.h: gnome-power-manager.gresource.xml gpm-statistics.ui
	glib-compile-resources --target=$@ --sourcedir=$(top_srcdir)/src --generate-header --c-name gpm $(srcdir)/gnome-power-manager.gresource.xml

BUILT_SOURCES =						\
	gpm-resources.c					\
	gpm-resources.h					\
	$(NULL)

EXTRA_DIST =						\
	gnome-power-manager.gresource.xml		\
	gpm-statistics.ui

clean-local:
	rm -f *~

CLEANFILES = $(BUILT_SOURCES)

TESTS = gnome-power-self-test

MAINTAINERCLEANFILES =					\
	*~			      			\
	Makefile.in

-include $(top_srcdir)/git.mk
