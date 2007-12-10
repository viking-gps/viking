SUBDIRS = src test doc po

INTLTOOL = \
	intltool-extract.in \
	intltool-merge.in \
	intltool-update.in

full:
	./autogen.sh
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

ACLOCAL_AMFLAGS = -I m4
