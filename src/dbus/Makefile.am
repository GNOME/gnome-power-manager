NULL =

EXTRA_DIST =						\
	xdg-power-management-core.xml			\
	xdg-power-management-widget.xml			\
	xdg-power-management-inhibit.xml		\
	xdg-power-management-backlight.xml		\
	xdg-power-management-stats.xml			\
	$(NULL)

BUILT_SOURCES = 					\
	xdg-power-management-core.h			\
	xdg-power-management-widget.h			\
	xdg-power-management-inhibit.h			\
	xdg-power-management-backlight.h		\
	xdg-power-management-stats.h			\
	$(NULL)

xdg-power-management-core.h: xdg-power-management-core.xml
	libtool --mode=execute dbus-binding-tool	\
		--prefix=gpm_manager			\
		--mode=glib-server			\
		--output=xdg-power-management-core.h		\
		$(srcdir)/xdg-power-management-core.xml

xdg-power-management-inhibit.h: xdg-power-management-inhibit.xml
	libtool --mode=execute dbus-binding-tool	\
		--prefix=gpm_inhibit			\
		--mode=glib-server			\
		--output=xdg-power-management-inhibit.h	\
		$(srcdir)/xdg-power-management-inhibit.xml

xdg-power-management-widget.h: xdg-power-management-widget.xml
	libtool --mode=execute dbus-binding-tool	\
		--prefix=gpm_widget				\
		--mode=glib-server			\
		--output=xdg-power-management-widget.h	\
		$(srcdir)/xdg-power-management-widget.xml

xdg-power-management-stats.h: xdg-power-management-stats.xml
	libtool --mode=execute dbus-binding-tool	\
		--prefix=gpm_statistics			\
		--mode=glib-server			\
		--output=xdg-power-management-stats.h	\
		$(srcdir)/xdg-power-management-stats.xml

xdg-power-management-backlight.h: xdg-power-management-backlight.xml
	libtool --mode=execute dbus-binding-tool	\
		--prefix=gpm_backlight			\
		--mode=glib-server			\
		--output=xdg-power-management-backlight.h	\
		$(srcdir)/xdg-power-management-backlight.xml

clean-local:
	rm -f *~

CLEANFILES = $(BUILT_SOURCES)

MAINTAINERCLEANFILES =					\
	*~			      			\
	Makefile.in					\
	$(NULL)
