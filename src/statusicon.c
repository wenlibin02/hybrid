/***************************************************************************
 *   Copyright (C) 2011 by levin                                           *
 *   levin108@gmail.com                                                    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Suite 500, Boston, MA 02110-1335, USA.            *
 ***************************************************************************/

#include "statusicon.h"
#include "gtkutils.h"
#include "gtkaccount.h"
#include "preference.h"
#include "buddyadd.h"
#include "conv.h"
#include "pref.h"

extern GtkWidget *hybrid_window;
extern GSList *account_list;
extern GSList *conv_list;

HybridStatusIcon *status_icon;

GdkPixbuf *default_icon;

#define HYBRID_BLINK_TIMEOUT 500

static gboolean
hybrid_status_icon_blinker(HybridStatusIcon *status_icon)
{
    fprintf(stderr, "%s\n", __func__);
    struct HybridBlinker *blinker = &status_icon->blinker;
    GtkStatusIcon *icon = status_icon->icon;

    if ((blinker->off = !blinker->off)) {
        gtk_status_icon_set_from_pixbuf(icon, blinker->blank);
    } else {
        gtk_status_icon_set_from_pixbuf(icon, blinker->back);
    }

    return TRUE;
}

static void
hybrid_status_icon_set_blinking(HybridStatusIcon *status_icon, gboolean blink)
{
    fprintf(stderr, "%s, %d\n", __func__, blink);
    struct HybridBlinker *blinker = &status_icon->blinker;
    GtkStatusIcon *icon = status_icon->icon;

    if (blink) {
        if (blinker->timeout)
            return;
        blinker->blank = default_icon;
        g_object_ref(blinker->blank);
        blinker->back = gtk_status_icon_get_pixbuf(icon);
        g_object_ref(blinker->back);
        blinker->timeout =
            gdk_threads_add_timeout(HYBRID_BLINK_TIMEOUT,
                                    (GSourceFunc)hybrid_status_icon_blinker,
                                    status_icon);
    } else {
        if (!blinker->timeout)
            return;
        g_source_remove(blinker->timeout);
        blinker->timeout = 0;
        blinker->off = FALSE;
        gtk_status_icon_set_from_pixbuf(icon, blinker->back);
        g_object_unref(blinker->back);
        blinker->back = NULL;
        g_object_unref(blinker->blank);
        blinker->blank = NULL;
    }
}

/**
 * Callback function of the status icon's activate event.
 */
static void
status_icon_activate_cb(GtkWidget *widget, gpointer user_data)
{

    gtk_window_deiconify(GTK_WINDOW(hybrid_window));

    if (GTK_WIDGET_VISIBLE(hybrid_window)) {

        if (!gtk_window_is_active(GTK_WINDOW(hybrid_window))) {

            gtk_window_present(GTK_WINDOW(hybrid_window));

            return;
        }

        gtk_widget_hide(hybrid_window);

    } else {

        gtk_widget_show(hybrid_window);
    }
}

/**
 * Callback function for the show-buddy-list menu's toggled event.
 */
static void
show_buddy_list_cb(GtkCheckMenuItem *item, gpointer user_data)
{
    gboolean toggled;

    toggled = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(item));

    if (toggled) {

        gtk_widget_show(hybrid_window);

    } else {

        gtk_widget_hide(hybrid_window);
    }
}

/**
 * Callback function for the account menu's activate event.
 */
static void
account_cb(GtkWidget *widget, gpointer user_data)
{
    hybrid_account_panel_create();
}

/**
 * Callback function for the add-buddy menu's activate event.
 */
static void
add_buddy_cb(GtkWidget *widget, gpointer user_data)
{
    hybrid_buddyadd_window_create();
}

/**
 * Callback function for the preference menu's activate event.
 */
static void
preference_cb(GtkWidget *widget, gpointer user_data)
{
    hybrid_pref_create();
}

/**
 * Callback function for the message notify activate event.
 */
static void
notify_cb(GtkWidget *widget, gpointer user_data)
{
    gboolean toggled;

    toggled = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget));

    if (toggled) {

        hybrid_pref_set_boolean("close_notify", TRUE);

    } else {

        hybrid_pref_set_boolean("close_notify", FALSE);
    }

    hybrid_pref_save();
}

/**
 * Callback function for the mute menu's activate event.
 */
static void
mute_cb(GtkWidget *widget, gpointer user_data)
{
    gboolean toggled;

    toggled = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget));

    if (toggled) {

        hybrid_pref_set_boolean("mute", TRUE);

    } else {

        hybrid_pref_set_boolean("mute", FALSE);
    }

    hybrid_pref_save();
}

/**
 * Callback function for the quit menu's activate event.
 */
static void
quit_cb(GtkWidget *widget, gpointer user_data)
{
    gtk_widget_destroy(hybrid_window);
}

/**
 * Callback function of the status icon's popup menu event.
 */
static void
status_icon_popup_cb(GtkWidget *widget, guint button, guint activate_time,
        gpointer user_data)
{
    GtkWidget *menu;
    GtkWidget *menu_item;
    GSList *pos;
    gboolean has_enabled_account = FALSE;

    menu = gtk_menu_new();

    /* show buddy list buddy. */
    menu_item = gtk_check_menu_item_new_with_label(_("Show buddy list"));

    if (GTK_WIDGET_VISIBLE(hybrid_window)) {
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_item), TRUE);
    }

    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);

    g_signal_connect(menu_item, "toggled",
                     G_CALLBACK(show_buddy_list_cb), NULL);

    hybrid_create_menu_seperator(menu);

    /* account menu. */
    hybrid_create_menu(menu, _("Manage Accounts"), NULL, TRUE,
                       G_CALLBACK(account_cb), NULL);

    /* add-buddy menu. */
    menu_item = gtk_image_menu_item_new_from_stock(GTK_STOCK_ADD, NULL);

    for (pos = account_list; pos; pos = pos->next) {
        if (((HybridAccount*)pos->data)->enabled) {
            has_enabled_account = TRUE;
        }
    }

    gtk_widget_set_sensitive(menu_item, has_enabled_account);

    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);

    g_signal_connect(menu_item, "activate",
                     G_CALLBACK(add_buddy_cb), NULL);

    hybrid_create_menu_seperator(menu);

    /* preference menu. */
    menu_item = gtk_image_menu_item_new_from_stock(GTK_STOCK_PREFERENCES, NULL);

    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);

    g_signal_connect(menu_item, "activate",
                     G_CALLBACK(preference_cb), NULL);

    /* mute menu. */
    menu_item = gtk_check_menu_item_new_with_label(_("Mute"));

    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);

    if (hybrid_pref_get_boolean("mute")) {
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_item), TRUE);
    }

    g_signal_connect(menu_item, "toggled",
                     G_CALLBACK(mute_cb), NULL);

    /* notify menu */
    menu_item = gtk_check_menu_item_new_with_label(_("Close Notify"));

    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);

    if (hybrid_pref_get_boolean("close_notify")) {
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_item), TRUE);
    }

    g_signal_connect(menu_item, "toggled",
                     G_CALLBACK(notify_cb), NULL);

    hybrid_create_menu_seperator(menu);

    /* quit menu. */
    menu_item = gtk_image_menu_item_new_from_stock(GTK_STOCK_QUIT, NULL);

    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);

    g_signal_connect(menu_item, "activate",
                     G_CALLBACK(quit_cb), NULL);


    gtk_widget_show_all(menu);

    gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
            button, activate_time);
}

static void
status_icon_msg_cb(GtkWidget *widget, gpointer user_data)
{
    GSList *conv_pos;
    GSList *chat_pos;
    HybridConversation *conv;
    HybridChatWindow *chat;
    gint current_page;

    g_signal_handler_disconnect(G_OBJECT(status_icon->icon),
                status_icon->conn_id);

    gtk_status_icon_set_from_pixbuf(
            GTK_STATUS_ICON(status_icon->icon), default_icon);

//    gtk_status_icon_set_blinking(GTK_STATUS_ICON(status_icon->icon), FALSE);
    hybrid_status_icon_set_blinking(status_icon, FALSE);

    status_icon->conn_id =
        g_signal_connect(G_OBJECT(status_icon->icon), "activate",
                         G_CALLBACK(status_icon_activate_cb), NULL);

    for (conv_pos = conv_list; conv_pos; conv_pos = conv_pos->next) {
        conv = (HybridConversation*)conv_pos->data;

        /* find the current chat panel. */
        current_page = gtk_notebook_current_page(GTK_NOTEBOOK(conv->notebook));

        for (chat_pos = conv->chat_buddies; chat_pos; chat_pos = chat_pos->next) {

            chat = (HybridChatWindow*)chat_pos->data;

            if (chat->unread != 0) {

                gtk_window_present(GTK_WINDOW(conv->window));

                if (current_page == gtk_notebook_page_num(
                            GTK_NOTEBOOK(conv->notebook), chat->vbox)) {
                    chat->unread = 0;
                    hybrid_chat_window_update_tips(chat);
                }
            }
        }
    }
}

void
hybrid_status_icon_init(void)
{
    GdkPixbuf *pixbuf;

    status_icon = g_new0(HybridStatusIcon, 1);

    status_icon->icon = gtk_status_icon_new();

    pixbuf = hybrid_create_round_pixbuf(NULL, 0, 20);

    default_icon = pixbuf;

    gtk_status_icon_set_from_pixbuf(GTK_STATUS_ICON(status_icon->icon), pixbuf);

    gtk_status_icon_set_tooltip(status_icon->icon, "Hybrid");

    status_icon->conn_id =
        g_signal_connect(G_OBJECT(status_icon->icon), "activate",
                         G_CALLBACK(status_icon_activate_cb), NULL);

    g_signal_connect(G_OBJECT(status_icon->icon), "popup-menu",
                     G_CALLBACK(status_icon_popup_cb), NULL);
}

void
hybrid_status_icon_blinking(HybridBuddy *buddy)
{
    GdkPixbuf *pixbuf;

    if (buddy) {

        pixbuf = hybrid_create_round_pixbuf(buddy->icon_data,
                                            buddy->icon_data_length, 20);

        if (!pixbuf) {
            return;
        }

        gtk_status_icon_set_from_pixbuf(
                GTK_STATUS_ICON(status_icon->icon), pixbuf);

//        gtk_status_icon_set_blinking(GTK_STATUS_ICON(status_icon->icon), TRUE);
        hybrid_status_icon_set_blinking(status_icon, TRUE);

        g_object_unref(pixbuf);

        g_signal_handler_disconnect(G_OBJECT(status_icon->icon),
                    status_icon->conn_id);

        status_icon->conn_id =
            g_signal_connect(G_OBJECT(status_icon->icon), "activate",
                             G_CALLBACK(status_icon_msg_cb), NULL);

    } else {
        g_signal_handler_disconnect(G_OBJECT(status_icon->icon),
                    status_icon->conn_id);

        gtk_status_icon_set_from_pixbuf(
                GTK_STATUS_ICON(status_icon->icon), default_icon);

//        gtk_status_icon_set_blinking(GTK_STATUS_ICON(status_icon->icon), FALSE);
        hybrid_status_icon_set_blinking(status_icon, FALSE);

        status_icon->conn_id =
            g_signal_connect(G_OBJECT(status_icon->icon), "activate",
                             G_CALLBACK(status_icon_activate_cb), NULL);
    }
}
