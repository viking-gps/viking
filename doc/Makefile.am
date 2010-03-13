SUBDIRS = dev examples

EXTRA_DIST = GEOCODED-PHOTOS GETTING-STARTED GPSMAPPER \
  viking.xml viking-remote.xml

CLEANFILES=

# man pages processing
# TODO conditional processing
# (availability of docbook stylesheet and xsltproc)
if GEN_MANPAGES
MANPAGES=viking.1 viking-remote.1
man_MANS = $(MANPAGES)
CLEANFILES+=$(MANPAGES)
endif
DB2MAN=@DB2MAN_XSL@
XP=@XP@ -''-nonet

%.1: %.xml
	$(XP) -o $@ $(DB2MAN) $<
