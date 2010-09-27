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

#include <config.h>
#include <glib/gi18n-lib.h>
#include <gnome-keyring.h>
#include <gtk/gtk.h>
#include "bisho-pane-username.h"

struct _BishoPaneUsernamePrivate {
  ServiceInfo *info; /* cached to speed access */
  SwClientService *service;
  gboolean started; /* so we don't show logged in banners on startup */
  gboolean can_verify;
  gboolean with_password;
  GtkWidget *table;
  GtkWidget *button;
  GtkWidget *logout_button;
  GtkWidget *username_e, *password_e;
  guint32 current_id; /* The keyring item ID of the current password */
};

enum {
  PROP_0,
  PROP_WITH_PASSWORD
};

#define GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), BISHO_TYPE_PANE_USERNAME, BishoPaneUsernamePrivate))
G_DEFINE_TYPE (BishoPaneUsername, bisho_pane_username, BISHO_TYPE_PANE);

static void
add_banner (BishoPaneUsername *pane, gboolean success)
{
  char *message;

  /* Don't do success banners if we've not fully started */
  if (success && !pane->priv->started)
    return;

  if (success) {
    message = g_strdup_printf (_("Log in succeeded. "
                                 "You'll see new things from %s in a couple of minutes."),
                               pane->priv->info->display_name);
  } else {
    message = g_strdup_printf (_("Sorry, we can't log into %s."),
                               pane->priv->info->display_name);
  }

  bisho_pane_set_banner (BISHO_PANE (pane), message);
  g_free (message);
}

static void
keyring_op_done_cb (GnomeKeyringResult result,
                    guint              new_id,
                    gpointer           user_data)
{
  BishoPaneUsername *pane = BISHO_PANE_USERNAME (user_data);

  switch (result) {
  case GNOME_KEYRING_RESULT_OK:
    pane->priv->current_id = new_id;
    sw_client_service_credentials_updated (pane->priv->service);
    break;
  default:
    add_banner (pane, FALSE);
    g_warning (G_STRLOC ": Error setting keyring: %s", gnome_keyring_result_to_message (result));
    break;
  }
}

static void
on_login_clicked (GtkButton *button, gpointer user_data)
{
  BishoPaneUsername *pane = BISHO_PANE_USERNAME (user_data);
  BishoPaneUsernamePrivate *priv = pane->priv;
  const char *username, *password;

  username = gtk_entry_get_text (GTK_ENTRY (priv->username_e));
  if (priv->with_password)
    password = gtk_entry_get_text (GTK_ENTRY (priv->password_e));
  else
    password = "";

  /* Wipe the old data because we only support -- at the moment -- a single user */
  g_debug ("deleting id %u", priv->current_id);
  gnome_keyring_item_delete_sync (NULL, priv->current_id);
  priv->current_id = 0;

  gnome_keyring_set_network_password  (NULL,
                                       username,
                                       NULL,
                                       priv->info->auth.password.server,
                                       NULL, NULL, NULL, 0, password,
                                       keyring_op_done_cb, pane, NULL);

  /* If we are not watching for the verify signal, show the banner now */
  if (!pane->priv->can_verify) {
    add_banner (pane, TRUE);
  }
}

static void
on_entry_activated (GtkEntry *entry, gpointer user_data)
{
  BishoPaneUsername *pane = BISHO_PANE_USERNAME (user_data);

  gtk_widget_child_focus (GTK_WIDGET (pane), GTK_DIR_TAB_FORWARD);
  /* TODO: examine the focus chain and if the next widget is the button,
     activate it */
}

static void
entries_have_content (GtkWidget *widget, gpointer user_data)
{
  gboolean *has_content = user_data;

  if (!GTK_IS_ENTRY (widget))
    return;

  if (gtk_entry_get_text_length (GTK_ENTRY (widget)) > 0)
    *has_content = TRUE;
}

static void
on_entry_changed (GtkEditable *editable, gpointer user_data)
{
  BishoPaneUsername *pane = BISHO_PANE_USERNAME (user_data);
  gboolean has_content = FALSE;

  gtk_container_foreach (GTK_CONTAINER (pane->priv->table),
                         entries_have_content, &has_content);

  gtk_widget_set_sensitive (pane->priv->logout_button, has_content);
}

static void
remove_done_cb (GnomeKeyringResult result,
                gpointer           user_data)
{
  BishoPaneUsername *pane = BISHO_PANE_USERNAME (user_data);

  switch (result) {
  case GNOME_KEYRING_RESULT_OK:
  case GNOME_KEYRING_RESULT_NO_MATCH:
    sw_client_service_credentials_updated (pane->priv->service);
    break;
  default:
    g_warning (G_STRLOC ": Error from keyring: %s", gnome_keyring_result_to_message (result));
    break;
  }
}

static void
on_logout_clicked (GtkButton *button, gpointer user_data)
{
  BishoPaneUsername *pane = BISHO_PANE_USERNAME (user_data);
  BishoPaneUsernamePrivate *priv = pane->priv;
  char *message;

  gtk_entry_set_text (GTK_ENTRY (priv->username_e), "");
  if (priv->with_password)
    gtk_entry_set_text (GTK_ENTRY (priv->password_e), "");

  priv->current_id = 0;
  gnome_keyring_item_delete (NULL, priv->current_id, remove_done_cb, pane, NULL);

  message = g_strdup_printf (_("Log out succeeded. "
                               "All trace of %s has been removed from your computer."),
                             priv->info->display_name);
  bisho_pane_set_banner (BISHO_PANE (pane), message);
  g_free (message);
}

static void
on_caps_changed (SwClientService *service, const char **caps, gpointer user_data)
{
  BishoPaneUsername *u_pane = BISHO_PANE_USERNAME (user_data);
  gboolean configured = sw_client_service_has_cap (caps, IS_CONFIGURED);
  gboolean valid = sw_client_service_has_cap (caps, CREDENTIALS_VALID);
  gboolean invalid = sw_client_service_has_cap (caps, CREDENTIALS_INVALID);

  if (configured) {
    if (valid) {
      gtk_button_set_label (GTK_BUTTON (u_pane->priv->button), _("Update"));
      add_banner (u_pane, TRUE);
    } else if (invalid) {
      gtk_button_set_label (GTK_BUTTON (u_pane->priv->button), _("Log in"));
      add_banner (u_pane, FALSE);
    }
  } else {
    gtk_button_set_label (GTK_BUTTON (u_pane->priv->button), _("Log in"));
  }
}

static void
got_dynamic_caps_cb (SwClientService  *service,
                     const char      **caps,
                     const GError     *error,
                     gpointer          user_data)
{
  BishoPaneUsername *pane = BISHO_PANE_USERNAME (user_data);

  if (error) {
    g_message ("Cannot get dynamic caps: %s", error->message);
    return;
  }

  on_caps_changed (service, caps, pane);

  pane->priv->started = TRUE;
}

static void
got_static_caps_cb (SwClientService  *service,
                     const char      **caps,
                     const GError     *error,
                     gpointer          user_data)
{
  BishoPaneUsername *pane = BISHO_PANE_USERNAME (user_data);

  if (error) {
    g_message ("Cannot get static caps: %s", error->message);
    return;
  }

  if (sw_client_service_has_cap (caps, CAN_VERIFY_CREDENTIALS)) {
    pane->priv->can_verify = TRUE;
    g_signal_connect (pane->priv->service, "capabilities-changed", G_CALLBACK (on_caps_changed), pane);
    sw_client_service_get_dynamic_capabilities (pane->priv->service, got_dynamic_caps_cb, pane);
  }
}

static void
bisho_pane_username_init (BishoPaneUsername *self)
{
  GtkWidget *hbox, *vbox, *align, *vbox2, *image;

  self->priv = GET_PRIVATE (self);
  self->priv->started = FALSE;
  self->priv->can_verify = FALSE;
  self->priv->with_password = TRUE;

  hbox = gtk_hbox_new (FALSE, 6);
  gtk_widget_show (hbox);
  gtk_container_add (GTK_CONTAINER (BISHO_PANE (self)->content), hbox);

  vbox = gtk_vbox_new (FALSE, 6);
  gtk_widget_show (vbox);
  gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);

  self->priv->table = gtk_table_new (0, 0, FALSE);
  g_object_set (self->priv->table,
                "row-spacing", 6,
                "column-spacing", 6,
                NULL);
  gtk_widget_show (self->priv->table);
  gtk_container_add (GTK_CONTAINER (vbox), self->priv->table);

  align = gtk_alignment_new (1, 0, 0, 0);
  gtk_widget_show (align);
  gtk_box_pack_start (GTK_BOX (vbox), align, FALSE, FALSE, 0);

  self->priv->button = gtk_button_new_with_label (_("Log in"));
  g_signal_connect (self->priv->button, "clicked", G_CALLBACK (on_login_clicked), self);
  gtk_widget_show (self->priv->button);
  gtk_container_add (GTK_CONTAINER (align), self->priv->button);

  vbox2 = gtk_vbox_new (FALSE, 6);
  gtk_widget_show (vbox2);
  gtk_box_pack_start (GTK_BOX (hbox), vbox2, FALSE, FALSE, 0);

  self->priv->logout_button = gtk_button_new ();
  g_signal_connect (self->priv->logout_button, "clicked", G_CALLBACK (on_logout_clicked), self);
  image = gtk_image_new_from_stock (GTK_STOCK_CLEAR, GTK_ICON_SIZE_BUTTON);
  gtk_widget_show (image);
  gtk_container_add (GTK_CONTAINER (self->priv->logout_button), image);
  gtk_widget_show (self->priv->logout_button);
  gtk_box_pack_start (GTK_BOX (vbox2), self->priv->logout_button, FALSE, FALSE, 0);
}

static void
found_password_cb (GnomeKeyringResult  result,
                   GList              *list,
                   gpointer            user_data)
{
  BishoPaneUsername *pane = BISHO_PANE_USERNAME (user_data);

  if (result == GNOME_KEYRING_RESULT_OK && list != NULL) {
    GnomeKeyringNetworkPasswordData *data = list->data;

    pane->priv->current_id = data->item_id;

    gtk_entry_set_text (GTK_ENTRY (pane->priv->username_e), data->user);
    if (pane->priv->with_password)
      gtk_entry_set_text (GTK_ENTRY (pane->priv->password_e), data->password);
  }
}

static void
bisho_pane_username_constructed (GObject *object)
{
  BishoPaneUsername *u_pane = (BishoPaneUsername*)object;
  BishoPaneUsernamePrivate *priv = u_pane->priv;
  BishoPane *pane = (BishoPane *)u_pane;
  GtkWidget *label, *entry;

  /* Get a local pointer to the ServiceInfo for convenience */
  priv->info = pane->info;

  /* Get the static caps so we know how to handle credential validation */
  u_pane->priv->service = sw_client_get_service (pane->socialweb, priv->info->name);
  sw_client_service_get_static_capabilities (u_pane->priv->service, got_static_caps_cb, u_pane);

  /* The username widgets */
  label = gtk_label_new (_("Username:"));
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (priv->table), label,
                    0, 1, 0, 1, GTK_FILL, GTK_FILL, 0, 0);

  priv->username_e = entry = gtk_entry_new ();
  gtk_entry_set_width_chars (GTK_ENTRY (entry), 30);
  g_signal_connect (entry, "activate", G_CALLBACK (on_entry_activated), pane);
  g_signal_connect (entry, "changed", G_CALLBACK (on_entry_changed), pane);
  gtk_widget_show (entry);
  gtk_table_attach (GTK_TABLE (priv->table), entry,
                    1, 2, 0, 1, GTK_FILL, GTK_FILL, 0, 0);

  /* The password widgets */
  if (priv->with_password) {
    label = gtk_label_new (_("Password:"));
    gtk_widget_show (label);
    gtk_table_attach (GTK_TABLE (priv->table), label,
                      0, 1, 1, 2, GTK_FILL, GTK_FILL, 0, 0);

    priv->password_e = entry = gtk_entry_new ();
    gtk_entry_set_visibility (GTK_ENTRY (entry), FALSE);
    gtk_entry_set_width_chars (GTK_ENTRY (entry), 30);
    g_signal_connect (entry, "activate", G_CALLBACK (on_entry_activated), pane);
    g_signal_connect (entry, "changed", G_CALLBACK (on_entry_changed), pane);
    gtk_widget_show (entry);
    gtk_table_attach (GTK_TABLE (priv->table), entry,
                      1, 2, 1, 2, GTK_FILL, GTK_FILL, 0, 0);
  }

  /* Now fetch the username/password */
  gnome_keyring_find_network_password (NULL, NULL,
                                       priv->info->auth.password.server,
                                       NULL, NULL, NULL, 0,
                                       found_password_cb, pane, NULL);
}

static void
bisho_pane_username_get_property (GObject *object, guint property_id,
                                  GValue *value, GParamSpec *pspec)
{
  BishoPaneUsername *pane = BISHO_PANE_USERNAME (object);

  switch (property_id) {
  case PROP_WITH_PASSWORD:
    g_value_set_boolean (value, pane->priv->with_password);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
bisho_pane_username_set_property (GObject *object, guint property_id,
                                  const GValue *value, GParamSpec *pspec)
{
  BishoPaneUsername *pane = BISHO_PANE_USERNAME (object);

  switch (property_id) {
  case PROP_WITH_PASSWORD:
    pane->priv->with_password = g_value_get_boolean (value);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
bisho_pane_username_class_init (BishoPaneUsernameClass *klass)
{
  GObjectClass *o_class = G_OBJECT_CLASS (klass);
  GParamSpec *pspec;

  g_type_class_add_private (klass, sizeof (BishoPaneUsernamePrivate));

  o_class->constructed = bisho_pane_username_constructed;
  o_class->get_property = bisho_pane_username_get_property;
  o_class->set_property = bisho_pane_username_set_property;

  pspec = g_param_spec_boolean ("with-password", "with-password", "with-password",
                                TRUE,
                                G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_property (o_class, PROP_WITH_PASSWORD, pspec);
}

GtkWidget *
bisho_pane_username_new (ServiceInfo *info, gboolean with_password)
{
  g_assert (info);

  return g_object_new (BISHO_TYPE_PANE_USERNAME,
                       "service", info,
                       "with-password", with_password,
                       NULL);
}
