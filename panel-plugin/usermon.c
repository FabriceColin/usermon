/*
 *  Copyright 2003-2020 Fabrice Colin
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include <stdio.h>
#include <unistd.h>
#include <utmpx.h>
#include <sys/time.h>
#include <sys/types.h>
#include <pwd.h>
#include <time.h>

#include <gtk/gtk.h>
#include <libnotify/notify.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfce4panel/libxfce4panel.h>

#include "usermon.h"
#include "usermon-dialogs.h"

/* default settings */
#define DEFAULT_MAX_USERS_COUNT 1
#define DEFAULT_USERS_COUNT 1
#define DEFAULT_IGNORE_ALL_USERS FALSE
#define DEFAULT_ALARM_PERIOD 5

UserMonitorPlugin *the_usermon_plugin = NULL;

/* prototypes */
static void xfce_usermon_construct(XfcePanelPlugin * plugin);
static void xfce_usermon_free(XfcePanelPlugin * plugin);
static gboolean xfce_usermon_size_changed(XfcePanelPlugin * plugin, gint size);
static void xfce_usermon_mode_changed(XfcePanelPlugin * plugin,
				      XfcePanelPluginMode mode);
static void xfce_usermon_set_timer(void);

/* define the plugin */
XFCE_PANEL_DEFINE_PLUGIN(UserMonitorPlugin, user_monitor)
static void user_monitor_class_init(UserMonitorPluginClass * klass)
{
	XfcePanelPluginClass *plugin_class;

	g_debug("user_monitor_class_init");
	plugin_class = XFCE_PANEL_PLUGIN_CLASS(klass);
	plugin_class->construct = xfce_usermon_construct;
	plugin_class->free_data = xfce_usermon_free;
	plugin_class->size_changed = xfce_usermon_size_changed;
	plugin_class->about = xfce_usermon_show_about;
	plugin_class->configure_plugin = xfce_usermon_configure_plugin;
	plugin_class->mode_changed = xfce_usermon_mode_changed;
}

static void xfce_usermon_read(UserMonitorPlugin * usermon_plugin)
{
	XfceRc *rc;
	struct passwd *passwd = getpwuid(geteuid());
	gchar *file;

	g_debug("xfce_usermon_read");
	/* get the plugin config file location */
	file =
	    xfce_panel_plugin_save_location(XFCE_PANEL_PLUGIN(usermon_plugin),
					    TRUE);

	if (G_LIKELY(file != NULL)) {
		/* open the config file, readonly */
		rc = xfce_rc_simple_open(file, TRUE);

		/* cleanup */
		g_free(file);

		if (G_LIKELY(rc != NULL)) {
			/* read the settings */
			usermon_plugin->max_users_count =
			    xfce_rc_read_int_entry(rc, "max_users_count",
						   DEFAULT_MAX_USERS_COUNT);
			usermon_plugin->ignore_all_users =
			    xfce_rc_read_bool_entry(rc, "ignore_all_users",
						    DEFAULT_IGNORE_ALL_USERS);
			usermon_plugin->alarm_period =
			    xfce_rc_read_int_entry(rc, "alarm_period",
						   DEFAULT_ALARM_PERIOD);

			/* cleanup */
			xfce_rc_close(rc);

			/* leave the function, everything went well */
			return;
		}
	}

	/* something went wrong, apply default values */
	g_debug("Applying default settings");

	if ((passwd != NULL) && (passwd->pw_name != NULL)) {
		usermon_plugin->user_name = g_strdup(passwd->pw_name);
	}
}

static void user_monitor_init(UserMonitorPlugin * usermon_plugin)
{
	GtkOrientation orientation;

	g_debug("user_monitor_init");
	/* Indicators print a lot of warnings. By default, "wrapper"
	   makes them critical, so the plugin "crashes" when run as an external
	   plugin but not internal one (loaded by "xfce4-panel" itself).
	   The following lines makes only g_error critical. */
	g_log_set_always_fatal(G_LOG_LEVEL_ERROR);

	usermon_plugin->log_file = NULL;
	usermon_plugin->ebox = NULL;
	usermon_plugin->hvbox = NULL;
	usermon_plugin->label = NULL;
	usermon_plugin->user_name = NULL;
	usermon_plugin->max_users_count = DEFAULT_MAX_USERS_COUNT;
	usermon_plugin->users_count = DEFAULT_USERS_COUNT;
	usermon_plugin->ignore_all_users = DEFAULT_IGNORE_ALL_USERS;
	usermon_plugin->alarm_period = DEFAULT_ALARM_PERIOD;
	usermon_plugin->start_time = time(NULL);
	usermon_plugin->last_alarm_time = 0;

	/* read the user settings */
	xfce_usermon_read(usermon_plugin);

	/* get the current orientation */
	orientation =
	    xfce_panel_plugin_get_orientation(XFCE_PANEL_PLUGIN
					      (usermon_plugin));

	/* create some panel widgets */
	usermon_plugin->ebox = gtk_event_box_new();
	gtk_widget_show(usermon_plugin->ebox);

	usermon_plugin->hvbox = gtk_box_new(orientation, 2);
	gtk_widget_show(usermon_plugin->hvbox);
	gtk_container_add(GTK_CONTAINER(usermon_plugin->ebox),
			  usermon_plugin->hvbox);

	usermon_plugin->label = gtk_label_new(_("1 User"));
	gtk_widget_show(usermon_plugin->label);
	gtk_box_pack_start(GTK_BOX(usermon_plugin->hvbox),
			   usermon_plugin->label, FALSE, FALSE, 0);

	the_usermon_plugin = usermon_plugin;
}

static void xfce_usermon_free(XfcePanelPlugin * plugin)
{
	UserMonitorPlugin *usermon_plugin;
	GtkWidget *dialog;

	g_debug("xfce_usermon_free");
	usermon_plugin = XFCE_USERMON_PLUGIN(plugin);

	/* check if the dialog is still open. if so, destroy it */
	dialog = g_object_get_data(G_OBJECT(plugin), "dialog");
	if (G_UNLIKELY(dialog != NULL))
		gtk_widget_destroy(dialog);

	/* destroy the panel widgets */
	gtk_widget_destroy(usermon_plugin->hvbox);

	/* cleanup the settings */
	if (G_LIKELY(usermon_plugin->user_name != NULL))
		g_free(usermon_plugin->user_name);

	/* free the plugin structure */
	g_slice_free(UserMonitorPlugin, usermon_plugin);
	the_usermon_plugin = NULL;

	notify_uninit();
}

static gboolean xfce_usermon_size_changed(XfcePanelPlugin * plugin, gint size)
{
	g_debug("xfce_usermon_size_changed");

	/* FIXME: handle this */

	/* we handled the orientation */
	return TRUE;
}

static void
xfce_usermon_mode_changed(XfcePanelPlugin * plugin, XfcePanelPluginMode mode)
{
	g_debug("xfce_usermon_mode_changed");

	/* FIXME: handle this */

	xfce_usermon_size_changed(plugin, xfce_panel_plugin_get_size(plugin));
}

void xfce_usermon_save(XfcePanelPlugin * plugin)
{
	UserMonitorPlugin *usermon_plugin;
	XfceRc *rc;
	gchar *file;

	g_debug("xfce_usermon_save");
	usermon_plugin = XFCE_USERMON_PLUGIN(plugin);

	/* get the config file location */
	file = xfce_panel_plugin_save_location(plugin, TRUE);

	if (G_UNLIKELY(file == NULL)) {
		g_debug("Failed to open config file");
		return;
	}

	/* open the config file, read/write */
	rc = xfce_rc_simple_open(file, FALSE);
	g_free(file);

	if (G_LIKELY(rc != NULL)) {
		/* save the settings */
		xfce_rc_write_int_entry(rc, "max_users_count",
					usermon_plugin->max_users_count);
		xfce_rc_write_bool_entry(rc, "ignore_all_users",
					 usermon_plugin->ignore_all_users);
		xfce_rc_write_int_entry(rc, "alarm_period",
					usermon_plugin->alarm_period);

		/* close the rc file */
		xfce_rc_close(rc);
		g_debug("Saved settings");
	}
}

static void xfce_usermon_update_users_list(void)
{
	struct utmpx *u = NULL;
	time_t now = time(NULL);
	int users_count = 0;

	g_debug("xfce_usermon_update_users_list");
	if (the_usermon_plugin == NULL) {
		return;
	}

	g_debug("Current user is %s", the_usermon_plugin->user_name);
	// Rewind to the beginning of utmpx
	setutxent();
	// Read utmp
	while ((u = getutxent())) {
		if (u->ut_type == USER_PROCESS) {
			g_debug("Found user %s", u->ut_user);

			// FIXME: Is this user in the ignore list ?
			if (the_usermon_plugin->ignore_all_users == TRUE) {
				// OK, don't increment the number of users and insert this user in the list
			} else {
				users_count++;
			}
		}
	}
	// Close utmpx
	endutxent();

	if (the_usermon_plugin->users_count != users_count) {
		gchar *label_text;

		/* if utmp is broken for some reason, we may get 0 users */
		if (users_count <= 1) {
			label_text = g_strdup(_("1 User"));
		} else {
			label_text =
			    g_strdup_printf(_("%d Users"), users_count);
		}

		gtk_widget_destroy(the_usermon_plugin->label);
		the_usermon_plugin->label = gtk_label_new(label_text);
		gtk_box_pack_start(GTK_BOX(the_usermon_plugin->hvbox),
				   the_usermon_plugin->label, FALSE, FALSE, 0);
		gtk_widget_show(the_usermon_plugin->label);

		the_usermon_plugin->users_count = users_count;
	}
	// Make sure this flag is reset
	the_usermon_plugin->ignore_all_users = FALSE;
	g_debug("Found %d users in total, max is %d", users_count,
		the_usermon_plugin->max_users_count);

	// If the maximum number of users has been reached, trigger the alarm.
	// Unless we have already done so less than a period ago or that the applet
	// was started less than 5 seconds ago.
	if ((users_count >= the_usermon_plugin->max_users_count) &&
	    (the_usermon_plugin->last_alarm_time +
	     the_usermon_plugin->alarm_period <= now)
	    && (the_usermon_plugin->start_time + DEFAULT_ALARM_PERIOD <= now)) {
		gchar *body =
		    g_strdup_printf(_("%d users are currently logged in"),
				    users_count);
		NotifyNotification *notification_popup =
		    notify_notification_new("User Monitor",
					    body,
					    PACKAGE_ICON_DIR
					    "/48x48/usermon.png");

		// Display a notification popup
		if (notification_popup != NULL) {
			notify_notification_set_timeout(notification_popup,
							the_usermon_plugin->
							alarm_period * 1000);
			notify_notification_set_urgency(notification_popup,
							NOTIFY_URGENCY_NORMAL);
			notify_notification_show(notification_popup, NULL);

			g_object_unref(G_OBJECT(notification_popup));
		}
		g_free(body);

		// Update the last alarm time
		the_usermon_plugin->last_alarm_time = time(NULL);
	}
}

static void xfce_usermon_timer_handler(int sig_num)
{
	struct itimerval new_timer;
	g_debug("xfce_usermon_timer_handler");
	if (sig_num == SIGALRM) {
		xfce_usermon_update_users_list();
		if (getitimer(ITIMER_REAL, &new_timer) == 0) {
			g_debug("Next timer in %ld:%ld seconds, %ld:%ld",
				new_timer.it_interval.tv_sec,
				new_timer.it_interval.tv_usec,
				new_timer.it_value.tv_sec,
				new_timer.it_value.tv_usec);
			if (new_timer.it_value.tv_sec == 0
			    && new_timer.it_value.tv_usec == 0) {
				xfce_usermon_set_timer();
			}
		}
	}
}

static void xfce_usermon_set_timer(void)
{
	struct itimerval new_timer;
	struct sigaction new_action;

	g_debug("xfce_usermon_set_timer");
	memset(&new_action, 0, sizeof(new_action));
	new_action.sa_handler = xfce_usermon_timer_handler;
	sigemptyset(&new_action.sa_mask);
	new_action.sa_flags = 0;

	sigaction(SIGALRM, NULL, NULL);
	if (sigaction(SIGALRM, &new_action, NULL) != 0) {
		g_debug("Failed to set alarm");
		return;
	}

	new_timer.it_value.tv_sec = DEFAULT_ALARM_PERIOD;
	new_timer.it_value.tv_usec = 0;
	new_timer.it_interval.tv_sec = DEFAULT_ALARM_PERIOD;
	new_timer.it_interval.tv_usec = 0;

	if (setitimer(ITIMER_REAL, &new_timer, NULL) != 0) {
		g_debug("Failed to set timer");
	}
}

static void
xfce_usermon_log_handler(const gchar * domain,
			 GLogLevelFlags level,
			 const gchar * message, gpointer data)
{
	UserMonitorPlugin *usermon_plugin = XFCE_USERMON_PLUGIN(data);
	gchar *path;
	const gchar *prefix;

	if (usermon_plugin->log_file == NULL) {
		g_mkdir_with_parents(g_get_user_cache_dir(), 0755);
		path =
		    g_build_filename(g_get_user_cache_dir(),
				     "xfce4-usermon_plugin-plugin.log", NULL);
#ifdef DEBUG
		usermon_plugin->log_file = freopen(path, "w", stderr);
#else
		usermon_plugin->log_file = fopen(path, "w");
#endif
		g_free(path);
	}

	if (usermon_plugin->log_file) {
		switch (level & G_LOG_LEVEL_MASK) {
		case G_LOG_LEVEL_ERROR:
			prefix = "ERROR";
			break;
		case G_LOG_LEVEL_CRITICAL:
			prefix = "CRITICAL";
			break;
		case G_LOG_LEVEL_WARNING:
			prefix = "WARNING";
			break;
		case G_LOG_LEVEL_MESSAGE:
			prefix = "MESSAGE";
			break;
		case G_LOG_LEVEL_INFO:
			prefix = "INFO";
			break;
		case G_LOG_LEVEL_DEBUG:
			prefix = "DEBUG";
			break;
		default:
			prefix = "LOG";
			break;
		}

		fprintf(usermon_plugin->log_file, "%-10s %-25s %s\n", prefix,
			domain, message);
		fflush(usermon_plugin->log_file);
	}

	/* print log to the stdout */
	if (level & G_LOG_LEVEL_ERROR || level & G_LOG_LEVEL_CRITICAL)
		g_log_default_handler(domain, level, message, NULL);
}

static void xfce_usermon_construct(XfcePanelPlugin * plugin)
{
	UserMonitorPlugin *usermon_plugin = XFCE_USERMON_PLUGIN(plugin);

	g_debug("xfce_usermon_construct");
	notify_init(GETTEXT_PACKAGE);

	xfce_panel_plugin_menu_show_configure(plugin);
	xfce_panel_plugin_menu_show_about(plugin);

	/* setup transation domain */
	xfce_textdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

	/* log messages to a file */
	g_log_set_default_handler(xfce_usermon_log_handler, plugin);

	/* Init some theme/icon stuff */
	gtk_icon_theme_append_search_path(gtk_icon_theme_get_default(),
					  PACKAGE_ICON_DIR);

	/* FIXME: initialize xfconf */

	/* add the ebox to the panel */
	gtk_container_add(GTK_CONTAINER(plugin), usermon_plugin->ebox);

	/* show the panel's right-click menu on this ebox */
	xfce_panel_plugin_add_action_widget(plugin, usermon_plugin->ebox);

	/* connect plugin signals */
	g_signal_connect(G_OBJECT(plugin), "save",
			 G_CALLBACK(xfce_usermon_save), usermon_plugin);

	/* set the timer */
	xfce_usermon_set_timer();
}
