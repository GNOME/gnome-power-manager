if DOXYGEN_DOCS_ENABLED

EXTRA_DIST = \
	Doxyfile		\
	Doxyfile.in		\
	utopia.repo		\
	video.txt

htmldocdir = $(DOCDIR)
htmldoc_DATA = doxygen/*

doxygen/* : Doxyfile
	$(DOXYGEN)

endif # DOXYGEN_DOCS_ENABLED

clean-local:
	rm -f *~
