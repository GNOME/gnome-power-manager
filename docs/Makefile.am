SPEC_XML_FILES =			\
	gnome-power-manager.xml		\
	dbus-interface.xml		\
	roadmap.xml			\
	sleep-names.xml			\
	faq.xml				\
	icon-scheme.xml

if DOCBOOK_DOCS_ENABLED

htmldocdir = $(DOCDIR)/spec
htmldoc_DATA = index.html

index.html : dbus-interface.xml
	$(XMLTO) html -m config.xsl gnome-power-manager.xml

endif # DOCBOOK_DOCS_ENABLED

clean-local:
	rm -f *~
	rm -f *.html

EXTRA_DIST = 				\
	*.html				\
	gnome-power-manager.xml.in	\
	config.xsl			\
	docbook.css			\
	$(SPEC_XML_FILES)
