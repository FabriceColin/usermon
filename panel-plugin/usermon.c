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
#define DEFAULT_MAX_USERS_COUNT	2
#define DEFAULT_USERS_COUNT	1
#define DEFAULT_ALARM_PERIOD	5

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
	gchar *file_name;

	g_debug("xfce_usermon_read");

	/* record the current user */
	if ((passwd != NULL) && (passwd->pw_name != NULL)) {
		usermon_plugin->user_name = g_strdup(passwd->pw_name);
		g_hash_table_insert(usermon_plugin->users_list,
				    usermon_plugin->user_name,
				    usermon_plugin->user_name);
		usermon_plugin->users_count = 1;
	}

	/* get the plugin config file location */
	file_name =
	    xfce_panel_plugin_save_location(XFCE_PANEL_PLUGIN(usermon_plugin),
					    TRUE);

	if (G_LIKELY(file_name != NULL)) {
		g_debug("Opening %s", file_name);
		/* open the config file, readonly */
		rc = xfce_rc_simple_open(file_name, TRUE);

		/* cleanup */
		g_free(file_name);

		if (G_LIKELY(rc != NULL)) {
			/* read the settings */
			usermon_plugin->max_users_count =
			    xfce_rc_read_int_entry(rc, "max_users_count",
						   DEFAULT_MAX_USERS_COUNT);
			usermon_plugin->alarm_period =
			    xfce_rc_read_int_entry(rc, "alarm_period",
						   DEFAULT_ALARM_PERIOD);

			/* cleanup */
			xfce_rc_close(rc);
		}
	}
}

static void user_monitor_init(UserMonitorPlugin * usermon_plugin)
{
	GtkOrientation orientation;

	/* make only g_error critical */
	g_log_set_always_fatal(G_LOG_LEVEL_ERROR);

	usermon_plugin->log_file = NULL;
	usermon_plugin->ebox = NULL;
	usermon_plugin->hvbox = NULL;
	usermon_plugin->label = NULL;
	usermon_plugin->users_list = NULL;
	usermon_plugin->user_name = NULL;
	usermon_plugin->max_users_count = DEFAULT_MAX_USERS_COUNT;
	usermon_plugin->users_count = DEFAULT_USERS_COUNT;
	usermon_plugin->alarm_period = DEFAULT_ALARM_PERIOD;
	usermon_plugin->start_time = time(NULL);
	usermon_plugin->last_alarm_time = 0;

	/* get the current orientation */
	orientation =
	    xfce_panel_plugin_get_orientation(XFCE_PANEL_PLUGIN
					      (usermon_plugin));

	/* create panel widgets */
	usermon_plugin->ebox = gtk_event_box_new();
	gtk_widget_show(usermon_plugin->ebox);

	usermon_plugin->hvbox = gtk_box_new(orientation, 2);
	gtk_widget_show(usermon_plugin->hvbox);
	gtk_container_add(GTK_CONTAINER(usermon_plugin->ebox),
			  usermon_plugin->hvbox);

	usermon_plugin->label = gtk_label_new(_("1 User"));
	gtk_widget_show(usermon_plugin->label);
	gtk_box_pack_start(GTK_BOX(usermon_plugin->hvbox),
			   usermon_plugin->label, FALSE, FALSE,
			   DEFAULT_USERMON_PADDING);

	/* create the users list */
	usermon_plugin->users_list = g_hash_table_new(g_str_hash, g_str_equal);

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

	/* destroy the users list */
	g_hash_table_destroy(usermon_plugin->users_list);

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
		xfce_rc_write_int_entry(rc, "alarm_period",
					usermon_plugin->alarm_period);

		/* close the rc file */
		xfce_rc_close(rc);
		g_debug("Saved settings");
	}
}

static void xfce_usermon_notify_for_new_user(gpointer key,
					     gpointer value, gpointer user_data)
{
	UserMonitorPlugin *usermon_plugin;
	NotifyUrgency urgency = NOTIFY_URGENCY_NORMAL;
	NotifyNotification *notification_popup = NULL;
	gchar *body = g_strdup_printf(_("%s logged in"),
				      (gchar *) key);

	usermon_plugin = XFCE_USERMON_PLUGIN(user_data);
	notification_popup =
	    notify_notification_new(_("User Monitor"),
				    body,
				    PACKAGE_ICON_DIR "/48x48/apps/usermon.png");

	/* display a notification popup */
	if (notification_popup != NULL) {
		if (usermon_plugin->users_count >
		    usermon_plugin->max_users_count) {
			urgency = NOTIFY_URGENCY_CRITICAL;
		}

		notify_notification_set_timeout(notification_popup,
						the_usermon_plugin->alarm_period
						* 1000);
		notify_notification_set_urgency(notification_popup, urgency);
		notify_notification_show(notification_popup, NULL);

		g_object_unref(G_OBJECT(notification_popup));
	}

	g_free(body);
}

static void xfce_usermon_record_user(gpointer key,
				     gpointer value, gpointer user_data)
{
	UserMonitorPlugin *usermon_plugin;

	usermon_plugin = XFCE_USERMON_PLUGIN(user_data);

	if (key != NULL && value != NULL) {
		g_debug("xfce_usermon_record_user %s", (gchar *) key);
		g_hash_table_insert(usermon_plugin->users_list, key, value);
	}
}

static void xfce_usermon_update_users_list(void)
{
	GHashTable *current_users_list = NULL;
	GHashTable *new_users_list = NULL;
	struct utmpx *u = NULL;
	guint users_count = 0;

	g_debug("xfce_usermon_update_users_list");
	if (the_usermon_plugin == NULL) {
		return;
	}
	g_debug("Found %d users so far", the_usermon_plugin->users_count);

	current_users_list = g_hash_table_new(g_str_hash, g_str_equal);
	new_users_list = g_hash_table_new(g_str_hash, g_str_equal);

	g_debug("Current user is %s", the_usermon_plugin->user_name);
	/* rewind to the beginning of utmpx */
	setutxent();
	/* read utmp */
	while ((u = getutxent())) {
		if (u->ut_type != USER_PROCESS) {
			continue;
		}

		g_hash_table_insert(current_users_list, u->ut_user, u->ut_user);

		/* is this a new user ? */
		if (g_hash_table_contains
		    (the_usermon_plugin->users_list, u->ut_user) == FALSE) {
			g_debug("Found new user %s", u->ut_user);
			g_hash_table_insert(new_users_list, u->ut_user,
					    u->ut_user);
		}
	}
	/* close utmpx */
	endutxent();

	/* update the label? */
	users_count = g_hash_table_size(current_users_list);
	if (the_usermon_plugin->users_count != users_count) {
		gtk_widget_destroy(the_usermon_plugin->label);

		/* if utmp is broken for some reason, we may get 0 users */
		if (users_count <= 1) {
			the_usermon_plugin->label = gtk_label_new(_("1 User"));
		} else {
			gchar *label_text =
			    g_strdup_printf(_("%d Users"), users_count);
			the_usermon_plugin->label = gtk_label_new(label_text);
			g_free(label_text);
		}
		gtk_widget_show(the_usermon_plugin->label);
		gtk_box_pack_start(GTK_BOX(the_usermon_plugin->hvbox),
				   the_usermon_plugin->label, FALSE, FALSE,
				   DEFAULT_USERMON_PADDING);
	}
	g_debug("Found %d new users, %d users in total, max is %d",
		g_hash_table_size(new_users_list),
		users_count, the_usermon_plugin->max_users_count);

	/* copy the current users list */
	g_hash_table_remove_all(the_usermon_plugin->users_list);
	g_hash_table_insert(the_usermon_plugin->users_list,
			    the_usermon_plugin->user_name,
			    the_usermon_plugin->user_name);
	if (users_count > 0) {
		g_hash_table_foreach(current_users_list,
				     xfce_usermon_record_user,
				     the_usermon_plugin);
	}
	the_usermon_plugin->users_count =
	    g_hash_table_size(the_usermon_plugin->users_list);
	g_debug("Reset users list to %d", the_usermon_plugin->users_count);

	/* notify for each new user */
	if (g_hash_table_size(new_users_list) > 0) {
		g_hash_table_foreach(new_users_list,
				     xfce_usermon_notify_for_new_user,
				     the_usermon_plugin);

		/* update the last alarm time */
		the_usermon_plugin->last_alarm_time = time(NULL);
	}

	g_hash_table_destroy(current_users_list);
	g_hash_table_destroy(new_users_list);
}

static void xfce_usermon_timer_handler(int sig_num)
{
	struct itimerval new_timer;
	g_debug("xfce_usermon_timer_handler");
	if (sig_num == SIGALRM) {
		xfce_usermon_update_users_list();

		if (getitimer(ITIMER_REAL, &new_timer) == 0) {
			/* rearm the timer? */
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
		usermon_plugin->log_file = fopen(path, "w");
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
#ifdef DEBUG
			prefix = "DEBUG";
			break;
#else
			return;
#endif
		default:
			prefix = "LOG";
			break;
		}

		fprintf(usermon_plugin->log_file, "%-10s %-25s %s\n", prefix,
			domain, message);
		fflush(usermon_plugin->log_file);
	}

	/* print log to stdout */
	if (level & G_LOG_LEVEL_ERROR || level & G_LOG_LEVEL_CRITICAL) {
		g_log_default_handler(domain, level, message, NULL);
	}
}

static void xfce_usermon_construct(XfcePanelPlugin * plugin)
{
	UserMonitorPlugin *usermon_plugin = XFCE_USERMON_PLUGIN(plugin);

	notify_init(GETTEXT_PACKAGE);

	xfce_panel_plugin_menu_show_configure(plugin);
	xfce_panel_plugin_menu_show_about(plugin);

	/* setup transation domain */
	xfce_textdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

	/* log messages to a file */
	g_log_set_default_handler(xfce_usermon_log_handler, plugin);

	/* init theme/icon stuff */
	gtk_icon_theme_append_search_path(gtk_icon_theme_get_default(),
					  PACKAGE_ICON_DIR);

	/* FIXME: switch to xfconf */
	/* read the user settings */
	xfce_usermon_read(usermon_plugin);

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
