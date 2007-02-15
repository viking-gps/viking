SUBDIRS = src test doc

bin_SCRIPTS = viking-remote

EXTRA_DIST = \
	     viking-remote \
	     viking.spec

dist-hook: viking.spec
	cp $(top_builddir)/viking.spec $(distdir)
