/*
 * Carrick - a connection panel for the Moblin Netbook
 * Copyright (C) 2009 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 * Written by - Joshua Lock <josh@linux.intel.com>
 *
 */

#include "carrick-pane.h"

#include <config.h>
#include <nbtk/nbtk-gtk.h>
#include <glib/gi18n.h>
#include <dbus/dbus-glib.h>

#include "connman-marshal.h"
#include "carrick-list.h"
#include "carrick-service-item.h"
#include "carrick-icon-factory.h"
#include "carrick-notification-manager.h"
#include "carrick-network-model.h"

G_DEFINE_TYPE (CarrickPane, carrick_pane, GTK_TYPE_TABLE)

#define PANE_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), CARRICK_TYPE_PANE, CarrickPanePrivate))

struct _CarrickPanePrivate
{
  GtkWidget *wifi_switch;
  GtkWidget *wifi_label;
  GtkWidget *ethernet_switch;
  GtkWidget *ethernet_label;
  GtkWidget *threeg_switch;
  GtkWidget *threeg_label;
  GtkWidget *wimax_switch;
  GtkWidget *wimax_label;
  GtkWidget *bluetooth_switch;
  GtkWidget *bluetooth_label;
  GtkWidget *offline_mode_switch;
  GtkWidget *service_list;
  GtkWidget *new_conn_button;
  CarrickIconFactory *icon_factory;
  time_t   last_scan;
  gboolean offline_mode;
  gboolean have_daemon;
  gboolean have_wifi;
  gboolean have_ethernet;
  gboolean have_threeg;
  gboolean have_wimax;
  gboolean have_bluetooth;
  gboolean wifi_enabled;
  gboolean ethernet_enabled;
  gboolean threeg_enabled;
  gboolean wimax_enabled;
  gboolean bluetooth_enabled;
  DBusGProxy *manager;
  CarrickNotificationManager *notes;
};

enum {
  PROP_0,
  PROP_ICON_FACTORY,
  PROP_NOTIFICATIONS
};

static void
pane_free_g_value (GValue *val)
{
  g_value_unset (val);
  g_slice_free (GValue, val);
}

static gboolean
_focus_callback (GtkWidget       *widget,
                 GtkDirectionType arg1,
                 gpointer         user_data)
{
  /*
   * Work around for bug #4319:
   * Stop propogating focus events to
   * contained widgets so that we do
   * not put items in the carrick-list
   * in a 'SELECTED' state
   */
  return TRUE;
}

static void
carrick_pane_get_property (GObject    *object,
                           guint       property_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  CarrickPane        *pane = CARRICK_PANE (object);
  CarrickPanePrivate *priv = pane->priv;

  switch (property_id)
    {
    case PROP_ICON_FACTORY:
      g_value_set_object (value, priv->icon_factory);
      break;

    case PROP_NOTIFICATIONS:
      g_value_set_object (value, priv->notes);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
carrick_pane_set_icon_factory (CarrickPane        *pane,
                               CarrickIconFactory *icon_factory)
{
  CarrickPanePrivate *priv = pane->priv;

  priv->icon_factory = icon_factory;
  carrick_list_set_icon_factory (CARRICK_LIST (priv->service_list),
                                 icon_factory);
}

static void
carrick_pane_set_notifications (CarrickPane                *pane,
                                CarrickNotificationManager *notes)
{
  CarrickPanePrivate *priv = pane->priv;

  priv->notes = notes;
  carrick_list_set_notification_manager (CARRICK_LIST (priv->service_list),
                                         notes);
}

static void
carrick_pane_set_property (GObject      *object,
                           guint         property_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  CarrickPane *pane = CARRICK_PANE (object);

  switch (property_id)
    {
    case PROP_ICON_FACTORY:
      carrick_pane_set_icon_factory (
        pane,
        CARRICK_ICON_FACTORY (g_value_get_object (value)));
      break;

    case PROP_NOTIFICATIONS:
      carrick_pane_set_notifications (
        pane,
        CARRICK_NOTIFICATION_MANAGER (g_value_get_object (value)));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
carrick_pane_class_init (CarrickPaneClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec   *pspec;

  g_type_class_add_private (klass, sizeof (CarrickPanePrivate));

  object_class->get_property = carrick_pane_get_property;
  object_class->set_property = carrick_pane_set_property;

  pspec = g_param_spec_object ("icon-factory",
                               "Icon factory",
                               "Icon factory to use",
                               CARRICK_TYPE_ICON_FACTORY,
                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
  g_object_class_install_property (object_class,
                                   PROP_ICON_FACTORY,
                                   pspec);

  pspec = g_param_spec_object ("notification-manager",
                               "CarrickNotificationManager",
                               "Notification manager to use",
                               CARRICK_TYPE_NOTIFICATION_MANAGER,
                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
  g_object_class_install_property (object_class,
                                   PROP_NOTIFICATIONS,
                                   pspec);
}

/*
 * Generic call_notify function for async d-bus calls
 */
static void
dbus_proxy_notify_cb (DBusGProxy     *proxy,
                      DBusGProxyCall *call,
                      gpointer        user_data)
{
  GError      *error = NULL;

  dbus_g_proxy_end_call (proxy,
                         call,
                         &error,
                         G_TYPE_INVALID);

  if (error)
    {
      g_debug ("Error when ending call: %s",
               error->message);
      g_clear_error (&error);
    }
}

static gboolean
_wifi_switch_callback (NbtkGtkLightSwitch *wifi_switch,
                       gboolean            new_state,
                       CarrickPane        *pane)
{
  CarrickPanePrivate *priv = pane->priv;

  if (new_state)
    {
      carrick_notification_manager_queue_event (priv->notes,
                                                "wifi",
                                                "ready",
                                                "all");
      dbus_g_proxy_begin_call (priv->manager,
                               "EnableTechnology",
                               dbus_proxy_notify_cb,
                               pane,
                               NULL,
                               G_TYPE_STRING,
                               "wifi",
                               G_TYPE_INVALID);
    }
  else
    {
      carrick_notification_manager_queue_event (priv->notes,
                                                "wifi",
                                                "idle",
                                                "all");
      dbus_g_proxy_begin_call (priv->manager,
                               "DisableTechnology",
                               dbus_proxy_notify_cb,
                               pane,
                               NULL,
                               G_TYPE_STRING,
                               "wifi",
                               G_TYPE_INVALID);
    }

  return TRUE;
}

static gboolean
_ethernet_switch_callback (NbtkGtkLightSwitch *ethernet_switch,
                           gboolean            new_state,
                           CarrickPane        *pane)
{
  CarrickPanePrivate *priv = pane->priv;

  if (new_state)
    {
      carrick_notification_manager_queue_event (priv->notes,
                                                "ethernet",
                                                "ready",
                                                "all");
      dbus_g_proxy_begin_call (priv->manager,
                               "EnableTechnology",
                               dbus_proxy_notify_cb,
                               pane,
                               NULL,
                               G_TYPE_STRING,
                               "ethernet",
                               G_TYPE_INVALID);
    }
  else
    {
      carrick_notification_manager_queue_event (priv->notes,
                                                "ethernet",
                                                "idle",
                                                "all");
      dbus_g_proxy_begin_call (priv->manager,
                               "DisableTechnology",
                               dbus_proxy_notify_cb,
                               pane,
                               NULL,
                               G_TYPE_STRING,
                               "ethernet",
                               G_TYPE_INVALID);
    }

  return TRUE;
}

static gboolean
_threeg_switch_callback (NbtkGtkLightSwitch *threeg_switch,
                         gboolean            new_state,
                         CarrickPane        *pane)
{
  CarrickPanePrivate *priv = pane->priv;

  if (new_state)
    {
      carrick_notification_manager_queue_event (priv->notes,
                                                "cellular",
                                                "ready",
                                                "all");
      dbus_g_proxy_begin_call (priv->manager,
                               "EnableTechnology",
                               dbus_proxy_notify_cb,
                               pane,
                               NULL,
                               G_TYPE_STRING,
                               "cellular",
                               G_TYPE_INVALID);
    }
  else
    {
      carrick_notification_manager_queue_event (priv->notes,
                                                "cellular",
                                                "idle",
                                                "all");
      dbus_g_proxy_begin_call (priv->manager,
                               "DisableTechnology",
                               dbus_proxy_notify_cb,
                               pane,
                               NULL,
                               G_TYPE_STRING,
                               "cellular",
                               G_TYPE_INVALID);
    }

  return TRUE;
}

static gboolean
_wimax_switch_callback (NbtkGtkLightSwitch *wimax_switch,
                        gboolean            new_state,
                        CarrickPane        *pane)
{
  CarrickPanePrivate *priv = pane->priv;

  if (new_state)
    {
      carrick_notification_manager_queue_event (priv->notes,
                                                "wimax",
                                                "ready",
                                                "all");
      dbus_g_proxy_begin_call (priv->manager,
                               "EnableTechnology",
                               dbus_proxy_notify_cb,
                               pane,
                               NULL,
                               G_TYPE_STRING,
                               "wimax",
                               G_TYPE_INVALID);
    }
  else
    {
      carrick_notification_manager_queue_event (priv->notes,
                                                "wimax",
                                                "idle",
                                                "all");
      dbus_g_proxy_begin_call (priv->manager,
                               "DisableTechnology",
                               dbus_proxy_notify_cb,
                               pane,
                               NULL,
                               G_TYPE_STRING,
                               "wimax",
                               G_TYPE_INVALID);
    }

  return TRUE;
}

static gboolean
_bluetooth_switch_callback (NbtkGtkLightSwitch *bluetooth_switch,
                            gboolean            new_state,
                            CarrickPane        *pane)
{
  CarrickPanePrivate *priv = pane->priv;

  if (new_state)
    {
      carrick_notification_manager_queue_event (priv->notes,
                                                "bluetooth",
                                                "ready",
                                                "all");
      dbus_g_proxy_begin_call (priv->manager,
                               "EnableTechnology",
                               dbus_proxy_notify_cb,
                               pane,
                               NULL,
                               G_TYPE_STRING,
                               "bluetooth",
                               G_TYPE_INVALID);
    }
  else
    {
      carrick_notification_manager_queue_event (priv->notes,
                                                "bluetooth",
                                                "idle",
                                                "all");
      dbus_g_proxy_begin_call (priv->manager,
                               "DisableTechnology",
                               dbus_proxy_notify_cb,
                               pane,
                               NULL,
                               G_TYPE_STRING,
                               "bluetooth",
                               G_TYPE_INVALID);
    }

  return TRUE;
}

static void
_secret_check_toggled (GtkToggleButton *toggle,
                       gpointer         user_data)
{
  GtkEntry *entry = GTK_ENTRY (user_data);
  gboolean  vis = gtk_toggle_button_get_active (toggle);

  gtk_entry_set_visibility (entry, vis);
}

/*
 * Find the uppermost parent window plug so that
 * we can hide it.
 */
static GtkWidget *
pane_find_plug (GtkWidget *widget)
{
  /* Pippinated */
  while (widget)
    {
      if (GTK_IS_PLUG (widget))
        return widget;
      widget = gtk_widget_get_parent (widget);
    }
  return NULL;
}

static void
_new_connection_cb (GtkButton *button,
                    gpointer   user_data)
{
  CarrickPane        *self = CARRICK_PANE (user_data);
  CarrickPanePrivate *priv = self->priv;
  GtkWidget          *dialog;
  GtkWidget          *desc;
  GtkWidget          *ssid_entry, *ssid_label;
  GtkWidget          *security_combo, *security_label;
  GtkWidget          *secret_entry, *secret_label;
  GtkWidget          *secret_check;
  GtkWidget          *table;
  GtkWidget          *tmp;
  const gchar        *network, *secret;
  gchar              *security;
  GtkWidget          *image;
  GHashTable         *method_props;
  GValue             *type_v, *mode_v, *ssid_v, *security_v, *pass_v;

  dialog = gtk_dialog_new_with_buttons (_ ("New connection settings"),
                                        GTK_WINDOW (gtk_widget_get_parent (user_data)),
                                        GTK_DIALOG_MODAL |
                                        GTK_DIALOG_DESTROY_WITH_PARENT,
                                        GTK_STOCK_CANCEL,
                                        GTK_RESPONSE_REJECT,
                                        GTK_STOCK_CONNECT,
                                        GTK_RESPONSE_ACCEPT, NULL);

  gtk_dialog_set_has_separator (GTK_DIALOG (dialog),
                                FALSE);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog),
                                   GTK_RESPONSE_ACCEPT);
  gtk_window_set_icon_name (GTK_WINDOW (dialog),
                            GTK_STOCK_NETWORK);
  gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->vbox),
                       6);

  table = gtk_table_new (5,
                         2,
                         FALSE);
  gtk_table_set_row_spacings (GTK_TABLE (table),
                              6);
  gtk_table_set_col_spacings (GTK_TABLE (table),
                              6);
  gtk_container_set_border_width (GTK_CONTAINER (table),
                                  6);
  gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox),
                     table);
  image = gtk_image_new_from_icon_name (GTK_STOCK_NETWORK,
                                        GTK_ICON_SIZE_DIALOG);
  gtk_table_attach_defaults (GTK_TABLE (table),
                             image,
                             0, 1,
                             0, 1);

  desc = gtk_label_new (_ ("Enter the name of the network you want\nto "
                           "connect to and, where necessary, the\n"
                           "password and security type."));
  gtk_table_attach_defaults (GTK_TABLE (table),
                             desc,
                             1, 3,
                             0, 1);
  gtk_misc_set_alignment (GTK_MISC (desc),
                          0.0, 0.5);

  ssid_label = gtk_label_new (_ ("Network name"));
  gtk_misc_set_alignment (GTK_MISC (ssid_label),
                          0.0, 0.5);
  gtk_table_attach_defaults (GTK_TABLE (table),
                             ssid_label,
                             1, 2, 1, 2);

  ssid_entry = gtk_entry_new ();
  gtk_table_attach_defaults (GTK_TABLE (table),
                             ssid_entry,
                             2, 3, 1, 2);

  security_label = gtk_label_new (_ ("Network security"));
  gtk_misc_set_alignment (GTK_MISC (security_label),
                          0.0, 0.5);
  gtk_table_attach_defaults (GTK_TABLE (table),
                             security_label,
                             1, 2,
                             2, 3);

  security_combo = gtk_combo_box_new_text ();
  gtk_combo_box_append_text (GTK_COMBO_BOX (security_combo),
                             _ ("None"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (security_combo),
                             "WEP");
  gtk_combo_box_append_text (GTK_COMBO_BOX (security_combo),
                             "WPA");
  gtk_combo_box_append_text (GTK_COMBO_BOX (security_combo),
                             "WPA2");
  gtk_combo_box_set_active (GTK_COMBO_BOX (security_combo),
                            0);
  gtk_table_attach_defaults (GTK_TABLE (table),
                             security_combo,
                             2, 3,
                             2, 3);

  secret_label = gtk_label_new (_ ("Password"));
  gtk_misc_set_alignment (GTK_MISC (secret_label),
                          0.0, 0.5);
  gtk_table_attach_defaults (GTK_TABLE (table),
                             secret_label,
                             1, 2,
                             3, 4);
  secret_entry = gtk_entry_new ();
  gtk_entry_set_visibility (GTK_ENTRY (secret_entry),
                            FALSE);
  gtk_table_attach_defaults (GTK_TABLE (table),
                             secret_entry,
                             2, 3,
                             3, 4);

  secret_check = gtk_check_button_new_with_label (_ ("Show password"));
  g_signal_connect (secret_check,
                    "toggled",
                    G_CALLBACK (_secret_check_toggled),
                    (gpointer) secret_entry);
  gtk_table_attach_defaults (GTK_TABLE (table),
                             secret_check,
                             1, 2,
                             4, 5);

  gtk_widget_show_all (dialog);
  tmp = pane_find_plug (GTK_WIDGET (button));
  if (tmp)
    {
      gtk_widget_hide (tmp);
    }

  if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
    {
      network = gtk_entry_get_text (GTK_ENTRY (ssid_entry));
      security = gtk_combo_box_get_active_text (GTK_COMBO_BOX (security_combo));
      secret = gtk_entry_get_text (GTK_ENTRY (secret_entry));

      if (network == NULL)
        return;

      if (gtk_combo_box_get_active (GTK_COMBO_BOX (security_combo)) == 0)
        {
          /* The 0th item is the None entry. Because we have marked that string
           * for translation we should convert it to a form that ConnMan recognises
           * if it has been selected */
          g_free (security);
          security = g_strdup ("none");
        }
      else if (g_strcmp0 (security, "WPA2") == 0)
        {
          g_free (security);
          security = g_strdup ("rsn");
        }
      else
        {
          guint i;
          for (i = 0; security[i] != '\0'; i++)
            {
              security[i] = g_ascii_tolower (security[i]);
            }
        }

      carrick_notification_manager_queue_event (priv->notes,
                                                "wifi",
                                                "ready",
                                                network);
      /*
       * Call method to connect
       */
      method_props = g_hash_table_new_full (g_str_hash,
                                            g_str_equal,
                                            g_free,
                                            (GDestroyNotify) pane_free_g_value);
      type_v = g_slice_new0 (GValue);
      g_value_init (type_v, G_TYPE_STRING);
      g_value_set_string (type_v, g_strdup ("wifi"));
      g_hash_table_insert (method_props, g_strdup ("Type"), type_v);

      mode_v = g_slice_new0 (GValue);
      g_value_init (mode_v, G_TYPE_STRING);
      g_value_set_string (mode_v, g_strdup ("managed"));
      g_hash_table_insert (method_props, g_strdup ("Mode"), mode_v);

      ssid_v = g_slice_new0 (GValue);
      g_value_init (ssid_v, G_TYPE_STRING);
      g_value_set_string (ssid_v, secret);
      g_hash_table_insert (method_props, g_strdup ("SSID"), ssid_v);

      if (security)
        {
          security_v = g_slice_new0 (GValue);
          g_value_init (security_v, G_TYPE_STRING);
          g_value_set_string (security_v, security);
          g_hash_table_insert (method_props, g_strdup ("Security"), security_v);
        }

      if (secret)
        {
          pass_v = g_slice_new0 (GValue);
          g_value_init (pass_v, G_TYPE_STRING);
          g_value_set_string (pass_v, secret);
          g_hash_table_insert (method_props, g_strdup ("Passphrase"), pass_v);
        }

      dbus_g_proxy_begin_call (priv->manager,
                               "ConnectService",
                               dbus_proxy_notify_cb,
                               self,
                               NULL,
                               dbus_g_type_get_map ("GHashTable",
                                                    G_TYPE_STRING,
                                                    G_TYPE_VALUE),
                               method_props,
                               G_TYPE_INVALID);
    }
  gtk_widget_destroy (dialog);
}

static gboolean
_offline_mode_switch_callback (NbtkGtkLightSwitch *flight_switch,
                               gboolean            new_state,
                               CarrickPane        *pane)
{
  CarrickPanePrivate *priv = pane->priv;

  /* FIXME: This is a band aid, needs a better fix */
  carrick_notification_manager_queue_event (priv->notes,
                                            "wifi",
                                            "idle",
                                            "all");

  dbus_g_proxy_begin_call (priv->manager,
                           "SetProperty",
                           dbus_proxy_notify_cb,
                           pane,
                           NULL,
                           G_TYPE_STRING,
                           "OfflineMode",
                           G_TYPE_BOOLEAN,
                           new_state,
                           G_TYPE_INVALID);

  return TRUE;
}

static void
_add_fallback (CarrickPane *pane)
{
  CarrickPanePrivate *priv = pane->priv;
  gchar              *fallback = NULL;

  /* Need to add some fall-back content */
  if (!priv->have_daemon)
    {
      /*
       * Hint to display when we detect that the connection manager
       * is dead.  Ideally the system auto-restarts connman so the
       * user will not see this, but this is what to show if all
       * available recovery measures fail.
       */
      fallback = g_strdup (_ ("Sorry, we can't find any networks. "
                              "The Connection Manager doesn't seem to be running. "
                              "You may want to try re-starting your device."
                              ));
    }
  else if (priv->offline_mode)
    {
      /*
       * Hint display if we detect that the system is in
       * offline mode and there are no available networks
       */
      fallback = g_strdup (_ ("Sorry, we can't find any networks. "
                              "You could try disabling Offline mode. "
                              ));
    }
  else if ((priv->have_wifi && !priv->wifi_enabled) ||
           (priv->have_ethernet && !priv->ethernet_enabled) ||
           (priv->have_threeg && !priv->threeg_enabled) ||
           (priv->have_wimax && !priv->wimax_enabled) ||
           (priv->have_bluetooth && !priv->bluetooth_enabled))
    {
      if (priv->have_wifi && !priv->wifi_enabled)
        {
          /*
           * Hint to display if we detect that wifi has been turned off
           * and there are no available networks
           */
          fallback = g_strdup (_ ("Sorry, we can't find any networks. "
                                  "You could try turning on WiFi."
                                  ));
        }
      else if (priv->have_wimax && !priv->wimax_enabled)
        {
          /*
           * Hint to display if we detect that wifi is on but
           * WiMAX has been turned off and there are no available
           * networks
           */
          fallback = g_strdup (_ ("Sorry, we can't find any networks. "
                                  "You could try turning on WiMAX."
                                  ));
        }
      else if (priv->have_threeg && !priv->threeg_enabled)
        {
          /*
           * Hint to display if we detect that wifi and wimax is on but
           * 3G has been turned off and there are no available
           * networks
           */
          fallback = g_strdup (_ ("Sorry, we can't find any networks. "
                                  "You could try turning on 3G."
                                  ));
        }
      else if (priv->have_bluetooth && !priv->bluetooth_enabled)
        {
          /*
           * Hint to display if we detect that wifi, wimax, and 3G are on but
           * bluetooth has been turned off and there are no available networks
           */
          fallback = g_strdup (_ ("Sorry, we can't find any networks. "
                                  "You could try turning on Bluetooth."
                                  ));
        }
      else if (priv->have_ethernet && !priv->ethernet_enabled)
        {
          /*
           * Hint to display if we detect that all technologies
           * other then ethernet have been turned on, and there
           * are no available networks
           */
          fallback = g_strdup (_ ("Sorry, we can't find any networks. "
                                  "You could try turning on Wired."
                                  ));
        }
    }
  else
    {
      /*
       * Generic message to display if all available networking
       * technologies are turned on, but for whatever reason we
       * can not find any networks
       */
      fallback = g_strdup (_ ("Sorry, we can't find any networks"));
    }

  carrick_list_set_fallback (CARRICK_LIST (priv->service_list), fallback);
  g_free (fallback);
}

static void
pane_update_property (const gchar *property,
                      GValue      *value,
                      gpointer     user_data)
{
  CarrickPane        *self = user_data;
  CarrickPanePrivate *priv = self->priv;

  if (g_str_equal (property, "OfflineMode"))
    {
      priv->offline_mode = g_value_get_boolean (value);

      /* disarm signal handler */
      g_signal_handlers_disconnect_by_func (priv->offline_mode_switch,
                                            _offline_mode_switch_callback,
                                            user_data);

      nbtk_gtk_light_switch_set_active (NBTK_GTK_LIGHT_SWITCH
                                        (priv->offline_mode_switch),
                                        priv->offline_mode);

      /* arm signal handler */
      g_signal_connect (NBTK_GTK_LIGHT_SWITCH (priv->offline_mode_switch),
                        "switch-flipped",
                        G_CALLBACK (_offline_mode_switch_callback),
                        user_data);
    }
  /*else if (g_str_equal (property, "State"))
     {
     }*/
  else if (g_str_equal (property, "AvailableTechnologies"))
    {
      gchar **tech = g_value_get_boxed (value);
      gint    i;

      priv->have_wifi = FALSE;
      priv->have_ethernet = FALSE;
      priv->have_threeg = FALSE;
      priv->have_wimax = FALSE;
      priv->have_bluetooth = FALSE;

      for (i = 0; i < g_strv_length (tech); i++)
        {
          if (g_str_equal ("wifi", *(tech + i)))
            {
              priv->have_wifi = TRUE;
            }
          else if (g_str_equal ("wimax", *(tech + i)))
            {
              priv->have_wimax = TRUE;
            }
          else if (g_str_equal ("bluetooth", *(tech + i)))
            {
              priv->have_bluetooth = TRUE;
            }
          else if (g_str_equal ("cellular", *(tech + i)))
            {
              priv->have_threeg = TRUE;
            }
          else if (g_str_equal ("ethernet", *(tech + i)))
            {
              priv->have_ethernet = TRUE;
            }
        }

      gtk_widget_set_sensitive (priv->wifi_switch,
                                priv->have_wifi);
      gtk_widget_set_sensitive (priv->ethernet_switch,
                                priv->have_ethernet);
      gtk_widget_set_sensitive (priv->threeg_switch,
                                priv->have_threeg);
      gtk_widget_set_sensitive (priv->wimax_switch,
                                priv->have_wimax);
      gtk_widget_set_sensitive (priv->bluetooth_switch,
                                priv->have_bluetooth);
    }
  else if (g_str_equal (property, "EnabledTechnologies"))
    {
      gchar **tech = g_value_get_boxed (value);
      gint    i;

      priv->wifi_enabled = FALSE;
      priv->ethernet_enabled = FALSE;
      priv->threeg_enabled = FALSE;
      priv->wimax_enabled = FALSE;
      priv->bluetooth_enabled = FALSE;

      for (i = 0; i < g_strv_length (tech); i++)
        {
          if (g_str_equal ("wifi", *(tech + i)))
            {
              priv->wifi_enabled = TRUE;
            }
          else if (g_str_equal ("wimax", *(tech + i)))
            {
              priv->wimax_enabled = TRUE;
            }
          else if (g_str_equal ("bluetooth", *(tech + i)))
            {
              priv->bluetooth_enabled = TRUE;
            }
          else if (g_str_equal ("cellular", *(tech + i)))
            {
              priv->threeg_enabled = TRUE;
            }
          else if (g_str_equal ("ethernet", *(tech + i)))
            {
              priv->ethernet_enabled = TRUE;
            }
        }

      /* disarm signal handlers */
      g_signal_handlers_disconnect_by_func (priv->ethernet_switch,
                                            _ethernet_switch_callback,
                                            user_data);
      g_signal_handlers_disconnect_by_func (priv->wifi_switch,
                                            _wifi_switch_callback,
                                            user_data);
      g_signal_handlers_disconnect_by_func (priv->threeg_switch,
                                            _threeg_switch_callback,
                                            user_data);
      g_signal_handlers_disconnect_by_func (priv->wimax_switch,
                                            _wimax_switch_callback,
                                            user_data);
      g_signal_handlers_disconnect_by_func (priv->bluetooth_switch,
                                            _bluetooth_switch_callback,
                                            user_data);

      nbtk_gtk_light_switch_set_active (NBTK_GTK_LIGHT_SWITCH
                                        (priv->ethernet_switch),
                                        priv->ethernet_enabled);
      nbtk_gtk_light_switch_set_active (NBTK_GTK_LIGHT_SWITCH
                                        (priv->wifi_switch),
                                        priv->wifi_enabled);
      nbtk_gtk_light_switch_set_active (NBTK_GTK_LIGHT_SWITCH
                                        (priv->threeg_switch),
                                        priv->threeg_enabled);
      nbtk_gtk_light_switch_set_active (NBTK_GTK_LIGHT_SWITCH
                                        (priv->wimax_switch),
                                        priv->wimax_enabled);
      nbtk_gtk_light_switch_set_active (NBTK_GTK_LIGHT_SWITCH
                                        (priv->bluetooth_switch),
                                        priv->bluetooth_enabled);

      /* arm signal handlers */
      g_signal_connect (NBTK_GTK_LIGHT_SWITCH (priv->ethernet_switch),
                        "switch-flipped",
                        G_CALLBACK (_ethernet_switch_callback),
                        user_data);
      g_signal_connect (NBTK_GTK_LIGHT_SWITCH (priv->wifi_switch),
                        "switch-flipped",
                        G_CALLBACK (_wifi_switch_callback),
                        user_data);
      g_signal_connect (NBTK_GTK_LIGHT_SWITCH (priv->threeg_switch),
                        "switch-flipped",
                        G_CALLBACK (_threeg_switch_callback),
                        user_data);
      g_signal_connect (NBTK_GTK_LIGHT_SWITCH (priv->wimax_switch),
                        "switch-flipped",
                        G_CALLBACK (_wimax_switch_callback),
                        user_data);
      g_signal_connect (NBTK_GTK_LIGHT_SWITCH (priv->bluetooth_switch),
                        "switch-flipped",
                        G_CALLBACK (_bluetooth_switch_callback),
                        user_data);

      /* only enable "Add new connection" if wifi is enabled */
      gtk_widget_set_sensitive (priv->new_conn_button,
                                priv->wifi_enabled);
    }
}

static void
pane_manager_changed_cb (DBusGProxy  *proxy,
                         const gchar *property,
                         GValue      *value,
                         gpointer     user_data)
{
  CarrickPane *self = user_data;

  pane_update_property (property, value, self);
}

static void
pane_manager_get_properties_cb (DBusGProxy     *manager,
                                DBusGProxyCall *call,
                                gpointer        user_data)
{
  CarrickPane *self = user_data;
  GError      *error = NULL;
  GHashTable  *properties;

  dbus_g_proxy_end_call (manager,
                         call,
                         &error,
                         dbus_g_type_get_map ("GHashTable",
                                              G_TYPE_STRING,
                                              G_TYPE_VALUE),
                         &properties,
                         G_TYPE_INVALID);

  if (error)
    {
      g_debug ("Error when ending GetProperties call: %s",
               error->message);
      g_clear_error (&error);
    }

  g_hash_table_foreach (properties,
                        (GHFunc) pane_update_property,
                        self);
  g_hash_table_unref (properties);
}

static void
carrick_pane_init (CarrickPane *self)
{
  CarrickPanePrivate *priv;
  GtkWidget          *switch_bin;
  GtkWidget          *flight_bin;
  GtkWidget          *net_list_bin;
  GtkWidget          *hbox, *switch_box;
  GtkWidget          *vbox;
  GtkWidget          *switch_label;
  GtkWidget          *frame_title;
  GtkWidget          *offline_mode_label;
  gchar              *label = NULL;
  GError             *error = NULL;
  DBusGConnection    *connection;
  GtkTreeModel       *model;

  priv = self->priv = PANE_PRIVATE (self);

  priv->icon_factory = NULL;
  priv->manager = NULL;
  priv->last_scan = time (NULL);
  priv->have_daemon = FALSE;
  priv->have_wifi = FALSE;
  priv->have_ethernet = FALSE;
  priv->have_threeg = FALSE;
  priv->have_wimax = FALSE;
  priv->have_bluetooth = FALSE;
  priv->wifi_enabled = FALSE;
  priv->ethernet_enabled = FALSE;
  priv->threeg_enabled = FALSE;
  priv->wimax_enabled = FALSE;
  priv->bluetooth_enabled = FALSE;

  dbus_g_object_register_marshaller (connman_marshal_VOID__STRING_BOXED,
                                     /* return */
                                     G_TYPE_NONE,
                                     /* args */
                                     G_TYPE_STRING,
                                     G_TYPE_VALUE,
                                     /* eom */
                                     G_TYPE_INVALID);

  /* Get D-Bus connection and create proxy */
  connection = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
  if (error)
    {
      g_debug ("Error connecting to bus: %s",
               error->message);
      g_clear_error (&error);
      /* FIXME: do something */
    }
  priv->manager = dbus_g_proxy_new_for_name (connection,
                                             CONNMAN_SERVICE,
                                             CONNMAN_MANAGER_PATH,
                                             CONNMAN_MANAGER_INTERFACE);

  model = carrick_network_model_new ();

  /* Listen for the D-Bus PropertyChanged signal */
  dbus_g_proxy_add_signal (priv->manager,
                           "PropertyChanged",
                           G_TYPE_STRING,
                           G_TYPE_VALUE,
                           G_TYPE_INVALID);
  dbus_g_proxy_connect_signal (priv->manager,
                               "PropertyChanged",
                               G_CALLBACK (pane_manager_changed_cb),
                               self,
                               NULL);

  dbus_g_proxy_begin_call (priv->manager,
                           "GetProperties",
                           pane_manager_get_properties_cb,
                           self,
                           NULL,
                           G_TYPE_INVALID);

  switch_bin = nbtk_gtk_frame_new ();
  gtk_widget_show (switch_bin);
  flight_bin = nbtk_gtk_frame_new ();
  gtk_widget_show (flight_bin);
  net_list_bin = nbtk_gtk_frame_new ();
  gtk_widget_show (net_list_bin);

  /* Set table up */
  gtk_table_resize (GTK_TABLE (self),
                    8,
                    6);
  gtk_table_set_homogeneous (GTK_TABLE (self),
                             TRUE);
  gtk_container_set_border_width (GTK_CONTAINER (self), 4);
  gtk_table_set_row_spacings (GTK_TABLE (self), 4);
  gtk_table_set_col_spacings (GTK_TABLE (self), 4);

  /* Network list */
  label = g_strdup_printf ("<span font_desc=\"Liberation Sans Bold 18px\""
                           "foreground=\"#3e3e3e\">%s</span>",
                           _ ("Networks"));

  frame_title = gtk_label_new ("");
  gtk_widget_show (frame_title);
  gtk_label_set_markup (GTK_LABEL (frame_title),
                        label);
  g_free (label);
  gtk_frame_set_label_widget (GTK_FRAME (net_list_bin),
                              frame_title);

  priv->service_list = carrick_list_new (priv->icon_factory,
                                         priv->notes,
                                         CARRICK_NETWORK_MODEL (model));
  gtk_container_add (GTK_CONTAINER (net_list_bin),
                     priv->service_list);
  gtk_widget_show (priv->service_list);

  gtk_table_attach_defaults (GTK_TABLE (self),
                             net_list_bin,
                             0, 4,
                             0, 7);

  /* New connection button */
  vbox = gtk_vbox_new (FALSE, 0);
  gtk_widget_show (vbox);
  gtk_table_attach (GTK_TABLE (self),
                    vbox,
                    0, 6,
                    7, 8,
                    GTK_FILL,
                    GTK_EXPAND,
                    0, 0);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (vbox),
                      hbox,
                      FALSE,
                      TRUE,
                      0);

  priv->new_conn_button = gtk_button_new_with_label (_ ("Add new connection"));
  gtk_widget_set_sensitive (priv->new_conn_button,
                            FALSE);
  gtk_widget_show (priv->new_conn_button);
  g_signal_connect (GTK_BUTTON (priv->new_conn_button),
                    "clicked",
                    G_CALLBACK (_new_connection_cb),
                    self);
  gtk_box_pack_start (GTK_BOX (hbox),
                      priv->new_conn_button,
                      FALSE,
                      TRUE,
                      12);


  /* Switches */
  vbox = gtk_vbox_new (TRUE,
                       6);
  gtk_widget_show (vbox);
  gtk_container_add (GTK_CONTAINER (switch_bin),
                     vbox);

  switch_box = gtk_hbox_new (TRUE,
                             6);
  gtk_widget_show (switch_box);

  priv->wifi_switch = nbtk_gtk_light_switch_new ();
  priv->wifi_label = gtk_label_new (_ ("WiFi"));
  gtk_misc_set_alignment (GTK_MISC (priv->wifi_label),
                          0.2,
                          0.5);
  gtk_widget_show (priv->wifi_switch);
  gtk_widget_show (priv->wifi_label);
  gtk_widget_set_sensitive (priv->wifi_switch, FALSE);

  gtk_box_pack_start (GTK_BOX (switch_box),
                      priv->wifi_label,
                      TRUE,
                      TRUE,
                      8);
  gtk_box_pack_start (GTK_BOX (switch_box),
                      priv->wifi_switch,
                      TRUE,
                      TRUE,
                      8);
  gtk_box_pack_start (GTK_BOX (vbox),
                      switch_box,
                      FALSE,
                      FALSE,
                      8);
  g_signal_connect (NBTK_GTK_LIGHT_SWITCH (priv->wifi_switch),
                    "switch-flipped",
                    G_CALLBACK (_wifi_switch_callback),
                    self);

  switch_box = gtk_hbox_new (TRUE,
                             6);
  gtk_widget_show (switch_box);
  priv->ethernet_switch = nbtk_gtk_light_switch_new ();
  priv->ethernet_label = gtk_label_new (_ ("Wired"));
  gtk_misc_set_alignment (GTK_MISC (priv->ethernet_label),
                          0.2,
                          0.5);
  gtk_widget_show (priv->ethernet_switch);
  gtk_widget_show (priv->ethernet_label);
  gtk_widget_set_sensitive (priv->ethernet_switch, FALSE);
  gtk_box_pack_start (GTK_BOX (switch_box),
                      priv->ethernet_label,
                      TRUE,
                      TRUE,
                      8);
  gtk_box_pack_start (GTK_BOX (switch_box),
                      priv->ethernet_switch,
                      TRUE,
                      TRUE,
                      8);
  gtk_box_pack_start (GTK_BOX (vbox),
                      switch_box,
                      FALSE,
                      FALSE,
                      8);
  g_signal_connect (NBTK_GTK_LIGHT_SWITCH (priv->ethernet_switch),
                    "switch-flipped",
                    G_CALLBACK (_ethernet_switch_callback),
                    self);

  switch_box = gtk_hbox_new (TRUE,
                             6);
  gtk_widget_show (switch_box);
  priv->threeg_switch = nbtk_gtk_light_switch_new ();
  priv->threeg_label = gtk_label_new (_ ("3G"));
  gtk_misc_set_alignment (GTK_MISC (priv->threeg_label),
                          0.2,
                          0.5);
  gtk_widget_show (priv->threeg_switch);
  gtk_widget_show (priv->threeg_label);
  gtk_widget_set_sensitive (priv->threeg_switch, FALSE);
  gtk_box_pack_start (GTK_BOX (switch_box),
                      priv->threeg_label,
                      TRUE,
                      TRUE,
                      8);
  gtk_box_pack_start (GTK_BOX (switch_box),
                      priv->threeg_switch,
                      TRUE,
                      TRUE,
                      8);
  gtk_box_pack_start (GTK_BOX (vbox),
                      switch_box,
                      FALSE,
                      FALSE,
                      8);
  g_signal_connect (NBTK_GTK_LIGHT_SWITCH (priv->threeg_switch),
                    "switch-flipped",
                    G_CALLBACK (_threeg_switch_callback),
                    self);

  switch_box = gtk_hbox_new (TRUE,
                             6);
  gtk_widget_show (switch_box);
  priv->wimax_switch = nbtk_gtk_light_switch_new ();
  priv->wimax_label = gtk_label_new (_ ("WiMAX"));
  gtk_misc_set_alignment (GTK_MISC (priv->wimax_label),
                          0.2,
                          0.5);
  gtk_widget_show (priv->wimax_switch);
  gtk_widget_show (priv->wimax_label);
  gtk_widget_set_sensitive (priv->wimax_switch, FALSE);

  gtk_box_pack_start (GTK_BOX (switch_box),
                      priv->wimax_label,
                      TRUE,
                      TRUE,
                      8);
  gtk_box_pack_start (GTK_BOX (switch_box),
                      priv->wimax_switch,
                      TRUE,
                      TRUE,
                      8);
  gtk_box_pack_start (GTK_BOX (vbox),
                      switch_box,
                      FALSE,
                      FALSE,
                      8);
  g_signal_connect (NBTK_GTK_LIGHT_SWITCH (priv->wimax_switch),
                    "switch-flipped",
                    G_CALLBACK (_wimax_switch_callback),
                    self);

  switch_box = gtk_hbox_new (TRUE,
                             6);
  gtk_widget_show (switch_box);
  priv->bluetooth_switch = nbtk_gtk_light_switch_new ();
  priv->bluetooth_label = gtk_label_new (_ ("Bluetooth"));
  gtk_misc_set_alignment (GTK_MISC (priv->bluetooth_label),
                          0.2,
                          0.5);
  gtk_widget_show (priv->bluetooth_switch);
  gtk_widget_show (priv->bluetooth_label);
  gtk_widget_set_sensitive (priv->bluetooth_switch, FALSE);

  gtk_box_pack_start (GTK_BOX (switch_box),
                      priv->bluetooth_label,
                      TRUE,
                      TRUE,
                      8);
  gtk_box_pack_start (GTK_BOX (switch_box),
                      priv->bluetooth_switch,
                      TRUE,
                      TRUE,
                      8);
  gtk_box_pack_start (GTK_BOX (vbox),
                      switch_box,
                      FALSE,
                      FALSE,
                      8);
  g_signal_connect (NBTK_GTK_LIGHT_SWITCH (priv->bluetooth_switch),
                    "switch-flipped",
                    G_CALLBACK (_bluetooth_switch_callback),
                    self);

  gtk_table_attach_defaults (GTK_TABLE (self),
                             switch_bin,
                             4, 6,
                             0, 5);

  vbox = gtk_vbox_new (TRUE,
                       0);
  gtk_widget_show (vbox);
  gtk_container_add (GTK_CONTAINER (flight_bin),
                     vbox);
  priv->offline_mode_switch = nbtk_gtk_light_switch_new ();
  gtk_widget_show (priv->offline_mode_switch);
  switch_box = gtk_hbox_new (TRUE,
                             6);
  gtk_widget_show (switch_box);
  switch_label = gtk_label_new (_ ("Offline mode"));
  gtk_widget_show (switch_label);
  gtk_misc_set_alignment (GTK_MISC (switch_label),
                          0.2,
                          0.5);
  g_signal_connect (NBTK_GTK_LIGHT_SWITCH (priv->offline_mode_switch),
                    "switch-flipped",
                    G_CALLBACK (_offline_mode_switch_callback),
                    self);
  gtk_box_pack_start (GTK_BOX (switch_box),
                      switch_label,
                      TRUE,
                      TRUE,
                      8);
  gtk_box_pack_start (GTK_BOX (switch_box),
                      priv->offline_mode_switch,
                      TRUE,
                      TRUE,
                      8);
  gtk_box_pack_start (GTK_BOX (vbox),
                      switch_box,
                      TRUE,
                      FALSE,
                      8);
  offline_mode_label = gtk_label_new
          (_ ("This will disable all your connections"));
  gtk_label_set_line_wrap (GTK_LABEL (offline_mode_label),
                           TRUE);
  gtk_misc_set_alignment (GTK_MISC (offline_mode_label),
                          0.5,
                          0.0);
  gtk_widget_show (offline_mode_label);
  gtk_box_pack_start (GTK_BOX (vbox),
                      offline_mode_label,
                      TRUE,
                      TRUE,
                      0);
  gtk_table_attach_defaults (GTK_TABLE (self),
                             flight_bin,
                             4, 6,
                             5, 7);

  g_signal_connect (GTK_WIDGET (self),
                    "focus",
                    G_CALLBACK (_focus_callback),
                    NULL);
}

void
carrick_pane_update (CarrickPane *pane)
{
  CarrickPanePrivate *priv = pane->priv;
  time_t              now = time (NULL);

  /* Only trigger a scan if we haven't triggered one in the last minute.
   * This number likely needs tweaking.
   */
  if (difftime (now, priv->last_scan) > 60)
    {
      priv->last_scan = now;

      /* Request a scan on all technologies.
       * The UI doesn't really care if the call to scan completes or
       * not so just fire and forget
       */
      dbus_g_proxy_begin_call (priv->manager,
                               "RequestScan",
                               dbus_proxy_notify_cb,
                               pane,
                               NULL,
                               G_TYPE_STRING,
                               "",
                               G_TYPE_INVALID);
    }

  carrick_list_set_all_inactive (CARRICK_LIST (priv->service_list));
}

GtkWidget*
carrick_pane_new (CarrickIconFactory         *icon_factory,
                  CarrickNotificationManager *notifications)
{
  return g_object_new (CARRICK_TYPE_PANE,
                       "icon-factory",
                       icon_factory,
                       "notification-manager",
                       notifications,
                       NULL);
}
