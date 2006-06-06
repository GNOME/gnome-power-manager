SPEC_XML_FILES =			\
	gnome-power-manager.xml		\
	dbus-interface.xml		\
	icon-scheme.xml

if DOCBOOK_DOCS_ENABLED

htmldocdir = $(DOCDIR)/spec
htmldoc_DATA = gnome-power-manager.html

gnome-power-manager.html : dbus-interface.xml
	$(XMLTO) html-nochunks -m config.xsl gnome-power-manager.xml

endif # DOCBOOK_DOCS_ENABLED

clean-local:
	rm -f *~
	rm -f gnome-power-manager.html gnome-power-manager.xml

EXTRA_DIST = 				\
	gnome-power-manager.html	\
	gnome-power-manager.xml.in	\
	config.xsl			\
	docbook.css			\
	$(SPEC_XML_FILES)
