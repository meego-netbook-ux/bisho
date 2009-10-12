/*
 * Copyright (C) 2009 Intel Corporation.
 *
 * Author: Ross Burton <ross@linux.intel.com>
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <mojito-client/mojito-client.h>
#include "mux-expanding-item.h"
#include "mux-window.h"
#include "bisho-window.h"
#include "bisho-utils.h"
#include "service-info.h"
#include "bisho-pane-oauth.h"
#include "bisho-pane-flickr.h"
#include "bisho-pane-username.h"

struct _BishoWindowPrivate {
  MojitoClient *client;
  GtkWidget *master_box;
  /* Hash of string (identifier) to pane widget */
  GHashTable *panes;
};

#define GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), BISHO_TYPE_WINDOW, BishoWindowPrivate))

G_DEFINE_TYPE (BishoWindow, bisho_window, MUX_TYPE_WINDOW);

static void
construct_ui (BishoWindow *window, const char *service_name)
{
  ServiceInfo *info;
  GtkWidget *expander, *pane;
  GtkBox *box;
  MuxExpandingItem *m;

  g_assert (window);
  g_assert (service_name);

  info = get_info_for_service (service_name);
  if (info == NULL)
    return;

  expander = mux_expanding_item_new ();
  m = MUX_EXPANDING_ITEM (expander);

  bisho_utils_make_exclusive_expander (m);
  if (info->icon) {
    mux_expanding_item_set_icon_from_file (m, info->icon);
  } else {
    mux_expanding_item_set_label (m, info->display_name);
  }

  box = mux_expanding_item_get_content_box (m);
  gtk_container_set_border_width (GTK_CONTAINER (box), 8);
  gtk_box_set_spacing (box, 8);

  switch (info->auth) {
  case AUTH_USERNAME:
    pane = bisho_pane_username_new (info);
    bisho_pane_username_add_entry
      (BISHO_PANE_USERNAME (pane), _("Username:"), "user");
    gtk_widget_show (pane);
    gtk_box_pack_start (GTK_BOX (box), pane, FALSE, FALSE, 0);
    break;
  case AUTH_USERNAME_PASSWORD:
    pane = bisho_pane_username_new (info);
    bisho_pane_username_add_entry
      (BISHO_PANE_USERNAME (pane), _("Username:"), "user");
    bisho_pane_username_add_entry
      (BISHO_PANE_USERNAME (pane), _("Password:"), "password");
    gtk_widget_show (pane);
    gtk_box_pack_start (GTK_BOX (box), pane, FALSE, FALSE, 0);
    break;
  case AUTH_OAUTH:
    pane = bisho_pane_oauth_new (window->priv->client, info);
    gtk_widget_show (pane);
    gtk_box_pack_start (GTK_BOX (box), pane, FALSE, FALSE, 0);
    g_hash_table_insert (window->priv->panes, info->name, pane);
    break;
  case AUTH_FLICKR:
    pane = bisho_pane_flickr_new (info);
    gtk_widget_show (pane);
    gtk_box_pack_start (GTK_BOX (box), pane, FALSE, FALSE, 0);
    g_hash_table_insert (window->priv->panes, info->name, pane);
    break;
  case AUTH_INVALID:
    /* Should never see this, so ignore it */
    break;
  }

  gtk_widget_show_all (expander);
  gtk_box_pack_start (GTK_BOX (window->priv->master_box), expander, FALSE, FALSE, 0);
}

static void
client_get_services_cb (MojitoClient *client,
                        const GList        *services,
                        gpointer      userdata)
{
  BishoWindow *window = BISHO_WINDOW (userdata);
  const GList *l;

  for (l = services; l; l = l->next) {
    construct_ui (window, l->data);
  }
}

static void
bisho_window_class_init (BishoWindowClass *klass)
{
  g_type_class_add_private (klass, sizeof (BishoWindowPrivate));
}

static void
bisho_window_init (BishoWindow *self)
{
  GtkWidget *label;

  self->priv = GET_PRIVATE (self);

  gtk_window_set_title (GTK_WINDOW (self), "");
  gtk_window_set_icon_name (GTK_WINDOW (self), "bisho");

  self->priv->master_box = gtk_vbox_new (FALSE, 8);
  gtk_container_set_border_width (GTK_CONTAINER (self->priv->master_box), 8);
  gtk_widget_show (self->priv->master_box);
  gtk_container_add (GTK_CONTAINER (self), self->priv->master_box);

  /* Please don't translate 'myzone' */
  label = gtk_label_new (_("Set up your social networks to see new updates in myzone and the status panel."));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (self->priv->master_box), label, FALSE, FALSE, 0);

  self->priv->panes = g_hash_table_new (g_str_hash, g_str_equal);

  self->priv->client = mojito_client_new ();
  /* TODO move to a separate populate() function? */
  mojito_client_get_services (self->priv->client, client_get_services_cb, self);
}

GtkWidget *
bisho_window_new (void)
{
  return g_object_new (BISHO_TYPE_WINDOW, NULL);
}

void
bisho_window_callback (BishoWindow *window, const char *id, GHashTable *params)
{
  BishoPane *pane;

  pane = g_hash_table_lookup (window->priv->panes, id);
  if (pane)
    bisho_pane_continue_auth (pane, params);
}

MojitoClient *
bisho_window_get_mojito (BishoWindow *window)
{
  return window->priv->client;
}
