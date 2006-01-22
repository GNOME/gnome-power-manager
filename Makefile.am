SUBDIRS = src po docs man data help

EXTRA_DIST =						\
	HACKING						\
	MAINTAINERS					\
	intltool-extract.in				\
	intltool-merge.in				\
	intltool-update.in

clean-local :
	rm -f *~

DISTCLEANFILES =					\
	intltool-extract				\
	intltool-merge					\
	intltool-update					\
	gnome-doc-utils.make				\
	gnome-power-manager-*.tar.gz

DISTCHECK_CONFIGURE_FLAGS = --disable-schemas-install --disable-scrollkeeper
