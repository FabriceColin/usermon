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
#include <string.h>

#include <gtk/gtk.h>
#include <libxfce4ui/libxfce4ui.h>
#include <libxfce4panel/libxfce4panel.h>

#include "usermon.h"
#include "usermon-dialogs.h"

/* the website url */
#define PLUGIN_WEBSITE "https://github.com/FabriceColin/usermon"

static void xfce_usermon_configure_response(GtkWidget * dialog,
					    gint response,
					    UserMonitorPlugin * usermon_plugin)
{
	gboolean result;

	if (response == GTK_RESPONSE_HELP) {
		/* show help */
		result =
		    g_spawn_command_line_async("exo-open --launch WebBrowser "
					       PLUGIN_WEBSITE, NULL);

		if (G_UNLIKELY(result == FALSE)) {
			g_warning(_("Unable to open the following URL: %s"),
				  PLUGIN_WEBSITE);
		}
	} else {
		/* remove the dialog data from the plugin */
		g_object_set_data(G_OBJECT(usermon_plugin), "dialog", NULL);

		/* unlock the panel menu */
		xfce_panel_plugin_unblock_menu(XFCE_PANEL_PLUGIN
					       (usermon_plugin));

		/* save the plugin */
		xfce_usermon_save(XFCE_PANEL_PLUGIN(usermon_plugin));

		/* destroy the properties dialog */
		gtk_widget_destroy(dialog);
	}
}

static void xfce_usermon_max_users_count_changed(GtkSpinButton * spin_button,
						 UserMonitorPlugin *
						 usermon_plugin)
{
	usermon_plugin->max_users_count =
	    gtk_spin_button_get_value_as_int(spin_button);
}

static void xfce_usermon_alarm_period_spin_changed(GtkSpinButton * spin_button,
						   UserMonitorPlugin *
						   usermon_plugin)
{
	usermon_plugin->alarm_period =
	    gtk_spin_button_get_value_as_int(spin_button);
}

static GtkWidget *xfce_usermon_create_layout(UserMonitorPlugin * usermon_plugin)
{
	GtkWidget *vbox =
	    gtk_box_new(GTK_ORIENTATION_VERTICAL, DEFAULT_USERMON_PADDING);
	GtkWidget *row1 =
	    gtk_box_new(GTK_ORIENTATION_HORIZONTAL, DEFAULT_USERMON_PADDING);
	GtkWidget *row2 =
	    gtk_box_new(GTK_ORIENTATION_HORIZONTAL, DEFAULT_USERMON_PADDING);
	GtkWidget *max_users_count_label =
	    gtk_label_new(_("Critical Number Of Users"));
	GtkWidget *max_users_count_spin =
	    gtk_spin_button_new_with_range(1, 100, 1);
	GtkWidget *alarm_period_label = gtk_label_new(_("Alarm Period"));
	GtkWidget *alarm_period_spin =
	    gtk_spin_button_new_with_range(5, 3600, 5);
	GtkWidget *alarm_period_label_post = gtk_label_new(_("Seconds"));

	gtk_box_pack_start(GTK_BOX(row1), max_users_count_label, TRUE, FALSE,
			   0);
	gtk_box_pack_start(GTK_BOX(row1), GTK_WIDGET(max_users_count_spin),
			   TRUE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), row1, FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(row2), alarm_period_label, TRUE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(row2), GTK_WIDGET(alarm_period_spin), TRUE,
			   FALSE, 0);
	gtk_box_pack_start(GTK_BOX(row2), alarm_period_label_post, TRUE, FALSE,
			   0);
	gtk_box_pack_start(GTK_BOX(vbox), row2, FALSE, FALSE, 0);

	gtk_spin_button_set_value(GTK_SPIN_BUTTON(max_users_count_spin),
				  (gdouble) usermon_plugin->max_users_count);
	g_signal_connect(G_OBJECT(max_users_count_spin), "value-changed",
			 G_CALLBACK(xfce_usermon_max_users_count_changed),
			 usermon_plugin);

	gtk_spin_button_set_value(GTK_SPIN_BUTTON(alarm_period_spin),
				  (gdouble) usermon_plugin->alarm_period);
	g_signal_connect(G_OBJECT(alarm_period_spin), "value-changed",
			 G_CALLBACK(xfce_usermon_alarm_period_spin_changed),
			 usermon_plugin);

	gtk_widget_show(max_users_count_label);
	gtk_widget_show(max_users_count_spin);
	gtk_widget_show(row1);
	gtk_widget_show(alarm_period_label);
	gtk_widget_show(alarm_period_spin);
	gtk_widget_show(alarm_period_label_post);
	gtk_widget_show(row2);

	return vbox;
}

void xfce_usermon_configure_plugin(XfcePanelPlugin * plugin)
{
	UserMonitorPlugin *usermon_plugin;
	GtkWidget *dialog;
	GtkWidget *vbox;
	GtkWidget *content;

	usermon_plugin = XFCE_USERMON_PLUGIN(plugin);

	/* block the plugin menu */
	xfce_panel_plugin_block_menu(plugin);

	/* create the dialog */
	dialog = xfce_titled_dialog_new_with_buttons(_("User Monitor Plugin"),
						     GTK_WINDOW
						     (gtk_widget_get_toplevel
						      (GTK_WIDGET(plugin))),
						     GTK_DIALOG_DESTROY_WITH_PARENT,
						     "gtk-help",
						     GTK_RESPONSE_HELP,
						     "gtk-close",
						     GTK_RESPONSE_OK, NULL);

	/* populate the dialog */
	vbox = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
	content = xfce_usermon_create_layout(usermon_plugin);
	gtk_box_pack_start(GTK_BOX(vbox), content, TRUE, FALSE,
			   DEFAULT_USERMON_PADDING);

	/* center dialog on the screen */
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);

	/* set dialog icon */
	gtk_window_set_icon_name(GTK_WINDOW(dialog), "usermon");

	/* link the dialog to the plugin, so we can destroy it when the plugin
	 * is closed, but the dialog is still open */
	g_object_set_data(G_OBJECT(plugin), "dialog", dialog);

	/* connect the reponse signal to the dialog */
	g_signal_connect(G_OBJECT(dialog), "response",
			 G_CALLBACK(xfce_usermon_configure_response),
			 usermon_plugin);

	/* show the entire dialog */
	gtk_widget_show(content);
	gtk_widget_show(vbox);
	gtk_widget_show(dialog);
}

void xfce_usermon_show_about(XfcePanelPlugin * plugin)
{
	/* about dialog code. you can use the GtkAboutDialog
	 * or the XfceAboutInfo widget */
	GdkPixbuf *icon;

	const gchar *auth[] = {
		"Fabrice Colin <fabrice.colin@gmail.com>",
		NULL
	};

	icon = xfce_panel_pixbuf_from_source(PACKAGE_NAME, NULL, 48);
	gtk_show_about_dialog(NULL, "logo", icon, "license",
			      xfce_get_license_text(XFCE_LICENSE_TEXT_GPL),
			      "version", PACKAGE_VERSION, "program-name",
			      PACKAGE_NAME, "comments",
			      _("User monitor plugin"), "website",
			      PLUGIN_WEBSITE, "copyright",
			      _("Copyright \xc2\xa9 2003-2020 Fabrice Colin"),
			      "authors", auth, NULL);

	if (icon) {
		g_object_unref(G_OBJECT(icon));
	}
}
