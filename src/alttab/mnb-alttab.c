/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Moblin Netbook
 * Copyright © 2009, 2010, Intel Corporation.
 *
 * Author: Tomas Frydrych <tf@linux.intel.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

/*
 * Alt_tab machinery.
 *
 * This is the machinery we need for handling Alt+Tab
 *
 * To start with, Metacity has a passive grab on this key, so we hook up a
 * custom handler to its keybingins.
 *
 * Once we get the initial Alt+Tab, we establish a global key grab (we use
 * metacity API for this, as we need the regular bindings to be correctly
 * restored when we are finished). When the alt key is released, we
 * release the grab.
 */

#include <clutter/x11/clutter-x11.h>
#include <string.h>

#include "mnb-alttab.h"
#include "mnb-alttab-overlay.h"
#include "mnb-alttab-overlay-private.h"
#include "../moblin-netbook.h"
#include <display.h>

/*
 * Establishes an active kbd grab for us via the Meta API.
 */
gboolean
mnb_alttab_overlay_establish_keyboard_grab (MnbAlttabOverlay  *overlay,
                                            MetaDisplay  *display,
                                            MetaScreen   *screen,
                                            gulong        mask,
                                            guint         timestamp)
{
  MnbAlttabOverlayPrivate *priv = overlay->priv;

  if (meta_display_begin_grab_op (display,
                                  screen,
                                  NULL,
                                  META_GRAB_OP_KEYBOARD_TABBING_NORMAL,
                                  FALSE,
                                  FALSE,
                                  0,
                                  mask,
                                  timestamp,
                                  0, 0))
    {
      priv->in_alt_grab = TRUE;

      return TRUE;
    }

  g_warning ("Failed to grab keyboard");

  return FALSE;
}

/*
 * Data we need to pass through via our timeout and show callbacks.
 */
struct alt_tab_show_complete_data
{
  MnbAlttabOverlay *overlay;
  MetaDisplay      *display;
  MetaScreen       *screen;
  MetaWindow       *window;
  MetaKeyBinding   *binding;
  XEvent            xevent;
};

/*
 * Ends kbd grab, if one is currently in place.
 */
void
end_kbd_grab (MnbAlttabOverlay *overlay)
{
  MnbAlttabOverlayPrivate *priv   = overlay->priv;
  MutterPlugin            *plugin = moblin_netbook_get_plugin_singleton ();

  if (priv->in_alt_grab)
    {
      MetaScreen  *screen  = mutter_plugin_get_screen (plugin);
      MetaDisplay *display = meta_screen_get_display (screen);
      guint        timestamp;

      priv->in_alt_grab = FALSE;

      /*
       * Make sure our stamp is recent enough.
       */
      timestamp = meta_display_get_current_time_roundtrip (display);

      meta_display_end_grab_op (display, timestamp);
    }
}

static gboolean
alt_tab_slow_down_timeout_cb (gpointer data)
{
  MnbAlttabOverlay *overlay = MNB_ALTTAB_OVERLAY (data);

  if (mnb_alttab_overlay_tab_still_down (overlay))
    {
      return TRUE;
    }
  else
    {
      overlay->priv->slowdown_timeout_id = 0;
      return FALSE;
    }
}

/*
 * Callback for the timeout we use to test whether to show alttab or directly
 * move to next window.
 */
static gboolean
alt_tab_initial_timeout_cb (gpointer data)
{
  struct alt_tab_show_complete_data *alt_data = data;
  ClutterActor              *stage;
  Window                     xwin;
  MnbAlttabOverlayPrivate   *priv = alt_data->overlay->priv;
  MutterPlugin              *plugin = moblin_netbook_get_plugin_singleton ();

  stage = mutter_get_stage_for_screen (alt_data->screen);
  xwin  = clutter_x11_get_stage_window (CLUTTER_STAGE (stage));

  priv->waiting_for_timeout = FALSE;

  /*
   * Check wether the Alt key is still down; if so, show the AlttabOverlay,
   * and wait for the show-completed signal to process the Alt+Tab.
   *
   * If the compositor is currently disabled, ignore.
   */
  if (priv->alt_tab_down &&
      !moblin_netbook_compositor_disabled (plugin))
    {
      gboolean backward = FALSE;

      if (alt_data->xevent.xkey.state & ShiftMask)
        backward = !backward;

      if (!mnb_alttab_overlay_show (alt_data->overlay, backward))
        {
          /*
           * There was nothing to show (i.e., there is at most one application
           * out there.
           */
          end_kbd_grab (alt_data->overlay);
          g_free (data);
        }
    }
  else if (!priv->alt_tab_down)
    {
      MnbAlttabOverlay *overlay = alt_data->overlay;
      GList            *l = mnb_alttab_overlay_get_app_list (overlay);

      if (l && l->next)
        {
          mnb_alttab_overlay_activate_window (overlay,
                                              l->next->data,
                                              alt_data->xevent.xkey.time);
        }

      g_list_free (l);

      end_kbd_grab (alt_data->overlay);
      g_free (data);
    }

  /* One off */
  return FALSE;
}

/*
 * The handler for Alt+Tab that we register with metacity.
 *
 * NB: this is the only function we expose from this file for the alttab
 *     object.
 */
void
mnb_alttab_overlay_alt_tab_key_handler (MetaDisplay    *display,
                                        MetaScreen     *screen,
                                        MetaWindow     *window,
                                        XEvent         *event,
                                        MetaKeyBinding *binding,
                                        gpointer        data)
{
  MnbAlttabOverlay           *overlay = MNB_ALTTAB_OVERLAY (data);
  MnbAlttabOverlayPrivate    *priv    = overlay->priv;
  MutterPlugin               *plugin  = moblin_netbook_get_plugin_singleton ();
  MoblinNetbookPluginPrivate *ppriv   = MOBLIN_NETBOOK_PLUGIN (plugin)->priv;

  if (CLUTTER_ACTOR_IS_MAPPED (ppriv->notification_urgent))
    {
      /*
       * Ignore if we have urgent notifications (MB#6036)
       */
      if (priv->in_alt_grab)
        {
          if (CLUTTER_ACTOR_IS_VISIBLE (overlay))
            mnb_alttab_overlay_hide (overlay);

          end_kbd_grab (overlay);
          priv->alt_tab_down = FALSE;
        }

      return;
    }

  priv->alt_tab_down = TRUE;

  /* MNB_DBG_MARK(); */

  if (!priv->in_alt_grab)
    {
      mnb_alttab_overlay_establish_keyboard_grab (overlay, display, screen,
                                                  binding->mask,
                                                  event->xkey.time);
    }

  if (!priv->waiting_for_timeout &&
      (!CLUTTER_ACTOR_IS_VISIBLE (overlay) ||
       moblin_netbook_compositor_disabled (plugin)))
    {
      struct alt_tab_show_complete_data *alt_data;

      /*
       * If the alttab is not visible we want to show it; this is, however,
       * complicated a bit:
       *
       *  If the user just hits and immediately releases Alt+Tab, we need to
       *  avoid initiating the effects alltogether, otherwise we just get bit of
       *  a flicker as the AlttabOverlay starts coming out and immediately
       *  disappears.
       *
       *  So, instead of showing the alttab, we install a timeout to introduce
       *  a short delay, so we can test whether the Alt key is still down. We
       *  then handle the actual show from the timeout.
       */
      alt_data = g_new0 (struct alt_tab_show_complete_data, 1);
      alt_data->display  = display;
      alt_data->screen   = screen;
      alt_data->binding  = binding;
      alt_data->overlay = overlay;

      memcpy (&alt_data->xevent, event, sizeof (XEvent));

      g_timeout_add (100, alt_tab_initial_timeout_cb, alt_data);
      priv->waiting_for_timeout = TRUE;
      return;
    }
  else
    {
      gboolean backward = FALSE;

      /*
       * Workaround autorepeat madness -- install a 100ms timeout; if we get
       * here while the timeout is still in place, ignore the button press
       * (which is almost certainly due to auto-repeat). When the timeout
       * triggers, check whether the Tab key is still down. If not, remove the
       * tiemeout so that the next key press will be handled here; if the key
       * is still down, leave the timeout in place and keep ignoring the
       * events.
       *
       * The actual advancing is handled by the autoscroll timeouts.
       */
      if (priv->slowdown_timeout_id || priv->waiting_for_timeout)
        return;

      priv->slowdown_timeout_id =
        g_timeout_add (100,
                       alt_tab_slow_down_timeout_cb, overlay);

      if (event->xkey.state & ShiftMask)
        backward = !backward;

      mnb_alttab_reset_autoscroll (overlay, backward);
      mnb_alttab_overlay_advance (overlay, backward);
    }
}

void
mnb_alttab_overlay_alt_tab_select_handler (MetaDisplay    *display,
                                           MetaScreen     *screen,
                                           MetaWindow     *window,
                                           XEvent         *event,
                                           MetaKeyBinding *binding,
                                           gpointer        data)
{
  MnbAlttabOverlay           *overlay = MNB_ALTTAB_OVERLAY (data);
  MnbAlttabOverlayPrivate    *priv    = overlay->priv;
  MutterPlugin               *plugin  = moblin_netbook_get_plugin_singleton ();
  MoblinNetbookPluginPrivate *ppriv   = MOBLIN_NETBOOK_PLUGIN (plugin)->priv;

  end_kbd_grab (overlay);

  priv->in_alt_grab = FALSE;
  priv->alt_tab_down = FALSE;

  if (CLUTTER_ACTOR_IS_MAPPED (ppriv->notification_urgent))
    {
      /*
       * Ignore if we have urgent notifications (MB#6036)
       */
      return;
    }

  if (!overlay->priv->waiting_for_timeout)
    mnb_alttab_overlay_activate_selection (overlay, event->xkey.time);
}

void
mnb_alttab_overlay_alt_tab_cancel_handler (MetaDisplay    *display,
                                           MetaScreen     *screen,
                                           MetaWindow     *window,
                                           XEvent         *event,
                                           MetaKeyBinding *binding,
                                           gpointer        data)
{
  MnbAlttabOverlay        *overlay = MNB_ALTTAB_OVERLAY (data);
  MnbAlttabOverlayPrivate *priv    = overlay->priv;

  MNB_DBG_MARK();

  end_kbd_grab (overlay);

  priv->alt_tab_down = FALSE;

  if (CLUTTER_ACTOR_IS_VISIBLE (overlay))
    mnb_alttab_overlay_hide (overlay);
}

