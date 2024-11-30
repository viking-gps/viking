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
	NEWS \
	ChangeLog.0 \
	viking.spec \
	mingw32-viking.spec \
	mingw64-viking.spec \
	FLATPAK.md \
	org.viking.Viking.yml \
	org.viking.Viking.appdata.xml \
	viking-128.png \
	$(INTLTOOL)
# NB viking-128.png is just a rescaled src/icons/viking.png
# Yet needed for Flathub
# ATM Simpler to just embed copy in source control rather than generate
#  (e.g. via ImageMagick convert - as then need that in the build chain)

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
	intltool-update \
	po/.intltool-merge-cache.lock

ACLOCAL_AMFLAGS = -I m4

# Ignore gtk theme cache files on distcheck
distuninstallcheck_listfiles = find . -type f -print | grep -v 'icon-theme.cache'
