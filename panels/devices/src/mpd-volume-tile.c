
/*
 * Copyright © 2010, 2012 Intel Corporation
 *
 * Authors: Rob Bradford <rob@linux.intel.com> (dalston-volume-applet.c)
 *          Rob Staudinger <robert.staudinger@intel.com>
 *          Damien Lespiau <damien.lespiau@intel.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU Lesser General Public License,
 * version 2.1, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdbool.h>

#include <canberra-gtk.h>
#include <glib/gi18n.h>
#include <gvc/gvc-mixer-control.h>

#include "mpd-gobject.h"
#include "mpd-shell-defines.h"
#include "mpd-text.h"
#include "mpd-volume-tile.h"
#include "config.h"

#define MIXER_CONTROL_NAME "Dawati Panel Devices"
#define VOLUME_CHANGED_EVENT "audio-volume-change"

GvcMixerStream *
mpd_volume_tile_get_sink (MpdVolumeTile   *self);

static void
mpd_volume_tile_set_sink (MpdVolumeTile   *self,
                          GvcMixerStream  *stream);

static void
update_volume_slider (MpdVolumeTile *self);

static void
update_stream_volume (MpdVolumeTile *self);

G_DEFINE_TYPE (MpdVolumeTile, mpd_volume_tile, MX_TYPE_BOX_LAYOUT)

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), MPD_TYPE_VOLUME_TILE, MpdVolumeTilePrivate))

enum
{
  PROP_0,

  PROP_SINK
};

typedef struct
{
  /* Managed by clutter */
  ClutterActor    *volume_slider;

  /* Data */
  GvcMixerControl *control;
  GvcMixerStream  *sink;
  int              playing_event_sound;
} MpdVolumeTilePrivate;

#if 0
static void
_play_sound_completed_cb (ca_context *context,
                          uint32_t       id,
                          int            res,
                          MpdVolumeTile *self)
{
  MpdVolumeTilePrivate *priv = GET_PRIVATE (self);

  (void) g_atomic_int_dec_and_test (&priv->playing_event_sound);
}

/*
 * Volume is a value from 0.0 to 1.0
 */
static void
update_volume_label (MpdVolumeTile  *self,
                     double          volume)
{
  MpdVolumeTilePrivate *priv = GET_PRIVATE (self);
  char  *old_level;
  float  label_width;
  float  slider_width;
  float  x;

  g_return_if_fail (0.0 <= volume && volume <= 1.0);

  old_level = g_strdup (mx_label_get_text (MX_LABEL (priv->volume_label)));

  /* Label text */
  if (volume == 1.0)
    mx_label_set_text (MX_LABEL (priv->volume_label), _("Turned up to 11"));
  else if (volume >= 0.90)
    mx_label_set_text (MX_LABEL (priv->volume_label), _("Very loud"));
  else if (volume >= 0.75)
    mx_label_set_text (MX_LABEL (priv->volume_label), _("Loud"));
  else if (volume > 0.50)
    mx_label_set_text (MX_LABEL (priv->volume_label), _("Fairly loud"));
  else if (volume == 0.50)
    mx_label_set_text (MX_LABEL (priv->volume_label), _("Middle of the road"));
  else if (volume >= 0.25)
    mx_label_set_text (MX_LABEL (priv->volume_label), _("Fairly quiet"));
  else if (volume >= 0.10)
    mx_label_set_text (MX_LABEL (priv->volume_label), _("Quiet"));
  else if (volume > 0.0)
    mx_label_set_text (MX_LABEL (priv->volume_label), _("Very quiet"));
  else
    mx_label_set_text (MX_LABEL (priv->volume_label), _("Silent"));

  /* Label position */
  label_width = clutter_actor_get_width (priv->volume_label);
  slider_width = clutter_actor_get_width (priv->volume_slider);
  x = slider_width * volume - label_width / 2;
  x = CLAMP (x, 0.0, slider_width - label_width);
  clutter_actor_set_x (priv->volume_label, x);

  /* Notification */
  if (0 != g_strcmp0 (old_level,
                      mx_label_get_text (MX_LABEL (priv->volume_label))))
  {
    gint res;
    ca_proplist *proplist;
    ca_context *context;

    if (g_atomic_int_get (&priv->playing_event_sound) > 0)
      return;

    context = ca_gtk_context_get ();

    ca_proplist_create (&proplist);
    ca_proplist_sets (proplist,
                      CA_PROP_EVENT_ID,
                      VOLUME_CHANGED_EVENT);

    res = ca_context_play_full (context,
                                1,
                                proplist,
                                (ca_finish_callback_t )_play_sound_completed_cb,
                                self);
    ca_proplist_destroy (proplist);

    if (res != CA_SUCCESS)
    {
      g_warning ("%s: Error playing test sound: %s",
                 G_STRLOC,
                 ca_strerror (res));
    } else {
      g_atomic_int_inc (&priv->playing_event_sound);
    }
  }
  g_free (old_level);
}
#endif

static void
_volume_slider_value_notify_cb (MxSlider      *slider,
                                GParamSpec    *pspec,
                                MpdVolumeTile *self)
{
  update_stream_volume (self);
}

static void
_stream_volume_notify_cb (GObject       *object,
                          GParamSpec    *pspec,
                          MpdVolumeTile *self)
{
  update_volume_slider (self);
}

static void
_mixer_control_default_sink_changed_cb (GvcMixerControl *control,
                                        unsigned int     id,
                                        MpdVolumeTile   *self)
{
  mpd_volume_tile_set_sink (self,
                            gvc_mixer_control_get_default_sink (control));
}

static void
_mixer_control_ready_cb (GvcMixerControl  *control,
                         MpdVolumeTile    *self)
{
  MpdVolumeTilePrivate *priv = GET_PRIVATE (self);

  mpd_volume_tile_set_sink (self,
                            gvc_mixer_control_get_default_sink (priv->control));
}

static void
_get_property (GObject      *object,
               unsigned int  property_id,
               GValue       *value,
               GParamSpec   *pspec)
{
  switch (property_id) {
  case PROP_SINK:
    g_value_set_object (value,
                        mpd_volume_tile_get_sink (MPD_VOLUME_TILE (object)));
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
_set_property (GObject      *object,
               unsigned int  property_id,
               const GValue *value,
               GParamSpec   *pspec)
{
  switch (property_id) {
  case PROP_SINK:
    mpd_volume_tile_set_sink (MPD_VOLUME_TILE (object),
                              g_value_get_object (value));
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
_dispose (GObject *object)
{
  MpdVolumeTilePrivate *priv = GET_PRIVATE (object);

  mpd_gobject_detach (object, (GObject **) &priv->control);

  G_OBJECT_CLASS (mpd_volume_tile_parent_class)->dispose (object);
}

static void
mpd_volume_tile_class_init (MpdVolumeTileClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamFlags   param_flags;

  g_type_class_add_private (klass, sizeof (MpdVolumeTilePrivate));

  object_class->get_property = _get_property;
  object_class->set_property = _set_property;
  object_class->dispose = _dispose;

  /* Properties */

  param_flags = G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS;

  g_object_class_install_property (object_class,
                                   PROP_SINK,
                                   g_param_spec_object ("sink",
                                                        "Sink",
                                                        "The mixer stream",
                                                        GVC_TYPE_MIXER_STREAM,
                                                        param_flags));
}

static void
mpd_volume_tile_init (MpdVolumeTile *self)
{
  MpdVolumeTilePrivate *priv = GET_PRIVATE (self);
  ClutterActor  *icon;

  /* Layout */
  mx_box_layout_set_spacing (MX_BOX_LAYOUT (self), MPD_TILE_ICON_SPACING);

  icon = mx_icon_new ();
  clutter_actor_set_name (icon, "volume-off");
  mx_box_layout_add_actor_with_properties (MX_BOX_LAYOUT (self),
                                           icon,
                                           -1,
                                           "y-fill", FALSE,
                                           NULL);

  priv->volume_slider = mx_slider_new ();
  mx_box_layout_add_actor_with_properties (MX_BOX_LAYOUT (self),
                                           priv->volume_slider,
                                           -1,
                                           "expand", TRUE,
                                           NULL);
  g_signal_connect (priv->volume_slider, "notify::value",
                    G_CALLBACK (_volume_slider_value_notify_cb), self);

  icon = mx_icon_new ();
  clutter_actor_set_name (icon, "volume-on");
  mx_box_layout_add_actor_with_properties (MX_BOX_LAYOUT (self),
                                           icon,
                                           -1,
                                           "y-fill", FALSE,
                                           NULL);

  /* Control */
  priv->control = gvc_mixer_control_new (MIXER_CONTROL_NAME);
  g_signal_connect (priv->control, "default-sink-changed",
                    G_CALLBACK (_mixer_control_default_sink_changed_cb), self);
  g_signal_connect (priv->control, "ready",
                    G_CALLBACK (_mixer_control_ready_cb), self);
  gvc_mixer_control_open (priv->control);
}

ClutterActor *
mpd_volume_tile_new (void)
{
  return g_object_new (MPD_TYPE_VOLUME_TILE, NULL);
}

GvcMixerStream *
mpd_volume_tile_get_sink (MpdVolumeTile *self)
{
  MpdVolumeTilePrivate *priv = GET_PRIVATE (self);

  g_return_val_if_fail (MPD_IS_VOLUME_TILE (self), NULL);

  return priv->sink;
}

static void
mpd_volume_tile_set_sink (MpdVolumeTile   *self,
                          GvcMixerStream  *sink)
{
  MpdVolumeTilePrivate *priv = GET_PRIVATE (self);
  bool do_notify;

  do_notify = sink != priv->sink;

  if (priv->sink)
  {
    g_signal_handlers_disconnect_by_func (priv->sink,
                                          _stream_volume_notify_cb,
                                          self);
    g_object_unref (priv->sink);
    priv->sink = NULL;
  }

  if (sink)
  {
    priv->sink = g_object_ref (sink);

    g_signal_connect (priv->sink, "notify::volume",
                      G_CALLBACK (_stream_volume_notify_cb), self);

    update_volume_slider (self);
  }

  if (do_notify)
  {
    g_object_notify (G_OBJECT (self), "sink");
  }
}

static void
update_volume_slider (MpdVolumeTile *self)
{
  MpdVolumeTilePrivate *priv = GET_PRIVATE (self);
  double  volume;
  double  value;

  g_signal_handlers_disconnect_by_func (priv->volume_slider,
                                        _volume_slider_value_notify_cb,
                                        self);

  volume = gvc_mixer_stream_get_volume (priv->sink);
  value = volume / PA_VOLUME_NORM;
  mx_slider_set_value (MX_SLIDER (priv->volume_slider), value);

  g_signal_connect (priv->volume_slider, "notify::value",
                    G_CALLBACK (_volume_slider_value_notify_cb), self);

}

static void
update_stream_volume (MpdVolumeTile *self)
{
  MpdVolumeTilePrivate *priv = GET_PRIVATE (self);
  double       progress;
  pa_volume_t  volume;

  g_signal_handlers_disconnect_by_func (priv->sink,
                                        _stream_volume_notify_cb,
                                        self);

  progress = mx_slider_get_value (MX_SLIDER (priv->volume_slider));
  volume = progress * PA_VOLUME_NORM;
  if (gvc_mixer_stream_set_volume (priv->sink, volume))
    gvc_mixer_stream_push_volume (priv->sink);
  else
    g_warning ("%s() failed", __FUNCTION__);

  g_signal_connect (priv->sink, "notify::volume",
                    G_CALLBACK (_stream_volume_notify_cb), self);

}

