SPEC_XML_FILES =			\
	gnome-power-manager.xml		\
	dbus-interface.xml		\
	sleep-names.xml			\
	faq.xml				\
	icon-scheme.xml

SPEC_HTML_FILES =			\
	dbus-interface.html		\
	faq.html			\
	icon-scheme.html		\
	index.html			\
	sleep-names.html

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
	gnome-power-manager.xml.in	\
	config.xsl			\
	docbook.css			\
	$(SPEC_XML_FILES)		\
	$(SPEC_HTML_FILES)
