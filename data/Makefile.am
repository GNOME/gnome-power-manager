SUBDIRS = appdata icons

%.desktop: %.desktop.in
	$(AM_V_GEN) msgfmt --desktop -d $(top_srcdir)/po --template $< -o $@

desktopdir = $(datadir)/applications
desktop_in_files = org.gnome.PowerStats.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

@GSETTINGS_RULES@
gsettings_SCHEMAS = org.gnome.power-manager.gschema.xml

EXTRA_DIST =						\
	$(gsettings_SCHEMAS)				\
	$(desktop_in_files)

clean-local :
	rm -f *~

DISTCLEANFILES =					\
	org.gnome.PowerStats.desktop			\
	org.gnome.power-manager.gschema.xml

-include $(top_srcdir)/git.mk
