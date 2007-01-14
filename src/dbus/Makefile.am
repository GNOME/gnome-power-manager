NULL =

EXTRA_DIST =						\
	gpm-dbus-control.xml				\
	gpm-dbus-inhibit.xml				\
	gpm-dbus-manager.xml				\
	gpm-dbus-statistics.xml				\
	gpm-dbus-ui.xml					\
	gpm-dbus-backlight.xml				\
	$(NULL)

BUILT_SOURCES = 					\
	gpm-dbus-control.h				\
	gpm-dbus-inhibit.h				\
	gpm-dbus-manager.h				\
	gpm-dbus-statistics.h				\
	gpm-dbus-ui.h					\
	gpm-dbus-backlight.h				\
	$(NULL)

gpm-dbus-control.h: gpm-dbus-control.xml
	libtool --mode=execute dbus-binding-tool	\
		--prefix=gpm_control			\
		--mode=glib-server			\
		--output=gpm-dbus-control.h		\
		$(srcdir)/gpm-dbus-control.xml

gpm-dbus-inhibit.h: gpm-dbus-inhibit.xml
	libtool --mode=execute dbus-binding-tool	\
		--prefix=gpm_inhibit			\
		--mode=glib-server			\
		--output=gpm-dbus-inhibit.h		\
		$(srcdir)/gpm-dbus-inhibit.xml

gpm-dbus-ui.h: gpm-dbus-ui.xml
	libtool --mode=execute dbus-binding-tool	\
		--prefix=gpm_ui				\
		--mode=glib-server			\
		--output=gpm-dbus-ui.h			\
		$(srcdir)/gpm-dbus-ui.xml

gpm-dbus-manager.h: gpm-dbus-manager.xml
	libtool --mode=execute dbus-binding-tool	\
		--prefix=gpm_manager			\
		--mode=glib-server			\
		--output=gpm-dbus-manager.h		\
		$(srcdir)/gpm-dbus-manager.xml

gpm-dbus-statistics.h: gpm-dbus-statistics.xml
	libtool --mode=execute dbus-binding-tool	\
		--prefix=gpm_statistics			\
		--mode=glib-server			\
		--output=gpm-dbus-statistics.h		\
		$(srcdir)/gpm-dbus-statistics.xml

gpm-dbus-backlight.h: gpm-dbus-backlight.xml
	libtool --mode=execute dbus-binding-tool	\
		--prefix=gpm_backlight			\
		--mode=glib-server			\
		--output=gpm-dbus-backlight.h		\
		$(srcdir)/gpm-dbus-backlight.xml

clean-local:
	rm -f *~

CLEANFILES = $(BUILT_SOURCES)

MAINTAINERCLEANFILES =					\
	*~			      			\
	Makefile.in					\
	$(NULL)
