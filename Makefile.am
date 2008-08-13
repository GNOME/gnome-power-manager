
ACLOCAL_AMFLAGS = -I m4

SUBDIRS =						\
	libdbus-glib					\
	libhal-glib					\
	libidletime					\
	libunique					\
	src						\
	po						\
	docs						\
	man						\
	data						\
	help						\
	tools

if HAVE_APPLETS
SUBDIRS += applets
endif

if HAVE_TESTS
SUBDIRS += test
endif

EXTRA_DIST =						\
	HACKING						\
	MAINTAINERS					\
	intltool-extract.in				\
	intltool-merge.in				\
	intltool-update.in				\
	omf.make					\
	xmldocs.make					\
	gnome-doc-utils.make

clean-local :
	rm -f *~

DISTCLEANFILES =					\
	intltool-extract				\
	intltool-merge					\
	intltool-update					\
	gnome-power-manager-*.tar.gz

DISTCHECK_CONFIGURE_FLAGS = --disable-schemas-install --disable-scrollkeeper --disable-applets

