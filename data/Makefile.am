SUBDIRS = icons

@INTLTOOL_DESKTOP_RULE@
desktopdir = $(datadir)/applications
desktop_in_files = gnome-power-statistics.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

@INTLTOOL_XML_RULE@
appdatadir = $(datadir)/appdata
appdata_DATA = $(appdata_in_files:.xml.in=.xml)
appdata_in_files = gnome-power-statistics.appdata.xml.in

@GSETTINGS_RULES@
gsettings_SCHEMAS = org.gnome.power-manager.gschema.xml

UI_FILES =						\
	gpm-statistics.ui

EXTRA_DIST =						\
	$(gsettings_SCHEMAS)				\
	$(appdata_in_files)				\
	$(desktop_in_files)				\
	$(UI_FILES)

CLEANFILES =						\
	$(appdata_DATA)

clean-local :
	rm -f *~

DISTCLEANFILES =					\
	gnome-power-statistics.desktop			\
	org.gnome.power-manager.gschema.xml

-include $(top_srcdir)/git.mk
