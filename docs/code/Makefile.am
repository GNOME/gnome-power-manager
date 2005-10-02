if DOXYGEN_DOCS_ENABLED

EXTRA_DIST = Doxyfile Doxyfile.in

htmldocdir = $(DOCDIR)/code
htmldoc_DATA = html/*

html/* : Doxyfile
	$(DOXYGEN)

clean-local:
	rm -f *~
	rm -rf html

endif # DOXYGEN_DOCS_ENABLED
