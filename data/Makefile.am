SUBDIRS = icons

@INTLTOOL_DESKTOP_RULE@
desktopdir = $(datadir)/applications
desktop_in_files = gnome-power-statistics.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

@GSETTINGS_RULES@
@INTLTOOL_XML_NOMERGE_RULE@
gsettings_schemas_in_files = org.gnome.power-manager.gschema.xml.in
gsettings_SCHEMAS = $(gsettings_schemas_in_files:.gschema.xml.in=.gschema.xml)

pkgdata_DATA =						\
	gpm-statistics.ui

EXTRA_DIST =						\
	$(gsettings_SCHEMAS)				\
	$(desktop_in_files)				\
	$(gsettings_schemas_in_files)			\
	$(pkgdata_DATA)

clean-local :
	rm -f *~

DISTCLEANFILES =					\
	gnome-power-statistics.desktop			\
	org.gnome.power-manager.gschema.xml
