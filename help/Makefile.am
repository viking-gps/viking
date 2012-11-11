include $(top_srcdir)/gnome-doc-utils.make

# Annoyingly in the XML, yelp refuses to load relative images fileref, i.e. ../../src/icons/file.png
#  whereas the PDF generation inserts these references OK
# Nevertheless we need to copy the images into figures/ so they are available for the distribution installed help
# Most tool icons are _18, except the pan one which is _22 just to be annoying
get-icons:
	cp -f ../src/icons/*_[12][0-9].png C/figures/

dist-hook: get-icons doc-dist-hook

DOC_MODULE = viking
DOC_ENTITIES = legal.xml
DOC_INCLUDES =
DOC_FIGURES = figures/*.png
DOC_LINGUAS =
