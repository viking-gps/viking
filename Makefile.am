SUBDIRS = src test doc po

INTLTOOL = \
	intltool-extract.in \
	intltool-merge.in \
	intltool-update.in

full:
	./autogen.sh
	if [ -x config.status ] ; then ./config.status --recheck ; else ./configure ; fi
	make

bin_SCRIPTS = viking-remote

EXTRA_DIST = \
	     viking-remote \
	     viking.spec \
	     $(INTLTOOL)

dist-hook: viking.spec
	cp $(top_builddir)/viking.spec $(distdir)

DISTCLEANFILES = \
	intltool-extract \
	intltool-merge \
	intltool-update
