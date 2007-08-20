SUBDIRS = src test doc

full:
	./autogen.sh
	if [ -x config.status ] ; then ./config.status --recheck ; else ./configure ; fi
	make

bin_SCRIPTS = viking-remote

EXTRA_DIST = \
	     viking-remote \
	     viking.spec

dist-hook: viking.spec
	cp $(top_builddir)/viking.spec $(distdir)

ACLOCAL_AMFLAGS = -I m4
