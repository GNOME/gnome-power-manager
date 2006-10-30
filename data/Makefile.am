SUBDIRS = icons sounds

schemadir = $(sysconfdir)/gconf/schemas
schema_in_files = gnome-power-manager.schemas.in
schema_DATA = $(schema_in_files:.schemas.in=.schemas)
@INTLTOOL_SCHEMAS_RULE@

desktopdir = $(datadir)/applications
desktop_in_files = gnome-power-preferences.desktop.in \
		   gnome-power-statistics.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

autostartdir = $(datadir)/gnome/autostart
autostart_in_files = gnome-power-manager.desktop.in
autostart_DATA = $(autostart_in_files:.desktop.in=.desktop)

dbusconfdir = $(DBUS_SYS_DIR)
dbusconf_DATA = gnome-power-manager.conf

servicedir       = $(DBUS_SERVICES_DIR)
service_in_files = gnome-power-manager.service.in
service_DATA     = $(service_in_files:.service.in=.service)

$(service_DATA): $(service_in_files) Makefile
	@sed -e "s|\@servicedir\@|$(bindir)|" $< > $@

pkgdata_DATA =						\
	gpm-graph.glade					\
	gpm-feedback-widget.glade			\
	gpm-inhibit-test.glade				\
	gpm-prefs.glade

EXTRA_DIST =						\
	gnome-power-manager.schemas.in			\
	$(schema_in_files)				\
	$(desktop_in_files)				\
	$(autostart_in_files)				\
	$(service_in_files)				\
	$(pkgdata_DATA)					\
	$(dbusconf_DATA)

install-data-local:
if GCONF_SCHEMAS_INSTALL
	-GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) \
	$(GCONFTOOL) --makefile-install-rule $(schema_DATA)
endif

clean-local :
	rm -f *~

DISTCLEANFILES =					\
	gnome-power-manager.desktop			\
	gnome-power-preferences.desktop			\
	gnome-power-statistics.desktop			\
	gnome-power-manager.schemas			\
	gnome-power-manager.service
