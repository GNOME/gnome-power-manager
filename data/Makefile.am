SUBDIRS = icons

@INTLTOOL_DESKTOP_RULE@
desktopdir = $(datadir)/applications
desktop_in_files = gnome-power-statistics.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

appdatadir = $(datadir)/appdata
appdata_DATA = gnome-power-statistics.appdata.xml

@GSETTINGS_RULES@
@INTLTOOL_XML_NOMERGE_RULE@
gsettings_schemas_in_files = org.gnome.power-manager.gschema.xml.in
gsettings_SCHEMAS = $(gsettings_schemas_in_files:.gschema.xml.in=.gschema.xml)

UI_FILES =						\
	gpm-statistics.ui

EXTRA_DIST =						\
	$(appdata_files)				\
	$(gsettings_SCHEMAS)				\
	$(desktop_in_files)				\
	$(gsettings_schemas_in_files)			\
	$(UI_FILES)

clean-local :
	rm -f *~

DISTCLEANFILES =					\
	gnome-power-statistics.desktop			\
	org.gnome.power-manager.gschema.xml

-include $(top_srcdir)/git.mk
