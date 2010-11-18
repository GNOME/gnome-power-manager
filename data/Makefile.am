SUBDIRS = icons

migrationdir = $(datadir)/GConf/gsettings
dist_migration_DATA = org.gnome.power-manager.gschema.migrate

@INTLTOOL_DESKTOP_RULE@
desktopdir = $(datadir)/applications
desktop_in_files = gnome-power-statistics.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

@INTLTOOL_DESKTOP_RULE@
autostartdir = $(sysconfdir)/xdg/autostart
autostart_in_files = gnome-power-manager.desktop.in
autostart_DATA = $(autostart_in_files:.desktop.in=.desktop)

servicedir       = $(DBUS_SERVICES_DIR)
service_in_files = gnome-power-manager.service.in
service_DATA     = $(service_in_files:.service.in=.service)

$(service_DATA): $(service_in_files) Makefile
	@sed -e "s|\@servicedir\@|$(bindir)|" $< > $@

@GSETTINGS_RULES@
gsettings_SCHEMAS = org.gnome.power-manager.gschema.xml

pkgdata_DATA =						\
	gpm-statistics.ui				\
	gpm-feedback-widget.ui				\
	acme.ui

EXTRA_DIST =						\
	$(gsettings_SCHEMAS)				\
	$(service_in_files)				\
	$(autostart_in_files)				\
	$(desktop_in_files)				\
	$(pkgdata_DATA)

clean-local :
	rm -f *~

DISTCLEANFILES =					\
	gnome-power-manager.desktop			\
	gnome-power-statistics.desktop			\
	gnome-power-manager.service
