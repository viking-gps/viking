SUBDIRS = src test doc po help

INTLTOOL = \
	intltool-extract.in \
	intltool-merge.in \
	intltool-update.in

full:
	./autogen.sh
	make

EXTRA_DIST = \
	     gnome-doc-utils.make \
		 ChangeLog.0 \
	     viking.spec \
	     $(INTLTOOL)

.PHONY: generate-changelog
generate-changelog:
	if test -d $(top_srcdir)/.git; then \
          cd $(top_srcdir) ; sh $(top_srcdir)/maintainer/git2changelog.sh > $(distdir)/c-l; \
          mv $(distdir)/c-l $(distdir)/ChangeLog; \
        fi

dist-hook: viking.spec generate-changelog
	cp $(top_builddir)/viking.spec $(distdir)

DISTCLEANFILES = \
	gnome-doc-utils.make \
	intltool-extract \
	intltool-merge \
	intltool-update

ACLOCAL_AMFLAGS = -I m4

DISTCHECK_CONFIGURE_FLAGS = \
	--disable-scrollkeeper
