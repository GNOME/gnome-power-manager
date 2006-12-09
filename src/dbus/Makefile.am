NULL =

EXTRA_DIST =						\
	gpm-dbus-dpms.xml				\
	gpm-dbus-manager.xml				\
	gpm-dbus-statistics.xml				\
	gpm-dbus-brightness-lcd.xml			\
	$(NULL)

BUILT_SOURCES = 					\
	gpm-dbus-dpms.h					\
	gpm-dbus-manager.h				\
	gpm-dbus-statistics.h				\
	gpm-dbus-brightness-lcd.h			\
	$(NULL)

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
