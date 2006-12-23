NULL =

EXTRA_DIST =						\
	gpm-dbus-inhibit.xml				\
	gpm-dbus-dpms.xml				\
	gpm-dbus-manager.xml				\
	gpm-dbus-statistics.xml				\
	gpm-dbus-ui.xml					\
	gpm-dbus-brightness-lcd.xml			\
	$(NULL)

BUILT_SOURCES = 					\
	gpm-dbus-inhibit.h				\
	gpm-dbus-dpms.h					\
	gpm-dbus-manager.h				\
	gpm-dbus-statistics.h				\
	gpm-dbus-ui.h					\
	gpm-dbus-brightness-lcd.h			\
	$(NULL)

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

gpm-dbus-dpms.h: gpm-dbus-dpms.xml
	libtool --mode=execute dbus-binding-tool	\
		--prefix=gpm_dpms			\
		--mode=glib-server			\
		--output=gpm-dbus-dpms.h		\
		$(srcdir)/gpm-dbus-dpms.xml

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

gpm-dbus-brightness-lcd.h: gpm-dbus-brightness-lcd.xml
	libtool --mode=execute dbus-binding-tool	\
		--prefix=gpm_brightness_lcd		\
		--mode=glib-server			\
		--output=gpm-dbus-brightness-lcd.h	\
		$(srcdir)/gpm-dbus-brightness-lcd.xml

clean-local:
	rm -f *~

CLEANFILES = $(BUILT_SOURCES)

MAINTAINERCLEANFILES =					\
	*~			      			\
	Makefile.in					\
	$(NULL)
