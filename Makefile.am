SUBDIRS = src test data po windows tools doc
if !WINDOWS
SUBDIRS += help
endif

INTLTOOL = \
	intltool-extract.in \
	intltool-merge.in \
	intltool-update.in

full:
	./autogen.sh
	make

EXTRA_DIST = \
	README.md \
	HACKING \
	ChangeLog.0 \
	viking.spec \
	mingw-viking.spec \
	mingw64-viking.spec \
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
	intltool-extract \
	intltool-merge \
	intltool-update

ACLOCAL_AMFLAGS = -I m4

# Ignore gtk theme cache files on distcheck
distuninstallcheck_listfiles = find . -type f -print | grep -v 'icon-theme.cache'
