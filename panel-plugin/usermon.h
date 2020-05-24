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

#ifndef __USER_MONITOR_H__
#define __USER_MONITOR_H__

#include <stdio.h>
#include <time.h>

G_BEGIN_DECLS typedef struct {
	XfcePanelPluginClass __parent__;
} UserMonitorPluginClass;

typedef struct {
	XfcePanelPlugin __parent__;

	FILE *log_file;

	/* panel widgets */
	GtkWidget *ebox;
	GtkWidget *hvbox;
	GtkWidget *label;

	/* users list */
	GHashTable *users_list;

	/* settings */
	gchar *user_name;
	guint max_users_count;
	guint users_count;
	guint alarm_period;
	time_t start_time;
	time_t last_alarm_time;
} UserMonitorPlugin;

#define XFCE_TYPE_USERMON_PLUGIN    (user_monitor_get_type ())
#define XFCE_USERMON_PLUGIN(obj)    (G_TYPE_CHECK_INSTANCE_CAST ((obj), XFCE_TYPE_USERMON_PLUGIN, UserMonitorPlugin))
#define XFCE_USERMON_PLUGIN_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), XFCE_TYPE_USERMON_PLUGIN, UserMonitorPluginClass))
#define XFCE_IS_USERMON_PLUGIN(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), XFCE_TYPE_USERMON_PLUGIN))
#define XFCE_IS_USERMON_PLUGIN_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), XFCE_TYPE_USERMON_PLUGIN))
#define XFCE_USERMON_PLUGIN_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), XFCE_TYPE_USERMON_PLUGIN, UserMonitorPluginClass))

GType user_monitor_get_type(void) G_GNUC_CONST;

void user_monitor_register_type(XfcePanelTypeModule * type_module);

void xfce_usermon_save(XfcePanelPlugin * plugin);

G_END_DECLS
#endif				/* !__USER_MONITOR_H__ */
