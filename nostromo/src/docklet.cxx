/*
    Nostromo_n50 configuration tools to support Belkin's Nostromo n50
    Copyright (C) 2003 Paul Bohme and others

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/**
 * @file docklet.cxx
 * @todo Remove hardwired icon path name.
 **/

#include <gtk/gtk.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include "nost_data.h"
#include "eggtrayicon.h"

static EggTrayIcon *docklet = NULL;
static GtkWidget *image = NULL;

/**
 * Callback hit from the menu to reload configs.
 **/
static void 
reload_data(GtkWidget *widget, gpointer data)
{
    kill(getpid(), SIGHUP);
}

/**
 * Callback hit from the menu to shut the app down.
 * This is ugly, but works.
 **/
static void 
exit_app(GtkWidget *widget, gpointer data)
{
    exit(0);
}

extern nost_config_data* cfg;
extern nost_data* all_cfg;

/**
 * Callback to set the current configuration from the menu.
 **/
static void 
set_cfg(void*, void* n)
{
    cfg = &all_cfg->configs[(int)(long long)n];
}

/**
 * Display a menu when the right mouse button is clicked.
 **/
static void 
docklet_menu()
{
    static GtkWidget* menu = NULL;
    static GtkWidget* sub_menu = NULL;
    GtkWidget* entry;
    int n;

    if(menu) {
        gtk_widget_destroy(menu);
    }

    menu = gtk_menu_new();
    sub_menu = gtk_menu_new();

    for(n = 0; n < all_cfg->num_configs; n++) {
        entry = gtk_menu_item_new_with_label(all_cfg->configs[n].name);
        g_signal_connect(G_OBJECT(entry), "activate", G_CALLBACK(set_cfg), (void*)n);
        gtk_menu_shell_append(GTK_MENU_SHELL(sub_menu), entry);
    }


    entry = gtk_menu_item_new_with_label(cfg->name);

    gtk_menu_item_set_submenu(GTK_MENU_ITEM(entry), sub_menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), entry);

    gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());

    entry = gtk_menu_item_new_with_label("Reload Configs");
    g_signal_connect(G_OBJECT(entry), "activate", G_CALLBACK(reload_data), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), entry);

    entry = gtk_menu_item_new_with_label("Exit");
    g_signal_connect(G_OBJECT(entry), "activate", G_CALLBACK(exit_app), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), entry);

    gtk_widget_show_all(menu);
	gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, 0, gtk_get_current_event_time());
}

/**
 * Callback to catch the right button being clicked
 * and invoke the menu.
 **/
static void
daemon_clicked_cb(GtkWidget *button, GdkEventButton *event, void *data)
{
	if (event->type == GDK_BUTTON_PRESS) {
        if(event->button == 3) {
            docklet_menu();
        }
    }
}

/**
 * Set up our systray icon.
 * @bug Need a better way to find our icon.
 **/
void 
daemon_create()
{
	GtkWidget *box;

	docklet = egg_tray_icon_new("Nostromo n5x");

	box = gtk_event_box_new();
	image = gtk_image_new_from_file(IMG_PATH "/n50_tray.png");

	g_signal_connect(G_OBJECT(box), "button-press-event", G_CALLBACK(daemon_clicked_cb), NULL);

	gtk_container_add(GTK_CONTAINER(box), image);
	gtk_container_add(GTK_CONTAINER(docklet), box);
	gtk_widget_show_all(GTK_WIDGET(docklet));

	/* ref the docklet before we bandy it about the place */
	g_object_ref(G_OBJECT(docklet));
}

