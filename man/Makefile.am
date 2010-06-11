EXTRA_DIST =						\
	gnome-power-manager.sgml			\
	gnome-power-statistics.sgml			\
	gnome-power-preferences.sgml

if HAVE_DOCBOOK2MAN
man_MANS =						\
	gnome-power-manager.1				\
	gnome-power-statistics.1			\
	gnome-power-preferences.1
endif

if HAVE_DOCBOOK2MAN
gnome-power-manager.1: gnome-power-manager.sgml
	docbook2man $? &> /dev/null

gnome-power-preferences.1: gnome-power-preferences.sgml
	docbook2man $? &> /dev/null

gnome-power-statistics.1: gnome-power-statistics.sgml
	docbook2man $? &> /dev/null
endif

clean-local :
	rm -f *~
	rm -f *.1
	rm -f manpage.*
