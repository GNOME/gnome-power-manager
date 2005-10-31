EXTRA_DIST =						\
	gnome-power-manager.sgml			\
	gnome-power-preferences.sgml			\
	gnome-power-info.sgml

if HAVE_DOCBOOK2MAN
man_MANS =						\
	gnome-power-manager.1				\
	gnome-power-preferences.1			\
	gnome-power-info.1
endif

if HAVE_DOCBOOK2MAN
gnome-power-manager.1: gnome-power-manager.sgml
	docbook2man $? > $@

gnome-power-preferences.1: gnome-power-preferences.sgml
	docbook2man $? > $@

gnome-power-info.1: gnome-power-info.sgml
	docbook2man $? > $@
endif

clean-local :
	rm -f *~
	rm -f *.1
	rm -f manpage.*
