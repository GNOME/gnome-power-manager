SUBDIRS = src po

schemadir = $(sysconfdir)/gconf/schemas
schema_in_files = gnome-power.schemas.in
schema_DATA = $(schema_in_files:.schemas.in=.schemas)
@INTLTOOL_SCHEMAS_RULE@

desktopdir = $(datadir)/applications
desktop_in_files = gnome-power-preferences.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

dbusconfdir = $(sysconfdir)/dbus-1/system.d
dbusconf_DATA = gnome-power.conf

man_MANS =						\
	gnome-power-manager.1				\
	gnome-power-preferences.1

EXTRA_DIST =						\
	HACKING						\
	gnome-power.spec				\
	gnome-power.spec.in				\
	gnome-power.schemas.in				\
	$(schema_in_files)				\
	$(desktop_in_files)				\
	$(dbusconf_DATA)				\
	intltool-extract.in				\
	intltool-merge.in				\
	intltool-update.in				\
	gnome-power-manager.sgml			\
	gnome-power-preferences.sgml			\
	mkinstalldirs

install-data-local:
	-GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) \
	$(GCONFTOOL) --makefile-install-rule $(srcdir)/$(schema_DATA)
	

clean-local :
	rm -f *~
	rm -f *.1
	rm -f manpage.*

gnome-power-manager.1: gnome-power-manager.sgml
	docbook2man $? > $@

gnome-power-preferences.1: gnome-power-preferences.sgml
	docbook2man $? > $@

DISTCLEANFILES =					\
	intltool-extract				\
	intltool-merge					\
	intltool-update					\
	gnome-power-preferences.desktop			\
	gnome-power.schemas

MAINTAINERCLEANFILES =					\
	Makefile.in					\
	aclocal.m4					\
	config.guess					\
	config.h.in					\
	config.sub					\
	configure					\
	depcomp						\
	install-sh					\
	libtool						\
	ltmain.sh					\
	missing						\
	autom4te.cache/*				\
	Makefile.in					\
	po/Makefile.in					\
	src/pixmaps/Makefile.in				\
	src/Makefile.in					\
	src/sounds/Makefile.in				\
	src/icons/Makefile.in				\
	po/$(GETTEXT_PACKAGE).pot			\
	po/*.bak
