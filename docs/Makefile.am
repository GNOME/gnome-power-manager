EXTRA_DIST = \
	Doxyfile		\
	Doxyfile.in		\
	utopia.repo		\
	video.txt

if DOXYGEN_DOCS_ENABLED

htmldocdir = $(DOCDIR)/doxygen
htmldoc_DATA = doxygen/*

doxygen/* : Doxyfile
	$(DOXYGEN)

endif # DOXYGEN_DOCS_ENABLED

clean-local:
	rm -f *~

DISTCLEANFILES =					\
	doxygen/*
