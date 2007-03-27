if DOCBOOK_DOCS_ENABLED

htmldocdir = $(DOCDIR)/spec
htmldoc_DATA = dbus-interface.html

dbus-interface.html : dbus-interface.xml
	$(XMLTO) html-nochunks -m config.xsl dbus-interface.xml

endif # DOCBOOK_DOCS_ENABLED

clean-local:
	rm -f *~
	rm -f *.html

EXTRA_DIST = 				\
	config.xsl			\
	docbook.css			\
	dbus-interface.xml		\
	dbus-interface.html

