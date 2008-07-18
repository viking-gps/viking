SUBDIRS = dev

EXTRA_DIST = GEOCODED-PHOTOS GETTING-STARTED GPSMAPPER \
  viking.xml viking-remote.xml

# man pages processing
# TODO conditional processing
# (availability of docbook stylesheet and xsltproc)
DB2MAN=/usr/share/sgml/docbook/stylesheet/xsl/nwalsh/manpages/docbook.xsl
XP=xsltproc -''-nonet
MANPAGES=viking.1 viking-remote.1

man_MANS = $(MANPAGES)

%.1: %.xml
	$(XP) -o $@ $(DB2MAN) $<
