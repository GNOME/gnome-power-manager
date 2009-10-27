SUBDIRS = icons

@INTLTOOL_SCHEMAS_RULE@
schemadir = $(sysconfdir)/gconf/schemas
schema_in_files = gnome-power-manager.schemas.in
schema_DATA = $(schema_in_files:.schemas.in=.schemas)

@INTLTOOL_DESKTOP_RULE@
desktopdir = $(datadir)/applications
desktop_in_files = gnome-power-preferences.desktop.in \
		   gnome-power-statistics.desktop.in
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

pkgdata_DATA =						\
	gpm-statistics.ui				\
	gpm-feedback-widget.ui				\
	gpm-prefs.ui					\
	acme.ui

EXTRA_DIST =						\
	gnome-power-manager.schemas.in			\
	$(schema_in_files)				\
	$(desktop_in_files)				\
	$(autostart_in_files)				\
	$(service_in_files)				\
	$(pkgdata_DATA)

if GCONF_SCHEMAS_INSTALL
install-data-local:
	-GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) \
	$(GCONFTOOL) --makefile-install-rule $(schema_DATA)
endif

if GCONF_SCHEMAS_INSTALL
uninstall-local:
	-GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) \
	$(GCONFTOOL) --makefile-uninstall-rule $(schema_DATA)
endif

clean-local :
	rm -f *~

DISTCLEANFILES =					\
	gnome-power-manager.desktop			\
	gnome-power-preferences.desktop			\
	gnome-power-statistics.desktop			\
	gnome-power-manager.schemas			\
	gnome-power-manager.service
