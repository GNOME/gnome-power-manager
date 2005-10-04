SUBDIRS = src po docs

schemadir = $(sysconfdir)/gconf/schemas
schema_in_files = gnome-power-manager.schemas.in
schema_DATA = $(schema_in_files:.schemas.in=.schemas)
@INTLTOOL_SCHEMAS_RULE@

desktopdir = $(datadir)/applications
desktop_in_files = gnome-power-preferences.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

dbusconfdir = $(DBUS_SYS_DIR)
dbusconf_DATA = gnome-power-manager.conf

servicedir       = $(DBUS_SERVICES_DIR)
service_in_files = gnome-power-manager.service.in
service_DATA     = $(service_in_files:.service.in=.service)

if HAVE_DOCBOOK2MAN
man_MANS =						\
	gnome-power-manager.1				\
	gnome-power-preferences.1
endif

$(service_DATA): $(service_in_files) Makefile
	@sed -e "s|\@servicedir\@|$(bindir)|" $< > $@

EXTRA_DIST =						\
	HACKING						\
	gnome-power-manager.spec			\
	gnome-power-manager.spec.in			\
	gnome-power-manager.schemas.in			\
	$(schema_in_files)				\
	$(desktop_in_files)				\
	$(service_in_files)				\
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

if HAVE_DOCBOOK2MAN
gnome-power-manager.1: gnome-power-manager.sgml
	docbook2man $? > $@

gnome-power-preferences.1: gnome-power-preferences.sgml
	docbook2man $? > $@
endif

DISTCLEANFILES =					\
	intltool-extract				\
	intltool-merge					\
	intltool-update					\
	Makefile					\
	Makefile.in					\
	gnome-power-manager-*.tar.gz			\
	gnome-power-preferences.desktop			\
	gnome-power-manager.schemas			\
	gnome-power-manager.service

MAINTAINERCLEANFILES =					\
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
	po/$(GETTEXT_PACKAGE).pot			\
	po/*.bak
