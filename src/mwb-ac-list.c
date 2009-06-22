/* mwb-ac-list.c */
/*
 * Copyright (c) 2009 Intel Corp.
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

/* Borrowed from the moblin-web-browser project */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cogl/cogl-pango.h>
#include <string.h>
#include <mhs/mhs.h>
#include <glib/gi18n.h>
#include "mwb-ac-list.h"
#include "mwb-separator.h"
#include "mwb-utils.h"

G_DEFINE_TYPE (MwbAcList, mwb_ac_list, NBTK_TYPE_WIDGET);

#define MWB_AC_LIST_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), MWB_TYPE_AC_LIST, \
   MwbAcListPrivate))

enum
{
  PROP_0,

  PROP_SEARCH_TEXT,
  PROP_SELECTION
};

#define MWB_AC_LIST_MAX_ENTRIES 15
#define MWB_AC_LIST_ICON_SIZE 16

struct _MwbAcListPrivate
{
  MhsHistory    *history;

  MhsPrefs      *prefs;
  gint           prefs_branch;
  guint          prefs_branch_changed_handler;
  gboolean       complete_domains;
  gboolean       search_in_automagic;

  guint          result_received_handler;
  guint32        search_id;

  GArray        *entries;

  gint           tallest_entry;

  guint            clear_timeout;
  gfloat           last_height;
  gdouble          anim_progress;
  ClutterTimeline *timeline;

  NbtkWidget    *separator;

  ClutterColor   match_color;

  GString       *search_text;
  /* Keeps track of the old search text length so that we can clear
     the favicon cache when it shrinks */
  guint          old_search_length;
  gint           selection;

  /* Cache of favicons. Maps from a gchar* to a
     MwbAcListCachedFavicon*. This is cleared every time the search
     string shrinks. That way it avoids refetching icons while the
     user is typing which is likely to consistently return the same
     results */
  GHashTable    *favicon_cache;

  CoglHandle     search_engine_icon;
  gchar         *search_engine_name;
  gchar         *search_engine_url;
};

typedef struct _MwbAcListEntry MwbAcListEntry;

struct _MwbAcListEntry
{
  NbtkWidget *label_actor;
  gchar *label_text;
  gchar *url;
  gint match_start, match_end;
  CoglHandle texture;

  /* This is used for drawing the highlight and also for picking. Its
     color gets set to the highlight color but it will not be painted
     if the row is not highlighted */
  NbtkWidget *highlight_widget;
  guint highlight_motion_handler;
  guint highlight_clicked_handler;
};

typedef struct _MwbAcListCachedFavicon MwbAcListCachedFavicon;

struct _MwbAcListCachedFavicon
{
  gchar *url;

  /* May be COGL_INVALID_HANDLE if the favicon isn't received yet */
  CoglHandle texture;

  guint favicon_handler;

  MwbAcList *ac_list;
};

static const struct
{
  const gchar *name;
  guint offset;
}
mwb_ac_list_boolean_prefs[] =
  {
    { "complete_domains",
      G_STRUCT_OFFSET (MwbAcListPrivate, complete_domains) },
    { "search_in_automagic",
      G_STRUCT_OFFSET (MwbAcListPrivate, search_in_automagic) }
  };

static void mwb_ac_list_clear_entries (MwbAcList *self);

static void mwb_ac_list_result_received_cb (MhsHistory *history,
                                            guint32 search_id,
                                            const gchar *value,
                                            const gchar *comment,
                                            MwbAcList *self);

static void mwb_ac_list_add_default_entries (MwbAcList *self);

static void mwb_ac_list_forget_search_engine (MwbAcList *self);

#define MWB_AC_LIST_SEARCH_ENTRY    0
#define MWB_AC_LIST_HOSTNAME_ENTRY  1
#define MWB_AC_LIST_N_FIXED_ENTRIES 2

enum
{
  ACTIVATE_SIGNAL,

  LAST_SIGNAL
};

static guint ac_list_signals[LAST_SIGNAL] = { 0, };

static void
mwb_ac_list_get_property (GObject *object, guint property_id,
                          GValue *value, GParamSpec *pspec)
{
  MwbAcList *ac_list = MWB_AC_LIST (object);

  switch (property_id)
    {
    case PROP_SEARCH_TEXT:
      g_value_set_string (value, mwb_ac_list_get_search_text (ac_list));
      break;

    case PROP_SELECTION:
      g_value_set_int (value, mwb_ac_list_get_selection (ac_list));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
mwb_ac_list_set_property (GObject *object, guint property_id,
                          const GValue *value, GParamSpec *pspec)
{
  MwbAcList *ac_list = MWB_AC_LIST (object);

  switch (property_id)
    {
    case PROP_SEARCH_TEXT:
      mwb_ac_list_set_search_text (ac_list, g_value_get_string (value));
      break;

    case PROP_SELECTION:
      mwb_ac_list_set_selection (ac_list, g_value_get_int (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
mwb_ac_list_dispose (GObject *object)
{
  MwbAcListPrivate *priv = MWB_AC_LIST (object)->priv;

  if (priv->clear_timeout)
    {
      g_source_remove (priv->clear_timeout);
      priv->clear_timeout = 0;
    }

  if (priv->timeline)
    {
      clutter_timeline_stop (priv->timeline);
      g_object_unref (priv->timeline);
      priv->timeline = NULL;
    }

  if (priv->separator)
    {
      clutter_actor_unparent (CLUTTER_ACTOR (priv->separator));
      priv->separator = NULL;
    }

  mwb_ac_list_clear_entries (MWB_AC_LIST (object));

  mwb_ac_list_forget_search_engine (MWB_AC_LIST (object));

  /* The favicon cache holds references to ClutterTextures so it
     should be cleared to unref them */
  g_hash_table_remove_all (priv->favicon_cache);

  if (priv->history)
    {
      g_signal_handler_disconnect (priv->history,
                                   priv->result_received_handler);
      if (priv->search_id)
        mhs_history_stop_ac_search (priv->history,
                                    priv->search_id,
                                    NULL);
      g_object_unref (priv->history);
      priv->history = NULL;
    }

  if (priv->prefs)
    {
      if (priv->prefs_branch != -1)
        {
          guint ns_result;

          mhs_prefs_release_branch (priv->prefs, priv->prefs_branch,
                                    &ns_result, NULL);
          priv->prefs_branch = -1;
        }

      if (priv->prefs_branch_changed_handler)
        {
          g_signal_handler_disconnect (priv->prefs,
                                       priv->prefs_branch_changed_handler);
          priv->prefs_branch_changed_handler = 0;
        }

      g_object_unref (priv->prefs);
      priv->prefs = NULL;
    }

  G_OBJECT_CLASS (mwb_ac_list_parent_class)->dispose (object);
}

static void
mwb_ac_list_finalize (GObject *object)
{
  MwbAcListPrivate *priv = MWB_AC_LIST (object)->priv;

  g_array_free (priv->entries, TRUE);

  g_string_free (priv->search_text, TRUE);

  g_hash_table_unref (priv->favicon_cache);

  if (priv->search_engine_name)
    g_free (priv->search_engine_name);
  if (priv->search_engine_url)
    g_free (priv->search_engine_url);

  G_OBJECT_CLASS (mwb_ac_list_parent_class)->finalize (object);
}

static gboolean
mwb_ac_list_stristr (const gchar *haystack, const gchar *needle,
                     gint *start_ret, gint *end_ret)
{
  const gchar *search_start;

  /* Looks for the first occurence of needle in haystack both of which
     are UTF-8 strings. Case is ignored as far as allowed by
     g_unichar_tolower. */

  /* Try each position in haystack */
  for (search_start = haystack;
       *search_start;
       search_start = g_utf8_next_char (search_start))
    {
      const gchar *haystack_ptr = search_start;
      const gchar *needle_ptr = needle;

      while (TRUE)
        {
          if (*needle_ptr == 0)
            {
              /* If we've reached the end of the needle then we have
                 found a match */
              if (start_ret)
                *start_ret = search_start - haystack;
              if (end_ret)
                *end_ret = haystack_ptr - haystack;
              return TRUE;
            }
          else if (*haystack_ptr == 0)
            break;
          else if (g_unichar_tolower (g_utf8_get_char (haystack_ptr))
                   != g_unichar_tolower (g_utf8_get_char (needle_ptr)))
            break;
          else
            {
              haystack_ptr = g_utf8_next_char (haystack_ptr);
              needle_ptr = g_utf8_next_char (needle_ptr);
            }
        }
    }

  return FALSE;
}

static void
mwb_ac_list_cached_favicon_free (MwbAcListCachedFavicon *cached_favicon)
{
  g_free (cached_favicon->url);

  if (cached_favicon->texture)
    cogl_handle_unref (cached_favicon->texture);
  if (cached_favicon->favicon_handler)
    mhs_history_cancel (cached_favicon->ac_list->priv->history,
                        cached_favicon->favicon_handler);

  g_slice_free (MwbAcListCachedFavicon, cached_favicon);
}

static gfloat
mwb_ac_list_get_height (MwbAcList *self)
{
  MwbAcListPrivate *priv = self->priv;
  gint n_entries = MIN (MWB_AC_LIST_MAX_ENTRIES, priv->entries->len);
  gfloat separator_height = 0.f;

  if (priv->separator)
    clutter_actor_get_preferred_height (CLUTTER_ACTOR (priv->separator), -1,
                                        NULL, &separator_height);

  if (priv->timeline)
    return ((1.0 - priv->anim_progress) * priv->last_height) +
            (priv->anim_progress *
             ((n_entries * priv->tallest_entry) +
              ((n_entries - 1) * separator_height)));
  else
    return (n_entries * priv->tallest_entry) +
           ((n_entries - 1) * separator_height);
}

static void
mwb_ac_list_get_preferred_height (ClutterActor *actor,
                                  gfloat        for_width,
                                  gfloat       *min_height_p,
                                  gfloat       *natural_height_p)
{
  MwbAcList *self = MWB_AC_LIST (actor);

  if (min_height_p)
    *min_height_p = 0;

  if (natural_height_p)
    {
      NbtkPadding padding;

      nbtk_widget_get_padding (NBTK_WIDGET (self), &padding);

      *natural_height_p = mwb_ac_list_get_height (self) +
                          padding.top + padding.bottom;
      if (*natural_height_p < 0)
        *natural_height_p = 0;
    }
}

static void
mwb_ac_list_paint (ClutterActor *actor)
{
  MwbAcListPrivate *priv = MWB_AC_LIST (actor)->priv;
  NbtkPadding padding;
  ClutterGeometry geom;
  gfloat separator_height = 0.;
  gfloat ypos;
  gint i;

  /* Chain up to get the background */
  CLUTTER_ACTOR_CLASS (mwb_ac_list_parent_class)->paint (actor);

  nbtk_widget_get_padding (NBTK_WIDGET (actor), &padding);

  clutter_actor_get_allocation_geometry (actor, &geom);

  ypos = padding.top;
  geom.height -= (gint)padding.bottom;

  if (priv->separator)
    clutter_actor_get_preferred_height (CLUTTER_ACTOR (priv->separator), -1,
                                        NULL, &separator_height);

  for (i = 0;
       i < priv->entries->len
         && ypos + priv->tallest_entry <= geom.height + 1e-6;
       i++)
    {
      MwbAcListEntry *entry = &g_array_index (priv->entries, MwbAcListEntry, i);

      /* Only paint the highlight widget if the row is selected */
      if (entry->highlight_widget && priv->selection == i
          && CLUTTER_ACTOR_IS_MAPPED (CLUTTER_ACTOR (entry->highlight_widget)))
        clutter_actor_paint (CLUTTER_ACTOR (entry->highlight_widget));

      if (entry->texture)
        {
          int y = ((int) ypos + priv->tallest_entry / 2
                   - MWB_AC_LIST_ICON_SIZE / 2);

          cogl_set_source_texture (entry->texture);
          cogl_rectangle (padding.left,
                          y,
                          padding.left + MWB_AC_LIST_ICON_SIZE,
                          y + MWB_AC_LIST_ICON_SIZE);
        }

      if (entry->label_actor
          && CLUTTER_ACTOR_IS_MAPPED (CLUTTER_ACTOR (entry->label_actor)))
        clutter_actor_paint (CLUTTER_ACTOR (entry->label_actor));

      /* Temporarily move the separator with cogl_translate so that we can
         paint it in the right place */
      if (ypos + priv->tallest_entry + separator_height <= geom.height + 1e-6)
        {
          cogl_push_matrix ();
          cogl_translate (0.0f, ypos + priv->tallest_entry, 0.0f);
          clutter_actor_paint (CLUTTER_ACTOR (priv->separator));
          cogl_pop_matrix ();
        }

      ypos += priv->tallest_entry + separator_height;
    }
}

static void
mwb_ac_list_pick (ClutterActor *actor, const ClutterColor *color)
{
  MwbAcListPrivate *priv = MWB_AC_LIST (actor)->priv;
  gfloat separator_height = 0;
  gfloat ypos;
  ClutterGeometry geom;
  NbtkPadding padding;
  gint i;

  /* Chain up so we get a bounding box painted */
  CLUTTER_ACTOR_CLASS (mwb_ac_list_parent_class)->pick (actor, color);

  if (priv->separator)
    clutter_actor_get_preferred_height (CLUTTER_ACTOR (priv->separator), -1,
                                        NULL, &separator_height);

  nbtk_widget_get_padding (NBTK_WIDGET (actor), &padding);

  clutter_actor_get_allocation_geometry (actor, &geom);

  ypos = padding.top;
  geom.height -= (gint)padding.bottom;

  /* We only want to pick on the highlight boxes. We will paint the
     highlight box regardless of whether the row is selected so that
     we can pick on all rows */
  for (i = 0;
       i < priv->entries->len
         && ypos + priv->tallest_entry <= geom.height + 1e-6;
       i++)
    {
      MwbAcListEntry *entry = &g_array_index (priv->entries, MwbAcListEntry, i);

      /* Only paint the highlight widget if the row is selected */
      if (entry->highlight_widget
          && CLUTTER_ACTOR_IS_MAPPED (CLUTTER_ACTOR (entry->highlight_widget)))
        clutter_actor_paint (CLUTTER_ACTOR (entry->highlight_widget));

      ypos += priv->tallest_entry + separator_height;
    }
}

static gboolean
mwb_ac_list_set_selection_for_highlight (MwbAcList *ac_list,
                                         ClutterActor *actor)
{
  MwbAcListPrivate *priv = ac_list->priv;
  int i;

  for (i = 0; i < priv->entries->len; i++)
    {
      MwbAcListEntry *entry = &g_array_index (priv->entries, MwbAcListEntry, i);

      if (CLUTTER_ACTOR (entry->highlight_widget) == actor)
        {
          mwb_ac_list_set_selection (ac_list, i);

          return TRUE;
        }
    }

  return FALSE;
}

static gboolean
mwb_ac_list_highlight_motion_cb (ClutterActor *actor,
                                 ClutterMotionEvent *event,
                                 MwbAcList *ac_list)
{
  return mwb_ac_list_set_selection_for_highlight (ac_list, actor);
}

static gboolean
mwb_ac_list_highlight_clicked_cb (ClutterActor *actor,
                                  ClutterMotionEvent *event,
                                  MwbAcList *ac_list)
{
  if (mwb_ac_list_set_selection_for_highlight (ac_list, actor))
    {
      g_signal_emit (ac_list, ac_list_signals[ACTIVATE_SIGNAL], 0);
      return TRUE;
    }
  else
    return FALSE;
}

static void
mwb_ac_list_forget_search_engine (MwbAcList *self)
{
  MwbAcListPrivate *priv = self->priv;

  if (priv->search_engine_name)
    {
      g_free (priv->search_engine_name);
      priv->search_engine_name = NULL;
    }
  if (priv->search_engine_url)
    {
      g_free (priv->search_engine_url);
      priv->search_engine_url = NULL;
    }

  if (priv->search_engine_icon != COGL_INVALID_HANDLE)
    {
      cogl_handle_unref (priv->search_engine_icon);
      priv->search_engine_icon = COGL_INVALID_HANDLE;
    }
}

static void
mwb_ac_list_update_search_engine (MwbAcList *ac_list)
{
  MwbAcListPrivate *priv = ac_list->priv;

  mwb_ac_list_forget_search_engine (ac_list);

  if (priv->prefs && priv->prefs_branch != -1)
    {
      GError *error = NULL;
      guint ns_result;

      if (mhs_prefs_branch_get_char (priv->prefs, priv->prefs_branch,
                                     "search_engine.selected",
                                     &priv->search_engine_name,
                                     &ns_result,
                                     &error))
        {
          gchar *url_pref_name = g_strconcat ("search_engine.data.",
                                              priv->search_engine_name,
                                              ".url",
                                              NULL);

          if (mhs_prefs_branch_get_char (priv->prefs, priv->prefs_branch,
                                         url_pref_name,
                                         &priv->search_engine_url,
                                         &ns_result,
                                         &error))
            {
              /* Get a filename-safe version of the search engine name */
              gchar *clean_name =
                g_malloc (strlen (priv->search_engine_name) + 4);
              gchar *path;
              gchar *src, *dst = clean_name;
              for (src = priv->search_engine_name; *src; src++)
                if ((*src >= 'A' && *src <= 'Z') ||
                    (*src >= 'a' && *src <= 'z') ||
                    (*src >= '0' && *src <= '9'))
                  *(dst++) = *src;
              strcpy (dst, ".ico");

              /* Try to load the icon from the user's home directory */
              path = g_build_filename (g_get_home_dir (),
                                       ".moblin-web-browser",
                                       "search-icons",
                                       clean_name,
                                       NULL);

              priv->search_engine_icon =
                cogl_texture_new_from_file (path,
                                            COGL_TEXTURE_NONE,
                                            COGL_PIXEL_FORMAT_ANY,
                                            &error);
              g_free (path);

              g_clear_error (&error);

              g_free (clean_name);
            }
          else
            {
              g_warning ("%s", error->message);
              g_clear_error (&error);
            }

          g_free (url_pref_name);
        }
      else
        {
          g_warning ("%s", error->message);
          g_clear_error (&error);
        }
    }
}

static void
mwb_ac_list_prefs_branch_changed_cb (MhsPrefs *prefs,
                                     gint id,
                                     const gchar *domain,
                                     MwbAcList *ac_list)
{
  MwbAcListPrivate *priv = ac_list->priv;

  if (id == priv->prefs_branch)
    {
      int i;

      /* If any of the search engine prefs change then update our
         search engine data */
      if (g_str_has_prefix (domain, "search_engine."))
        {
          mwb_ac_list_update_search_engine (ac_list);
          return;
        }

      for (i = 0; i < G_N_ELEMENTS (mwb_ac_list_boolean_prefs); i++)
        if (!strcmp (domain, mwb_ac_list_boolean_prefs[i].name))
          {
            GError *error = NULL;
            gboolean *val = (gboolean *) ((guchar *) priv +
                                          mwb_ac_list_boolean_prefs[i].offset);
            guint ns_result;

            if (!mhs_prefs_branch_get_bool (priv->prefs,
                                            priv->prefs_branch,
                                            mwb_ac_list_boolean_prefs[i].name,
                                            val,
                                            &ns_result,
                                            &error))
              {
                g_warning ("%s", error->message);
                g_clear_error (&error);
              }

            break;
          }
    }
}

static void
mwb_ac_list_update_entry (MwbAcList *ac_list,
                          MwbAcListEntry *entry)
{
  MwbAcListPrivate *priv = ac_list->priv;
  gfloat label_height;
  ClutterActor *text;

  if (entry->highlight_widget == NULL)
    {
      entry->highlight_widget = g_object_new (NBTK_TYPE_BIN,
                                              "reactive", TRUE,
                                              NULL);
      entry->highlight_motion_handler
        = g_signal_connect (entry->highlight_widget, "motion-event",
                            G_CALLBACK (mwb_ac_list_highlight_motion_cb),
                            ac_list);
      entry->highlight_clicked_handler
        = g_signal_connect (entry->highlight_widget, "button-press-event",
                            G_CALLBACK (mwb_ac_list_highlight_clicked_cb),
                            ac_list);
      clutter_actor_set_parent (CLUTTER_ACTOR (entry->highlight_widget),
                                CLUTTER_ACTOR (ac_list));
    }

  if (entry->label_actor)
    clutter_actor_unparent (CLUTTER_ACTOR (entry->label_actor));

  entry->label_actor = nbtk_label_new (entry->label_text);
  clutter_actor_set_parent (CLUTTER_ACTOR (entry->label_actor),
                            CLUTTER_ACTOR (ac_list));

  text = nbtk_label_get_clutter_text (NBTK_LABEL (entry->label_actor));

  if (entry->match_end > 0)
    {
      PangoAttrList *attr_list = pango_attr_list_new ();
      PangoAttribute *bold_attr = pango_attr_weight_new (PANGO_WEIGHT_BOLD);
      PangoAttribute *color_attr
        = pango_attr_foreground_new (priv->match_color.red * 65535 / 255,
                                     priv->match_color.green * 65535 / 255,
                                     priv->match_color.blue * 65535 / 255);
      bold_attr->start_index = entry->match_start;
      bold_attr->end_index = entry->match_end;
      pango_attr_list_insert (attr_list, bold_attr);
      color_attr->start_index = entry->match_start;
      color_attr->end_index = entry->match_end;
      pango_attr_list_insert (attr_list, color_attr);
      clutter_text_set_attributes (CLUTTER_TEXT (text), attr_list);
      pango_attr_list_unref (attr_list);
    }

  clutter_text_set_ellipsize (CLUTTER_TEXT (text), PANGO_ELLIPSIZE_MIDDLE);

  clutter_actor_get_preferred_height (CLUTTER_ACTOR (entry->label_actor), -1,
                                      NULL, &label_height);
  if (label_height > priv->tallest_entry)
    priv->tallest_entry = label_height;
}

static void
mwb_ac_list_allocate (ClutterActor           *actor,
                      const ClutterActorBox  *box,
                      ClutterAllocationFlags  flags)
{
  MwbAcList *self = MWB_AC_LIST (actor);
  MwbAcListPrivate *priv = self->priv;
  NbtkPadding padding;
  gint i;
  gfloat ypos;
  gfloat separator_height = 0;

  CLUTTER_ACTOR_CLASS (mwb_ac_list_parent_class)->allocate (actor, box, flags);

  nbtk_widget_get_padding (NBTK_WIDGET (actor), &padding);

  ypos = padding.top;

  if (priv->separator)
    {
      ClutterActorBox separator_box;

      clutter_actor_get_preferred_height (CLUTTER_ACTOR (priv->separator), -1,
                                          NULL, &separator_height);

      /* The separator is actually going to be draw multiple times at
         different locations using cogl_translate, so we just want to
         allocate it with the right width at the top */
      separator_box.x1 = 0;
      separator_box.x2 = box->x2 - box->x1;
      separator_box.y1 = 0;
      separator_box.y2 = separator_height;

      clutter_actor_allocate (CLUTTER_ACTOR (priv->separator),
                              &separator_box, flags);
    }

  /* Allocate all of the labels and highlight widgets */
  for (i = 0; i < priv->entries->len; i++)
    {
      MwbAcListEntry *entry = &g_array_index (priv->entries, MwbAcListEntry, i);

      /* Put the label to the right of the icon and vertically
         centered within the row size */
      if (entry->label_actor)
        {
          ClutterActorBox label_box;
          gfloat height;
          ClutterActor *label_actor = CLUTTER_ACTOR (entry->label_actor);

          clutter_actor_get_preferred_height (label_actor, -1, NULL, &height);

          label_box.x1 = (padding.left + (gfloat)MWB_AC_LIST_ICON_SIZE);
          label_box.x2 = box->x2 - box->x1 - padding.right;
          label_box.y1
            = (MWB_PIXBOUND (priv->tallest_entry / 2.0f - height / 2.0f)
               + ypos);
          label_box.y2 = (label_box.y1 + (gfloat)priv->tallest_entry);

          clutter_actor_allocate (label_actor, &label_box, flags);
        }

      /* Let the highlight widget fill the entire row */
      if (entry->highlight_widget)
        {
          ClutterActorBox highlight_box;
          const int BORDER_PAD = 1;  /* yes, this is a temporary hack */

          highlight_box.x1 = BORDER_PAD;
          highlight_box.x2 = box->x2 - box->x1 - BORDER_PAD;
          highlight_box.y1 = ypos;
          highlight_box.y2 = ypos + (gfloat)priv->tallest_entry;

          clutter_actor_allocate (CLUTTER_ACTOR (entry->highlight_widget),
                                  &highlight_box, flags);
        }

      ypos += (gfloat)priv->tallest_entry + separator_height;
    }
}

static void
mwb_ac_list_update_all_entries (MwbAcList *self)
{
  MwbAcListPrivate *priv = self->priv;
  gint i;

  for (i = 0; i < priv->entries->len; i++)
    {
      MwbAcListEntry *entry
        = &g_array_index (priv->entries, MwbAcListEntry, i);
      mwb_ac_list_update_entry (self, entry);
    }
}

static void
mwb_ac_list_style_changed_cb (NbtkWidget *widget)
{
  MwbAcList *self = MWB_AC_LIST (widget);
  MwbAcListPrivate *priv = self->priv;
  ClutterColor *color = NULL;

  nbtk_stylable_get (NBTK_STYLABLE (self),
                     "color", &color,
                     NULL);

  /* The 'color' property is used to set the search match color. This
     should really be done with a different property but it will be
     left as a FIXME until Nbtk/libccss gains the ability to add
     custom color properties */
  if (color)
    {
      if (!clutter_color_equal (&priv->match_color, color))
        {
          priv->match_color = *color;
          /* The color has changed so we need to reset the pango
             attributes on all of the entries */
          mwb_ac_list_update_all_entries (self);
        }

      clutter_color_free (color);
    }
}

static void
mwb_ac_list_map (ClutterActor *actor)
{
  gint i;
  MwbAcListPrivate *priv = MWB_AC_LIST (actor)->priv;

  CLUTTER_ACTOR_CLASS (mwb_ac_list_parent_class)->map (actor);

  clutter_actor_map (CLUTTER_ACTOR (priv->separator));

  for (i = 0; i < priv->entries->len; i++)
    {
      MwbAcListEntry *entry = &g_array_index (priv->entries, MwbAcListEntry, i);
      clutter_actor_map (CLUTTER_ACTOR (entry->label_actor));
      clutter_actor_map (CLUTTER_ACTOR (entry->highlight_widget));
    }
}

static void
mwb_ac_list_unmap (ClutterActor *actor)
{
  gint i;
  MwbAcListPrivate *priv = MWB_AC_LIST (actor)->priv;

  CLUTTER_ACTOR_CLASS (mwb_ac_list_parent_class)->unmap (actor);

  clutter_actor_unmap (CLUTTER_ACTOR (priv->separator));

  for (i = 0; i < priv->entries->len; i++)
    {
      MwbAcListEntry *entry = &g_array_index (priv->entries, MwbAcListEntry, i);
      clutter_actor_unmap (CLUTTER_ACTOR (entry->label_actor));
      clutter_actor_unmap (CLUTTER_ACTOR (entry->highlight_widget));
    }
}

static void
mwb_ac_list_class_init (MwbAcListClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = (ClutterActorClass *) klass;
  GParamSpec *pspec;

  g_type_class_add_private (klass, sizeof (MwbAcListPrivate));

  object_class->dispose = mwb_ac_list_dispose;
  object_class->finalize = mwb_ac_list_finalize;
  object_class->get_property = mwb_ac_list_get_property;
  object_class->set_property = mwb_ac_list_set_property;

  actor_class->get_preferred_height = mwb_ac_list_get_preferred_height;
  actor_class->paint = mwb_ac_list_paint;
  actor_class->pick = mwb_ac_list_pick;
  actor_class->allocate = mwb_ac_list_allocate;
  actor_class->map = mwb_ac_list_map;
  actor_class->unmap = mwb_ac_list_unmap;

  pspec = g_param_spec_string ("search-text", "Search Text",
                               "The text to auto complete on",
                               "",
                               G_PARAM_READWRITE |
                               G_PARAM_STATIC_NAME |
                               G_PARAM_STATIC_NICK |
                               G_PARAM_STATIC_BLURB);
  g_object_class_install_property (object_class, PROP_SEARCH_TEXT, pspec);

  pspec = g_param_spec_int ("selection", "Selection",
                            "The currently selected row or -1 for no selection",
                            -1, G_MAXINT, -1,
                            G_PARAM_READWRITE |
                            G_PARAM_STATIC_NAME |
                            G_PARAM_STATIC_NICK |
                            G_PARAM_STATIC_BLURB);
  g_object_class_install_property (object_class, PROP_SELECTION, pspec);

  ac_list_signals[ACTIVATE_SIGNAL] =
    g_signal_new ("activate",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (MwbAcListClass, activate),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}

static void
mwb_ac_list_init (MwbAcList *self)
{
  MwbAcListPrivate *priv = self->priv = MWB_AC_LIST_PRIVATE (self);
  GError *error = NULL;
  guint ns_result;

  priv->entries = g_array_new (FALSE, TRUE, sizeof (MwbAcListEntry));

  priv->history = mhs_history_new ();

  priv->result_received_handler =
    g_signal_connect (priv->history, "ac-result-received",
                      G_CALLBACK (mwb_ac_list_result_received_cb),
                      self);

  priv->search_text = g_string_new ("");

  priv->selection = -1;

  priv->favicon_cache
    = g_hash_table_new_full (g_str_hash,
                             g_str_equal,
                             NULL,
                             (GDestroyNotify) mwb_ac_list_cached_favicon_free);

  priv->separator = mwb_separator_new ();
  clutter_actor_set_parent (CLUTTER_ACTOR (priv->separator),
                            CLUTTER_ACTOR (self));

  g_object_set (G_OBJECT (self), "clip-to-allocation", TRUE, NULL);

  /* Initialise the preference observers */
  priv->prefs = mhs_prefs_new ();
  if (mhs_prefs_get_branch (priv->prefs, "mwb.", &priv->prefs_branch,
                            &ns_result, &error))
    {
      int i;

      /* Add the boolean observers */
      for (i = 0; i < G_N_ELEMENTS (mwb_ac_list_boolean_prefs); i++)
        {
          const gchar *name = mwb_ac_list_boolean_prefs[i].name;
          gboolean *val = (gboolean *) ((guchar *) priv +
                                        mwb_ac_list_boolean_prefs[i].offset);

          if (!mhs_prefs_branch_get_bool (priv->prefs,
                                          priv->prefs_branch,
                                          name,
                                          val,
                                          &ns_result,
                                          &error)
              || !mhs_prefs_branch_add_observer (priv->prefs,
                                                 priv->prefs_branch,
                                                 name,
                                                 &ns_result,
                                                 &error))
            {
              g_warning ("%s", error->message);
              g_clear_error (&error);
            }
        }

      /* Add the search engine observer */
      if (!mhs_prefs_branch_add_observer (priv->prefs,
                                          priv->prefs_branch,
                                          "search_engine.",
                                          &ns_result,
                                          &error))
        {
          g_warning ("%s", error->message);
          g_clear_error (&error);
        }

      mwb_ac_list_update_search_engine (self);
    }
  else
    {
      g_warning ("%s", error->message);
      g_clear_error (&error);
      priv->prefs_branch = -1;
    }

  priv->prefs_branch_changed_handler =
    g_signal_connect (priv->prefs, "branch-changed",
                      G_CALLBACK (mwb_ac_list_prefs_branch_changed_cb),
                      self);

  g_signal_connect (self, "style-changed",
                    G_CALLBACK (mwb_ac_list_style_changed_cb), NULL);
}

NbtkWidget*
mwb_ac_list_new (void)
{
  return NBTK_WIDGET (g_object_new (MWB_TYPE_AC_LIST, NULL));
}

static void
mwb_ac_list_get_favicon_cb (MhsHistory *history,
                            const gchar *mime_type,
                            const guint8 *data,
                            guint data_len,
                            const GError *error,
                            gpointer user_data)
{
  MwbAcListCachedFavicon *cached_favicon = user_data;
  MwbAcList *ac_list = cached_favicon->ac_list;
  MwbAcListPrivate *priv = ac_list->priv;

  if (error)
    {
      /* We don't care if the icon isn't available */
      if (error->domain != MHS_ERROR
          || error->code != MHS_ERROR_NOTAVAILABLE)
        g_warning ("favicon error: %s", error->message);
    }
  else
    {
      GError *texture_error = NULL;

      cached_favicon->texture = mwb_utils_image_to_texture (data, data_len,
                                                            &texture_error);
      if (cached_favicon->texture != COGL_INVALID_HANDLE)
        {
          guint i;

          /* Set all of the entries that correspond to this URL */
          for (i = MWB_AC_LIST_N_FIXED_ENTRIES; i < priv->entries->len; i++)
            {
              MwbAcListEntry *entry = &g_array_index (priv->entries,
                                                      MwbAcListEntry, i);

              if (entry->url && !strcmp (entry->url, cached_favicon->url))
                {
                  entry->texture = cogl_handle_ref (cached_favicon->texture);
                  clutter_actor_queue_redraw (CLUTTER_ACTOR (ac_list));
                }
            }
        }
      else
        {
          g_warning ("favicon error: %s", texture_error->message);
          g_error_free (texture_error);
        }
    }

  cached_favicon->favicon_handler = 0;
}

static void
mwb_ac_list_new_frame_cb (ClutterTimeline *timeline,
                          guint            msecs,
                          MwbAcList       *self)
{
  MwbAcListPrivate *priv = self->priv;
  priv->anim_progress = clutter_timeline_get_progress (timeline);
  clutter_actor_queue_relayout (CLUTTER_ACTOR (self));
}

static void
mwb_ac_list_completed_cb (ClutterTimeline *timeline,
                          MwbAcList       *self)
{
  MwbAcListPrivate *priv = self->priv;

  g_object_unref (priv->timeline);
  priv->timeline = NULL;

  priv->last_height = mwb_ac_list_get_height (self);
  clutter_actor_queue_relayout (CLUTTER_ACTOR (self));
}

static void
mwb_ac_list_start_transition (MwbAcList *self)
{
  MwbAcListPrivate *priv = self->priv;

  if (priv->timeline)
    return;

  priv->timeline = clutter_timeline_new (100);
  priv->anim_progress = 0.0;

  /* Set a small delay on the timeline to allow new results to come in */
  clutter_timeline_set_delay (priv->timeline, 50);

  g_signal_connect (priv->timeline, "new-frame",
                    G_CALLBACK (mwb_ac_list_new_frame_cb), self);
  g_signal_connect (priv->timeline, "completed",
                    G_CALLBACK (mwb_ac_list_completed_cb), self);
  clutter_timeline_start (priv->timeline);
}

static void
mwb_ac_list_result_received_cb (MhsHistory *history,
                                guint32 search_id,
                                const gchar *value,
                                const gchar *comment,
                                MwbAcList *self)
{
  MwbAcListPrivate *priv = self->priv;

  if (search_id == priv->search_id
      && priv->entries->len < MWB_AC_LIST_MAX_ENTRIES)
    {
      MwbAcListEntry *entry;
      const gchar *result_text;
      MwbAcListCachedFavicon *cached_favicon;

      /* Remove the clear timeout */
      if (priv->clear_timeout)
        {
          mwb_ac_list_clear_entries (self);
          mwb_ac_list_add_default_entries (self);
          g_source_remove (priv->clear_timeout);
          priv->clear_timeout = 0;
        }

      g_array_set_size (priv->entries, priv->entries->len + 1);
      entry = &g_array_index (priv->entries, MwbAcListEntry,
                              priv->entries->len - 1);

      /* Prefer to display the comment if the search string matches in
         it */
      if (mwb_ac_list_stristr (comment, priv->search_text->str,
                               &entry->match_start, &entry->match_end))
        result_text = comment;
      else if (mwb_ac_list_stristr (value, priv->search_text->str,
                                    &entry->match_start, &entry->match_end))
        {
          result_text = value;

          /* Strip leading http:// and www. */
          if (strncmp ("http://", result_text, 7) == 0)
            {
              int offset = strncmp ("www.", result_text + 7, 4) ? 7 : 11;
              entry->match_start -= offset;
              entry->match_end -= offset;
              if (entry->match_start < 0)
                entry->match_start = 0;
              if (entry->match_end < 0)
                entry->match_end = 0;
              result_text += offset;
            }
        }
      else
        {
          /* If neither match just display the comment and trust that
             places had some reason to suggest it */
          result_text = comment;
          entry->match_start = entry->match_end = 0;
        }

      entry->label_text = g_strdup (result_text);
      entry->url = g_strdup (value);

      mwb_ac_list_update_entry (self, entry);

      /* Check if we've still got a cached version of the icon */
      if ((cached_favicon = g_hash_table_lookup (priv->favicon_cache, value)))
        {
          /* The favicon might not have finished being fetched
             yet. Once it is then the callback will set the icon on
             all matching entries */
          if (cached_favicon->texture)
            entry->texture = cogl_handle_ref (cached_favicon->texture);
        }
      else
        {
          /* Otherwise start fetching it */
          cached_favicon = g_slice_new (MwbAcListCachedFavicon);

          cached_favicon->texture = NULL;
          cached_favicon->ac_list = self;

          cached_favicon->favicon_handler
            = mhs_history_get_favicon (priv->history, value, FALSE,
                                       mwb_ac_list_get_favicon_cb,
                                       cached_favicon,
                                       NULL);

          cached_favicon->url = g_strdup (value);

          g_hash_table_insert (priv->favicon_cache,
                               cached_favicon->url,
                               cached_favicon);
        }

      mwb_ac_list_start_transition (self);

      clutter_actor_queue_relayout (CLUTTER_ACTOR (self));
    }
}

static void
mwb_ac_list_clear_entries (MwbAcList *self)
{
  MwbAcListPrivate *priv = self->priv;
  guint i;

  for (i = 0; i < priv->entries->len; i++)
    {
      MwbAcListEntry *entry
        = &g_array_index (priv->entries, MwbAcListEntry, i);
      if (entry->label_actor)
        clutter_actor_unparent (CLUTTER_ACTOR (entry->label_actor));
      if (entry->highlight_widget)
        {
          g_signal_handler_disconnect (entry->highlight_widget,
                                       entry->highlight_clicked_handler);
          g_signal_handler_disconnect (entry->highlight_widget,
                                       entry->highlight_motion_handler);
          clutter_actor_unparent (CLUTTER_ACTOR (entry->highlight_widget));
        }
      if (entry->label_text)
        g_free (entry->label_text);
      if (entry->url)
        g_free (entry->url);
      if (entry->texture != COGL_INVALID_HANDLE)
        cogl_handle_unref (entry->texture);
    }

  g_array_set_size (priv->entries, 0);

  if (priv->selection >= 0)
    {
      priv->selection = -1;
      g_object_notify (G_OBJECT (self), "selection");
    }

  /* Clear the tallest entry so that adding new entries will reset
     it */
  priv->tallest_entry = MWB_AC_LIST_ICON_SIZE;
}

static void
mwb_ac_list_add_default_entry (MwbAcList *self,
                               const gchar *label_text,
                               const gchar *search_text,
                               gchar *url,
                               CoglHandle icon)
{
  MwbAcListPrivate *priv = self->priv;
  const gchar *marker;
  MwbAcListEntry *entry;

  g_array_set_size (priv->entries, priv->entries->len + 1);
  entry = &g_array_index (priv->entries, MwbAcListEntry,
                          priv->entries->len - 1);

  /* This is done instead of hardcoding the match_end and match_start
     offsets in the hope that it would make translation easier */
  if ((marker = strstr (label_text, "%s")) == NULL)
    {
      entry->label_text = g_strdup (label_text);
      entry->match_start = entry->match_end = -1;
    }
  else
    {
      size_t search_len = strlen (search_text);
      size_t end_len = strlen (marker + 2);
      entry->label_text = g_malloc (marker - label_text
                                    + search_len + end_len + 1);
      memcpy (entry->label_text, label_text, marker - label_text);
      memcpy (entry->label_text + (marker - label_text),
              search_text, search_len);
      memcpy (entry->label_text + (marker - label_text) + search_len,
              marker + 2, end_len + 1);
      entry->match_start = marker - label_text;
      entry->match_end = entry->match_start + search_len;
    }

  entry->url = url;
  if (icon != COGL_INVALID_HANDLE)
    entry->texture = cogl_handle_ref (icon);

  mwb_ac_list_update_entry (self, entry);
}

static void
mwb_ac_list_add_default_entries (MwbAcList *self)
{
  MwbAcListPrivate *priv = self->priv;

  /* Add the two default entries */
  if (priv->search_in_automagic &&
      priv->search_engine_name &&
      priv->search_engine_url)
    {
      char *percent;
      gchar *label;
      GString *search_uri = g_string_new ("");

      /* If the search engine URL contains '%s' then replace that with
         the search text, otherwise just append it */
      if ((percent = strstr (priv->search_engine_url, "%s")))
        {
          g_string_append_len (search_uri, priv->search_engine_url,
                               percent - priv->search_engine_url);
          g_string_append_uri_escaped (search_uri, priv->search_text->str,
                                       NULL, FALSE);
          g_string_append (search_uri, percent + 2);
        }
      else
        {
          g_string_append (search_uri, priv->search_engine_url);
          g_string_append_uri_escaped (search_uri, priv->search_text->str,
                                       NULL, FALSE);
        }

      label = g_strdup_printf (_("Search for %%s on %s"),
                               priv->search_engine_name);

      mwb_ac_list_add_default_entry (self,
                                     label,
                                     priv->search_text->str,
                                     search_uri->str,
                                     priv->search_engine_icon);
      /* Above call takes ownership of the URI string */
      g_string_free (search_uri, FALSE);

      g_free (label);
    }
  if (priv->complete_domains)
    mwb_ac_list_add_default_entry (self,
                                   _("Go to %s.com"),
                                   priv->search_text->str,
                                   g_strconcat ("http://",
                                                priv->search_text->str,
                                                ".com/",
                                                NULL),
                                   COGL_INVALID_HANDLE);
}

static gboolean
mwb_ac_list_clear_timeout_cb (MwbAcList *self)
{
  MwbAcListPrivate *priv = self->priv;

  mwb_ac_list_clear_entries (self);
  mwb_ac_list_add_default_entries (self);

  mwb_ac_list_start_transition (self);

  priv->clear_timeout = 0;

  return FALSE;
}

void
mwb_ac_list_set_search_text (MwbAcList *self,
                             const gchar *search_text)
{
  MwbAcListPrivate *priv = self->priv;

  g_return_if_fail (MWB_IS_AC_LIST (self));

  if (strcmp (priv->search_text->str, search_text) != 0)
    {
      int search_text_len = strlen (search_text);

      if (search_text_len < priv->old_search_length)
        g_hash_table_remove_all (priv->favicon_cache);
      priv->old_search_length = search_text_len;

      g_string_set_size (priv->search_text, 0);
      g_string_append_len (priv->search_text, search_text, search_text_len);

      /* Store the current height for animation */
      priv->last_height = mwb_ac_list_get_height (self);

      if (priv->history)
        {
          GError *error = NULL;

          if (!mhs_history_start_ac_search (priv->history, search_text,
                                            &priv->search_id,
                                            &error))
            {
              g_warning ("Search failed: %s", error->message);
              g_error_free (error);
              priv->search_id = 0;
            }
        }

      /* Only clear after a short timeout, stops us from spawning
       * lots of queries if this gets set frequently, and stops
       * the animation from looking shaky
       */
      if (priv->clear_timeout)
        g_source_remove (priv->clear_timeout);
      priv->clear_timeout = g_timeout_add (100, (GSourceFunc)
                                           mwb_ac_list_clear_timeout_cb,
                                           self);

      g_object_notify (G_OBJECT (self), "search-text");
    }
}

const gchar *
mwb_ac_list_get_search_text (MwbAcList *self)
{
  MwbAcListPrivate *priv = self->priv;

  g_return_val_if_fail (MWB_IS_AC_LIST (self), NULL);

  return priv->search_text->str;
}

void
mwb_ac_list_set_selection (MwbAcList *self, gint selection)
{
  MwbAcListPrivate *priv = self->priv;

  g_return_if_fail (MWB_IS_AC_LIST (self));
  g_return_if_fail (selection == -1
                    || (selection >= 0 && selection < priv->entries->len));

  if (priv->selection != selection)
    {
      priv->selection = selection;

      g_object_notify (G_OBJECT (self), "selection");

      if (CLUTTER_ACTOR_IS_MAPPED (CLUTTER_ACTOR (self)))
        clutter_actor_queue_redraw (CLUTTER_ACTOR (self));
    }
}

gint
mwb_ac_list_get_selection (MwbAcList *self)
{
  g_return_val_if_fail (MWB_IS_AC_LIST (self), -1);

  return self->priv->selection;
}

guint
mwb_ac_list_get_n_entries (MwbAcList *self)
{
  g_return_val_if_fail (MWB_IS_AC_LIST (self), 0);

  return self->priv->entries->len;
}

gchar *
mwb_ac_list_get_entry_url (MwbAcList *self, guint entry)
{
  MwbAcListPrivate *priv;

  g_return_val_if_fail (MWB_IS_AC_LIST (self), NULL);

  priv = self->priv;

  g_return_val_if_fail (entry < priv->entries->len, NULL);

  return g_strdup (g_array_index (priv->entries, MwbAcListEntry, entry).url);
}