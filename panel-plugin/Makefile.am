AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-DG_LOG_DOMAIN=\"xfce4-usermon-plugin\" \
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\" \
	-DPACKAGE_ICON_DIR=\"$(datadir)/icons/hicolor\" \
	$(PLATFORM_CPPFLAGS)

#
# User monitor plugin
#
plugin_LTLIBRARIES = \
	libusermon.la

plugindir = \
	$(libdir)/xfce4/panel/plugins

libusermon_la_SOURCES = \
	usermon.c \
	usermon.h \
	usermon-dialogs.c \
	usermon-dialogs.h

libusermon_la_CFLAGS = \
	$(LIBNOTIFY_CFLAGS) \
	$(LIBXFCE4UTIL_CFLAGS) \
	$(LIBXFCE4UI_CFLAGS) \
	$(LIBXFCE4PANEL_CFLAGS) \
	$(PLATFORM_CFLAGS)

libusermon_la_LDFLAGS = \
       -avoid-version \
       -module \
       -no-undefined \
       -export-symbols-regex '^xfce_panel_module_(preinit|init|construct)' \
       $(PLATFORM_LDFLAGS)

libusermon_la_LIBADD = \
	$(LIBNOTIFY_LIBS) \
	$(LIBXFCE4UTIL_LIBS) \
	$(LIBXFCE4UI_LIBS) \
	$(LIBXFCE4PANEL_LIBS)

#
# Desktop file
#
desktopdir =								\
	$(datadir)/xfce4/panel/plugins

desktop_DATA =								\
	usermon.desktop

@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST =								\
	usermon.desktop.in

CLEANFILES =								\
	$(desktop_DATA)

