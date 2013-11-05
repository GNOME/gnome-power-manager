SUBDIRS = appdata icons

@INTLTOOL_DESKTOP_RULE@
desktopdir = $(datadir)/applications
desktop_in_files = gnome-power-statistics.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

@GSETTINGS_RULES@
gsettings_SCHEMAS = org.gnome.power-manager.gschema.xml

EXTRA_DIST =						\
	$(gsettings_SCHEMAS)				\
	$(desktop_in_files)

clean-local :
	rm -f *~

DISTCLEANFILES =					\
	gnome-power-statistics.desktop			\
	org.gnome.power-manager.gschema.xml

-include $(top_srcdir)/git.mk
