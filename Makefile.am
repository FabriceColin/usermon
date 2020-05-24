@SET_MAKE@

AUTOMAKE_OPTIONS = subdir-objects
ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

SUBDIRS =	\
	icons	\
	panel-plugin \
	po

distclean-local:
	rm -rf *.cache *~

distuninstallcheck_listfiles =                                          \
	find . -type f -print | grep -v ./share/icons/hicolor/icon-theme.cache

indent:
	indent -linux panel-plugin/usermon.c
	indent -linux panel-plugin/usermon.h
	indent -linux panel-plugin/usermon-dialogs.c
	indent -linux panel-plugin/usermon-dialogs.h

.PHONY: ChangeLog

ChangeLog: Makefile
	(GIT_DIR=$(top_srcdir)/.git git log > .changelog.tmp \
	&& mv .changelog.tmp ChangeLog; rm -f .changelog.tmp) \
	|| (touch ChangeLog; echo 'Git directory not found: installing possibly empty changelog.' >&2)

dist-hook: ChangeLog

EXTRA_DIST = \
	intltool-extract.in \
	intltool-merge.in \
	intltool-update.in \
	usermon.spec

DISTCLEANFILES = \
	intltool-extract \
	intltool-merge \
	intltool-update

