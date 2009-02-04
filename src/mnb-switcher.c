/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Moblin Netbook
 * Copyright © 2009, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU Lesser General Public License,
 * version 2.1, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#include "mnb-switcher.h"
#include "moblin-netbook-ui.h"
#include "moblin-netbook.h"
#include <nbtk/nbtk-tooltip.h>

#define HOVER_TIMEOUT  800

/*
 * MnbSwitcherApp
 *
 * A NbtkWidget subclass represening a single thumb in the switcher.
 */
#define MNB_TYPE_SWITCHER_APP                 (mnb_switcher_app_get_type ())
#define MNB_SWITCHER_APP(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), MNB_TYPE_SWITCHER_APP, MnbSwitcherApp))
#define MNB_IS_SWITCHER_APP(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MNB_TYPE_SWITCHER_APP))
#define MNB_SWITCHER_APP_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), MNB_TYPE_SWITCHER_APP, MnbSwitcherAppClass))
#define MNB_IS_SWITCHER_APP_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), MNB_TYPE_SWITCHER_APP))
#define MNB_SWITCHER_APP_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), MNB_TYPE_SWITCHER_APP, MnbSwitcherAppClass))

typedef struct _MnbSwitcherApp               MnbSwitcherApp;
typedef struct _MnbSwitcherAppPrivate        MnbSwitcherAppPrivate;
typedef struct _MnbSwitcherAppClass          MnbSwitcherAppClass;

struct _MnbSwitcherApp
{
  /*< private >*/
  NbtkWidget parent_instance;

  MnbSwitcherAppPrivate *priv;
};

struct _MnbSwitcherAppClass
{
  /*< private >*/
  NbtkWidgetClass parent_class;
};

struct _MnbSwitcherAppPrivate
{
  MnbSwitcher  *switcher;
  MutterWindow *mw;
  guint         hover_timeout_id;
  ClutterActor *tooltip;
  guint         focus_id;
  guint         raised_id;
};

GType mnb_switcher_app_get_type (void);

G_DEFINE_TYPE (MnbSwitcherApp, mnb_switcher_app, NBTK_TYPE_WIDGET)

#define MNB_SWITCHER_APP_GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), MNB_TYPE_SWITCHER_APP,\
                                MnbSwitcherAppPrivate))

static void
mnb_switcher_app_class_init (MnbSwitcherAppClass *klass)
{
  g_type_class_add_private (klass, sizeof (MnbSwitcherAppPrivate));
}

static void
mnb_switcher_app_init (MnbSwitcherApp *self)
{
  self->priv = MNB_SWITCHER_APP_GET_PRIVATE (self);
}

static void
mnb_switcher_app_dispose (GObject *object)
{
  MnbSwitcherAppPrivate *priv = MNB_SWITCHER_APP (object)->priv;
  MetaWindow            *meta_win;

  meta_win = mutter_window_get_meta_window (priv->mw);

  if (priv->hover_timeout_id)
    {
      g_source_remove (priv->hover_timeout_id);
      priv->hover_timeout_id = 0;
    }

  if (priv->focus_id)
    {
      g_signal_handler_disconnect (meta_win, priv->focus_id);
      priv->focus_id = 0;
    }

  if (priv->raised_id)
    {
      g_signal_handler_disconnect (meta_win, priv->raised_id);
      priv->raised_id = 0;
    }

  /*
   * Do not destroy the tooltip, this is happens automatically.
   */

  G_OBJECT_CLASS (mnb_switcher_app_parent_class)->dispose (object);
}

G_DEFINE_TYPE (MnbSwitcher, mnb_switcher, MNB_TYPE_DROP_DOWN)

#define MNB_SWITCHER_GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), MNB_TYPE_SWITCHER, MnbSwitcherPrivate))

struct _MnbSwitcherPrivate {
  MutterPlugin *plugin;
  NbtkWidget   *table;
  NbtkWidget   *new_workspace;
  NbtkWidget   *new_label;
  GList        *last_workspaces;

  ClutterActor *last_focused;
  MutterWindow *selected;
  GList        *tab_list;
  gboolean      dnd_in_progress : 1;
};

struct input_data
{
  gint          index;
  MutterPlugin *plugin;
};

/*
 * Calback for clicks on a workspace in the switcher (switches to the
 * appropriate ws).
 */
static gboolean
workspace_input_cb (ClutterActor *clone, ClutterEvent *event, gpointer data)
{
  struct input_data *input_data = data;
  gint               indx       = input_data->index;
  MutterPlugin      *plugin     = input_data->plugin;
  MetaScreen        *screen     = mutter_plugin_get_screen (plugin);
  MetaWorkspace     *workspace;
  MoblinNetbookPluginPrivate *priv = MOBLIN_NETBOOK_PLUGIN (plugin)->priv;

  if (MNB_SWITCHER (priv->switcher)->priv->dnd_in_progress)
    return FALSE;

  workspace = meta_screen_get_workspace_by_index (screen, indx);

  if (!workspace)
    {
      g_warning ("No workspace specified, %s:%d\n", __FILE__, __LINE__);
      return FALSE;
    }

  clutter_actor_hide (priv->switcher);
  hide_panel (plugin);

  if (priv->in_alt_grab)
    {
      MetaDisplay *display = meta_screen_get_display (screen);

      meta_display_end_grab_op (display, event->key.time);
      priv->in_alt_grab = FALSE;
    }

  meta_workspace_activate (workspace, event->any.time);

  return FALSE;
}

static gboolean
workspace_switcher_clone_input_cb (ClutterActor *clone,
                                   ClutterEvent *event,
                                   gpointer      data)
{
  MnbSwitcherAppPrivate      *app_priv = MNB_SWITCHER_APP (clone)->priv;
  MutterWindow               *mw = app_priv->mw;
  MetaWindow                 *window;
  MetaWorkspace              *workspace;
  MetaWorkspace              *active_workspace;
  MetaScreen                 *screen;
  MnbSwitcher                *switcher = app_priv->switcher;
  MutterPlugin               *plugin = switcher->priv->plugin;
  MoblinNetbookPluginPrivate *priv = MOBLIN_NETBOOK_PLUGIN (plugin)->priv;

  if (MNB_SWITCHER (priv->switcher)->priv->dnd_in_progress)
    return FALSE;

  window           = mutter_window_get_meta_window (mw);
  screen           = meta_window_get_screen (window);
  workspace        = meta_window_get_workspace (window);
  active_workspace = meta_screen_get_active_workspace (screen);

  clutter_actor_hide (CLUTTER_ACTOR (switcher));
  hide_panel (plugin);

  if (!active_workspace || (active_workspace == workspace))
    {
      meta_window_activate_with_workspace (window, event->any.time, workspace);
    }
  else
    {
      meta_workspace_activate_with_focus (workspace, window, event->any.time);
    }

  return FALSE;
}

static void
dnd_begin_cb (NbtkWidget   *table,
	      ClutterActor *dragged,
	      ClutterActor *icon,
	      gint          x,
	      gint          y,
	      gpointer      data)
{
  MnbSwitcherPrivate    *priv         = MNB_SWITCHER (data)->priv;
  MnbSwitcherAppPrivate *dragged_priv = MNB_SWITCHER_APP (dragged)->priv;

  priv->dnd_in_progress = TRUE;

  if (dragged_priv->hover_timeout_id)
    {
      g_source_remove (dragged_priv->hover_timeout_id);
      dragged_priv->hover_timeout_id = 0;
    }

  if (CLUTTER_ACTOR_IS_VISIBLE (dragged_priv->tooltip))
    nbtk_tooltip_hide (NBTK_TOOLTIP (dragged_priv->tooltip));

  clutter_actor_set_rotation (icon, CLUTTER_Y_AXIS, 60.0, 0, 0, 0);
  clutter_actor_set_opacity (dragged, 0x4f);
}

static void
dnd_end_cb (NbtkWidget   *table,
	    ClutterActor *dragged,
	    ClutterActor *icon,
	    gint          x,
	    gint          y,
	    gpointer      data)
{
  MnbSwitcherPrivate *priv = MNB_SWITCHER (data)->priv;

  priv->dnd_in_progress = FALSE;

  clutter_actor_set_rotation (icon, CLUTTER_Y_AXIS, 0.0, 0, 0, 0);
  clutter_actor_set_opacity (dragged, 0xff);
}

static gint
tablist_sort_func (gconstpointer a, gconstpointer b)
{
  ClutterActor          *clone1 = CLUTTER_ACTOR (a);
  ClutterActor          *clone2 = CLUTTER_ACTOR (b);
  ClutterActor          *parent1 = clutter_actor_get_parent (clone1);
  ClutterActor          *parent2 = clutter_actor_get_parent (clone2);
  ClutterActor          *gparent1 = clutter_actor_get_parent (parent1);
  ClutterActor          *gparent2 = clutter_actor_get_parent (parent2);
  gint                   pcol1, pcol2;

  if (parent1 == parent2)
    {
      /*
       * The simple case of both clones on the same workspace.
       */
      gint row1, row2, col1, col2;

      clutter_container_child_get (CLUTTER_CONTAINER (parent1), clone1,
                                   "row", &row1, "column", &col1, NULL);
      clutter_container_child_get (CLUTTER_CONTAINER (parent1), clone2,
                                   "row", &row2, "column", &col2, NULL);

      if (row1 < row2)
        return -1;

      if (row1 > row2)
        return 1;

      if (col1 < col2)
        return -1;

      if (col1 > col2)
        return 1;

      return 0;
    }

  clutter_container_child_get (CLUTTER_CONTAINER (gparent1), parent1,
                               "column", &pcol1, NULL);
  clutter_container_child_get (CLUTTER_CONTAINER (gparent2), parent2,
                               "column", &pcol2, NULL);

  if (pcol1 < pcol2)
    return -1;

  if (pcol1 > pcol2)
    return 1;

  return 0;
}

static void
dnd_dropped_cb (NbtkWidget   *table,
		ClutterActor *dragged,
		ClutterActor *icon,
		gint          x,
		gint          y,
		gpointer      data)
{
  MnbSwitcher           *switcher = MNB_SWITCHER (data);
  MnbSwitcherPrivate    *priv = switcher->priv;
  MnbSwitcherAppPrivate *dragged_priv = MNB_SWITCHER_APP (dragged)->priv;
  ClutterChildMeta      *meta;
  ClutterActor          *parent;
  ClutterActor          *table_actor = CLUTTER_ACTOR (table);
  MetaWindow            *meta_win;
  gint                   col;

  if (!(meta_win = mutter_window_get_meta_window (dragged_priv->mw)))
    {
      g_warning ("No MutterWindow associated with this item.");
      return;
    }

  parent = clutter_actor_get_parent (table_actor);

  g_assert (NBTK_IS_TABLE (parent));

  meta = clutter_container_get_child_meta (CLUTTER_CONTAINER (parent),
					   table_actor);

  g_object_get (meta, "column", &col, NULL);

  if (priv->tab_list)
    {
      priv->tab_list = g_list_sort (priv->tab_list, tablist_sort_func);
    }

  /*
   * TODO -- perhaps we should expose the timestamp from the pointer event,
   * or event the entire Clutter event.
   */
  meta_window_change_workspace_by_index (meta_win, col, TRUE, CurrentTime);
}

static NbtkTable *
mnb_switcher_append_workspace (MnbSwitcher *switcher);

static void
dnd_new_dropped_cb (NbtkWidget   *table,
                    ClutterActor *dragged,
                    ClutterActor *icon,
                    gint          x,
                    gint          y,
                    gpointer      data)
{
  MnbSwitcher           *switcher = MNB_SWITCHER (data);
  MnbSwitcherPrivate    *priv = switcher->priv;
  MnbSwitcherAppPrivate *dragged_priv = MNB_SWITCHER_APP (dragged)->priv;
  ClutterChildMeta      *meta, *d_meta;
  ClutterActor          *parent;
  ClutterActor          *table_actor = CLUTTER_ACTOR (table);
  MetaWindow            *meta_win;
  gint                   col;
  NbtkTable             *new_ws;
  gboolean               keep_ratio = FALSE;

  if (!(meta_win = mutter_window_get_meta_window (dragged_priv->mw)))
    {
      g_warning ("No MutterWindow associated with this item.");
      return;
    }

  parent = clutter_actor_get_parent (table_actor);

  g_assert (NBTK_IS_TABLE (parent));

  meta = clutter_container_get_child_meta (CLUTTER_CONTAINER (parent),
					   table_actor);
  d_meta = clutter_container_get_child_meta (CLUTTER_CONTAINER (table),
                                             dragged);

  g_object_get (meta, "column", &col, NULL);
  g_object_get (d_meta, "keep-aspect-ratio", &keep_ratio, NULL);

  new_ws = mnb_switcher_append_workspace (switcher);

  g_object_ref (dragged);
  clutter_container_remove_actor (CLUTTER_CONTAINER (table), dragged);
  nbtk_table_add_actor (new_ws, dragged, 1, 0);

  clutter_container_child_set (CLUTTER_CONTAINER (new_ws), dragged,
			       "keep-aspect-ratio", keep_ratio, NULL);

  g_object_unref (dragged);

  if (priv->tab_list)
    {
      priv->tab_list = g_list_sort (priv->tab_list, tablist_sort_func);
    }

  /*
   * TODO -- perhaps we should expose the timestamp from the pointer event,
   * or event the entire Clutter event.
   */
  meta_window_change_workspace_by_index (meta_win, col, TRUE, CurrentTime);
}

static gboolean
clone_hover_timeout_cb (gpointer data)
{
  MnbSwitcherAppPrivate *app_priv = MNB_SWITCHER_APP (data)->priv;
  MnbSwitcherPrivate    *priv     = app_priv->switcher->priv;

  if (!priv->dnd_in_progress)
    nbtk_tooltip_show (NBTK_TOOLTIP (app_priv->tooltip));

  app_priv->hover_timeout_id = 0;

  return FALSE;
}

static gboolean
clone_enter_event_cb (ClutterActor *actor,
		      ClutterCrossingEvent *event,
		      gpointer data)
{
  MnbSwitcherAppPrivate *child_priv = MNB_SWITCHER_APP (actor)->priv;
  MnbSwitcherPrivate    *priv       = child_priv->switcher->priv;

  if (!priv->dnd_in_progress)
    child_priv->hover_timeout_id = g_timeout_add (HOVER_TIMEOUT,
						  clone_hover_timeout_cb,
						  actor);

  return FALSE;
}

static gboolean
clone_leave_event_cb (ClutterActor *actor,
		      ClutterCrossingEvent *event,
		      gpointer data)
{
  MnbSwitcherAppPrivate *child_priv = MNB_SWITCHER_APP (actor)->priv;

  if (child_priv->hover_timeout_id)
    {
      g_source_remove (child_priv->hover_timeout_id);
      child_priv->hover_timeout_id = 0;
    }

  if (CLUTTER_ACTOR_IS_VISIBLE (child_priv->tooltip))
    nbtk_tooltip_hide (NBTK_TOOLTIP (child_priv->tooltip));

  return FALSE;
}

static NbtkWidget *
make_workspace_content (MnbSwitcher *switcher, gboolean active, gint col)
{
  MnbSwitcherPrivate *priv = switcher->priv;
  NbtkWidget         *table = priv->table;
  NbtkWidget         *new_ws;
  struct input_data  *input_data = g_new (struct input_data, 1);
  NbtkPadding         padding = MNB_PADDING (6, 6, 6, 6);

  input_data = g_new (struct input_data, 1);
  input_data->index = col;
  input_data->plugin = priv->plugin;

  new_ws = nbtk_table_new ();
  nbtk_table_set_row_spacing (NBTK_TABLE (new_ws), 6);
  nbtk_table_set_col_spacing (NBTK_TABLE (new_ws), 6);
  nbtk_widget_set_padding (new_ws, &padding);
  nbtk_widget_set_style_class_name (NBTK_WIDGET (new_ws),
                                    "switcher-workspace");

  if (active)
    clutter_actor_set_name (CLUTTER_ACTOR (new_ws),
                            "switcher-workspace-active");

  nbtk_widget_set_dnd_threshold (new_ws, 5);

  g_signal_connect (new_ws, "dnd-begin",
                    G_CALLBACK (dnd_begin_cb), switcher);

  g_signal_connect (new_ws, "dnd-end",
                    G_CALLBACK (dnd_end_cb), switcher);

  g_signal_connect (new_ws, "dnd-dropped",
                    G_CALLBACK (dnd_dropped_cb), switcher);

  nbtk_table_add_widget (NBTK_TABLE (table), new_ws, 1, col);

  /* switch workspace when the workspace is selected */
  g_signal_connect_data (new_ws, "button-press-event",
                         G_CALLBACK (workspace_input_cb), input_data,
                         (GClosureNotify)g_free, 0);

  return new_ws;
}

static NbtkWidget *
make_workspace_label (MnbSwitcher *switcher, gboolean active, gint col)
{
  MnbSwitcherPrivate *priv = switcher->priv;
  NbtkWidget         *table = priv->table;
  NbtkWidget         *ws_label;
  gchar              *s;
  struct input_data  *input_data = g_new (struct input_data, 1);

  input_data->index = col;
  input_data->plugin = priv->plugin;

  s = g_strdup_printf ("%d", col + 1);

  ws_label = nbtk_label_new (s);

  if (active)
    clutter_actor_set_name (CLUTTER_ACTOR (ws_label), "workspace-title-active");

  nbtk_widget_set_style_class_name (ws_label, "workspace-title");

  clutter_actor_set_reactive (CLUTTER_ACTOR (ws_label), TRUE);

  g_signal_connect_data (ws_label, "button-press-event",
                         G_CALLBACK (workspace_input_cb), input_data,
                         (GClosureNotify) g_free, 0);

  nbtk_table_add_widget (NBTK_TABLE (table), ws_label, 0, col);
  clutter_container_child_set (CLUTTER_CONTAINER (table),
                               CLUTTER_ACTOR (ws_label),
                               "y-expand", FALSE, NULL);

  return ws_label;
}

struct ws_remove_data
{
  MnbSwitcher *switcher;
  gint         col;
  GList       *remove;
};

static void
table_foreach_remove_ws (ClutterActor *child, gpointer data)
{
  struct ws_remove_data *remove_data = data;
  MnbSwitcher           *switcher    = remove_data->switcher;
  NbtkWidget            *table       = switcher->priv->table;
  ClutterChildMeta      *meta;
  gint                   row, col;

  meta = clutter_container_get_child_meta (CLUTTER_CONTAINER (table), child);

  g_assert (meta);
  g_object_get (meta, "row", &row, "column", &col, NULL);

  /*
   * Children below the column we are removing are unaffected.
   */
  if (col < remove_data->col)
    return;

  /*
   * We cannot remove the actors in the foreach function, as that potentially
   * affects a list in which the container holds the data (e.g., NbtkTable).
   * We schedule it for removal, and then remove all once we are finished with
   * the foreach.
   */
  if (col == remove_data->col)
    {
      remove_data->remove = g_list_prepend (remove_data->remove, child);
    }
  else
    {
      /*
       * For some reason changing the colum clears the y-expand property :-(
       * Need to preserve it on the first row.
       */
      if (!row)
        clutter_container_child_set (CLUTTER_CONTAINER (table), child,
                                     "column", col - 1,
                                     "y-expand", FALSE, NULL);
      else
        clutter_container_child_set (CLUTTER_CONTAINER (table), child,
                                     "column", col - 1, NULL);
    }
}

static void
screen_n_workspaces_notify (MetaScreen *screen,
                            GParamSpec *pspec,
                            gpointer    data)
{
  MnbSwitcher *switcher = MNB_SWITCHER (data);
  gint   n_c_workspaces = meta_screen_get_n_workspaces (screen);
  GList *c_workspaces = meta_screen_get_workspaces (screen);
  GList *o_workspaces = switcher->priv->last_workspaces;
  gint   n_o_workspaces = g_list_length (o_workspaces);
  gboolean *map;
  gint i;
  GList *k;
  struct ws_remove_data remove_data;

  if (n_o_workspaces < n_c_workspaces)
    {
      g_warning ("Adding workspaces into running switcher is currently not "
                 "supported.");

      g_list_free (switcher->priv->last_workspaces);
      switcher->priv->last_workspaces = g_list_copy (c_workspaces);
      return;
    }

  remove_data.switcher = switcher;
  remove_data.remove = NULL;

  map = g_slice_alloc0 (sizeof (gboolean) * n_o_workspaces);

  k = c_workspaces;

  while (k)
    {
      MetaWorkspace *w = k->data;
      GList         *l = o_workspaces;

      i = 0;

      while (l)
        {
          MetaWorkspace *w2 = l->data;

          if (w == w2)
            {
              map[i] = TRUE;
              break;
            }

          ++i;
          l = l->next;
        }

      k = k->next;
    }

  for (i = 0; i < n_o_workspaces; ++i)
    {
      if (!map[i])
        {
          GList *l;
          ClutterContainer *table = CLUTTER_CONTAINER (switcher->priv->table);

          remove_data.col = i;
          clutter_container_foreach (table,
                                     (ClutterCallback) table_foreach_remove_ws,
                                     &remove_data);

          l = remove_data.remove;
          while (l)
            {
              ClutterActor *child = l->data;

              clutter_container_remove_actor (table, child);

              l = l->next;
            }

          g_list_free (remove_data.remove);
        }
    }

  g_list_free (switcher->priv->last_workspaces);
  switcher->priv->last_workspaces = g_list_copy (c_workspaces);
}

static void
dnd_new_enter_cb (NbtkWidget   *table,
                  ClutterActor *dragged,
                  ClutterActor *icon,
                  gint          x,
                  gint          y,
                  gpointer      data)
{
  MnbSwitcherPrivate *priv = MNB_SWITCHER (data)->priv;

  clutter_actor_set_name (CLUTTER_ACTOR (priv->new_workspace),
                          "switcher-workspace-new-active");
  clutter_actor_set_name (CLUTTER_ACTOR (priv->new_label),
                          "switcher-workspace-new-active");
}

static void
dnd_new_leave_cb (NbtkWidget   *table,
                  ClutterActor *dragged,
                  ClutterActor *icon,
                  gint          x,
                  gint          y,
                  gpointer      data)
{
  MnbSwitcherPrivate *priv = MNB_SWITCHER (data)->priv;

  clutter_actor_set_name (CLUTTER_ACTOR (priv->new_workspace), "");
  clutter_actor_set_name (CLUTTER_ACTOR (priv->new_label), "");
}

static void
meta_window_focus_cb (MetaWindow *mw, gpointer data)
{
  MnbSwitcherAppPrivate *child_priv = MNB_SWITCHER_APP (data)->priv;
  MnbSwitcher           *switcher = child_priv->switcher;
  MnbSwitcherPrivate    *priv = switcher->priv;

  if (priv->last_focused == data)
    return;

  if (priv->last_focused)
    clutter_actor_set_name (priv->last_focused, "");

  clutter_actor_set_name (CLUTTER_ACTOR (data), "switcher-application-active");
  priv->last_focused = data;
  priv->selected = child_priv->mw;
}

static void mnb_switcher_clone_weak_notify (gpointer data, GObject *object);

struct origin_data
{
  ClutterActor *clone;
  MutterWindow *mw;
  MnbSwitcher  *switcher;
};

static void
mnb_switcher_origin_weak_notify (gpointer data, GObject *obj)
{
  struct origin_data *origin_data = data;
  ClutterActor       *clone = origin_data->clone;
  MnbSwitcherPrivate *priv  = origin_data->switcher->priv;

  if (priv->tab_list)
    {
      priv->tab_list = g_list_remove (priv->tab_list, clone);
    }

  /*
   * The original MutterWindow destroyed; remove the weak reference the
   * we added to the clone referencing the original window, then
   * destroy the clone.
   */
  g_object_weak_unref (G_OBJECT (clone), mnb_switcher_clone_weak_notify, data);
  clutter_actor_destroy (clone);

  g_free (data);
}

static void
mnb_switcher_clone_weak_notify (gpointer data, GObject *obj)
{
  struct origin_data *origin_data = data;
  GObject            *origin = G_OBJECT (origin_data->mw);

  /*
   * Clone destroyed -- this function gets only called whent the clone
   * is destroyed while the original MutterWindow still exists, so remove
   * the weak reference we added on the origin for sake of the clone.
   */
  g_object_weak_unref (origin, mnb_switcher_origin_weak_notify, data);
}

static void
mnb_switcher_show (ClutterActor *self)
{
  MnbSwitcherPrivate *priv = MNB_SWITCHER (self)->priv;
  MetaScreen   *screen = mutter_plugin_get_screen (priv->plugin);
  gint          ws_count, active_ws, ws_max_windows = 0;
  gint         *n_windows;
  gint          i, screen_width;
  NbtkWidget   *table;
  GList        *window_list, *l;
  NbtkWidget  **spaces;
  NbtkPadding   padding = MNB_PADDING (6, 6, 6, 6);
  GList        *workspaces = meta_screen_get_workspaces (screen);

  priv->last_workspaces = g_list_copy (workspaces);

  if (priv->tab_list)
    {
      g_list_free (priv->tab_list);
      priv->tab_list = NULL;
    }

  /* create the contents */

  table = nbtk_table_new ();
  priv->table = table;
  nbtk_table_set_row_spacing (NBTK_TABLE (table), 4);
  nbtk_table_set_col_spacing (NBTK_TABLE (table), 7);
  nbtk_widget_set_padding (table, &padding);

  clutter_actor_set_name (CLUTTER_ACTOR (table), "switcher-table");

  ws_count = meta_screen_get_n_workspaces (screen);
  active_ws = meta_screen_get_active_workspace_index (screen);

  mutter_plugin_query_screen_size (priv->plugin, &screen_width, &i);

  /* loop through all the workspaces, adding a label for each */
  for (i = 0; i < ws_count; i++)
    {
      gboolean active = FALSE;

      if (i == active_ws)
        active = TRUE;

      make_workspace_label (MNB_SWITCHER (self), active, i);
    }

  /* iterate through the windows, adding them to the correct workspace */

  n_windows = g_new0 (gint, ws_count);
  spaces = g_new0 (NbtkWidget*, ws_count);
  window_list = mutter_plugin_get_windows (priv->plugin);
  for (l = window_list; l; l = g_list_next (l))
    {
      MutterWindow          *mw = l->data;
      ClutterActor          *texture, *c_tx, *clone;
      gint                   ws_indx;
      MetaCompWindowType     type;
      gint                   w, h;
      struct origin_data    *origin_data;
      MetaWindow            *meta_win = mutter_window_get_meta_window (mw);
      gchar                 *title;
      MnbSwitcherAppPrivate *app_priv;

      ws_indx = mutter_window_get_workspace (mw);
      type = mutter_window_get_window_type (mw);
      /*
       * Only show regular windows that are not sticky (getting stacking order
       * right for sticky windows would be really hard, and since they appear
       * on each workspace, they do not help in identifying which workspace
       * it is).
       */
      if (ws_indx < 0                             ||
          mutter_window_is_override_redirect (mw) ||
          type != META_COMP_WINDOW_NORMAL)
        {
          continue;
        }

      /* create the table for this workspace if we don't already have one */
      if (!spaces[ws_indx])
        {
          gboolean active = FALSE;

          if (ws_indx == active_ws)
            active = TRUE;

          spaces[ws_indx] =
            make_workspace_content (MNB_SWITCHER (self), active, ws_indx);
        }

      texture = mutter_window_get_texture (mw);
      c_tx    = clutter_clone_texture_new (CLUTTER_TEXTURE (texture));
      clone   = g_object_new (MNB_TYPE_SWITCHER_APP, NULL);
      nbtk_widget_set_style_class_name (NBTK_WIDGET (clone),
                                        "switcher-application");

      if (meta_window_has_focus (meta_win))
          clutter_actor_set_name (clone, "switcher-application-active");

      priv->last_focused = clone;
      priv->selected = mw;

      clutter_container_add_actor (CLUTTER_CONTAINER (clone), c_tx);

      clutter_actor_set_reactive (clone, TRUE);

      origin_data = g_new0 (struct origin_data, 1);
      origin_data->clone = clone;
      origin_data->mw = mw;
      origin_data->switcher = MNB_SWITCHER (self);
      priv->tab_list = g_list_prepend (priv->tab_list, clone);

      g_object_weak_ref (G_OBJECT (mw),
                         mnb_switcher_origin_weak_notify, origin_data);
      g_object_weak_ref (G_OBJECT (clone),
                         mnb_switcher_clone_weak_notify, origin_data);

      g_object_get (meta_win, "title", &title, NULL);

      app_priv = MNB_SWITCHER_APP (clone)->priv;
      app_priv->switcher = MNB_SWITCHER (self);
      app_priv->mw       = mw;
      app_priv->tooltip  = g_object_new (NBTK_TYPE_TOOLTIP,
                                         "widget", clone,
                                         "label", title,
                                         NULL);
      g_free (title);

      g_signal_connect (clone, "enter-event",
                        G_CALLBACK (clone_enter_event_cb), NULL);
      g_signal_connect (clone, "leave-event",
                        G_CALLBACK (clone_leave_event_cb), NULL);

      app_priv->focus_id =
        g_signal_connect (meta_win, "focus",
                          G_CALLBACK (meta_window_focus_cb), clone);
      app_priv->raised_id =
        g_signal_connect (meta_win, "raised",
                          G_CALLBACK (meta_window_focus_cb), clone);

      n_windows[ws_indx]++;
      nbtk_table_add_actor (NBTK_TABLE (spaces[ws_indx]), clone,
                            n_windows[ws_indx], 0);
      clutter_container_child_set (CLUTTER_CONTAINER (spaces[ws_indx]), clone,
                                   "keep-aspect-ratio", TRUE, NULL);

      clutter_actor_get_size (clone, &h, &w);
      clutter_actor_set_size (clone, h/(gdouble)w * 80.0, 80);

      g_signal_connect (clone, "button-release-event",
                        G_CALLBACK (workspace_switcher_clone_input_cb),
			NULL);

      ws_max_windows = MAX (ws_max_windows, n_windows[ws_indx]);
    }

  /* add an "empty" message for empty workspaces */
  for (i = 0; i < ws_count; i++)
    {
      if (!spaces[i])
        {
          NbtkWidget *label;

          label = nbtk_label_new ("No applications on this space");

          nbtk_table_add_widget (NBTK_TABLE (table), label, 1, i);
        }
    }

  /*
   * Now create the new workspace column.
   */
  {
    NbtkWidget *new_ws = nbtk_table_new ();
    NbtkWidget *label;
    ClutterChildMeta *child;

    label = nbtk_label_new ("");
    nbtk_table_add_widget (NBTK_TABLE (table), label, 0, ws_count);
    nbtk_widget_set_style_class_name (label, "workspace-title-new");
    clutter_container_child_set (CLUTTER_CONTAINER (table),
                                 CLUTTER_ACTOR (label),
                                 "y-expand", FALSE, NULL);

    nbtk_table_set_row_spacing (NBTK_TABLE (new_ws), 6);
    nbtk_table_set_col_spacing (NBTK_TABLE (new_ws), 6);
    nbtk_widget_set_padding (new_ws, &padding);
    nbtk_widget_set_style_class_name (NBTK_WIDGET (new_ws),
                                      "switcher-workspace-new");

    nbtk_widget_set_dnd_threshold (new_ws, 5);

    g_signal_connect (new_ws, "dnd-begin",
                      G_CALLBACK (dnd_begin_cb), self);

    g_signal_connect (new_ws, "dnd-end",
                      G_CALLBACK (dnd_end_cb), self);

    g_signal_connect (new_ws, "dnd-dropped",
                      G_CALLBACK (dnd_new_dropped_cb), self);

    g_signal_connect (new_ws, "dnd-enter",
                      G_CALLBACK (dnd_new_enter_cb), self);

    g_signal_connect (new_ws, "dnd-leave",
                      G_CALLBACK (dnd_new_leave_cb), self);

    priv->new_workspace = new_ws;
    priv->new_label = label;

    nbtk_table_add_widget (NBTK_TABLE (table), new_ws, 1, ws_count);
  }

  g_free (spaces);
  g_free (n_windows);

  priv->tab_list = g_list_sort (priv->tab_list, tablist_sort_func);

  mnb_drop_down_set_child (MNB_DROP_DOWN (self),
                           CLUTTER_ACTOR (table));

  g_signal_connect (screen, "notify::n-workspaces",
                    G_CALLBACK (screen_n_workspaces_notify), self);

  CLUTTER_ACTOR_CLASS (mnb_switcher_parent_class)->show (self);
}

static NbtkTable *
mnb_switcher_append_workspace (MnbSwitcher *switcher)
{
  MnbSwitcherPrivate *priv = switcher->priv;
  NbtkWidget         *table = priv->table;
  NbtkWidget         *last_ws = priv->new_workspace;
  NbtkWidget         *last_label = priv->new_label;
  NbtkTable          *new_ws;
  gint                row, col;
  ClutterChildMeta   *meta;

  meta = clutter_container_get_child_meta (CLUTTER_CONTAINER (table),
                                           CLUTTER_ACTOR (last_ws));

  g_object_get (meta, "column", &col, NULL);

  clutter_container_child_set (CLUTTER_CONTAINER (table),
                               CLUTTER_ACTOR (last_ws),
                               "column", col + 1, NULL);

  clutter_container_child_set (CLUTTER_CONTAINER (table),
                               CLUTTER_ACTOR (last_label),
                               "column", col + 1,
                               "y-expand", FALSE, NULL);

  /*
   * Insert new workspace label and content pane where the new workspace
   * area was.
   */
  make_workspace_label   (switcher, FALSE, col);
  new_ws = NBTK_TABLE (make_workspace_content (switcher, FALSE, col));

  return new_ws;
}

static void
mnb_switcher_hide (ClutterActor *self)
{
  MnbSwitcherPrivate *priv;

  g_return_if_fail (MNB_IS_SWITCHER (self));

  priv = MNB_SWITCHER (self)->priv;

  if (priv->tab_list)
    {
      g_list_free (priv->tab_list);
      priv->tab_list = NULL;
    }

  mnb_drop_down_set_child (MNB_DROP_DOWN (self), NULL);
  priv->table = NULL;
  priv->last_focused = NULL;
  priv->selected = NULL;

  CLUTTER_ACTOR_CLASS (mnb_switcher_parent_class)->hide (self);
}

static void
mnb_switcher_finalize (GObject *object)
{
  MnbSwitcher *switcher = MNB_SWITCHER (object);

  g_list_free (switcher->priv->last_workspaces);

  G_OBJECT_CLASS (mnb_switcher_parent_class)->finalize (object);
}

static void
mnb_switcher_class_init (MnbSwitcherClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);

  object_class->finalize = mnb_switcher_finalize;

  actor_class->show = mnb_switcher_show;
  actor_class->hide = mnb_switcher_hide;

  g_type_class_add_private (klass, sizeof (MnbSwitcherPrivate));
}

static void
mnb_switcher_init (MnbSwitcher *self)
{
  self->priv = MNB_SWITCHER_GET_PRIVATE (self);
}

NbtkWidget*
mnb_switcher_new (MutterPlugin *plugin)
{
  MnbSwitcher *switcher;

  g_return_val_if_fail (MUTTER_PLUGIN (plugin), NULL);

  switcher = g_object_new (MNB_TYPE_SWITCHER, NULL);
  switcher->priv->plugin = plugin;

  return NBTK_WIDGET (switcher);
}


static void
select_inner_foreach_cb (ClutterActor *child, gpointer data)
{
  MnbSwitcherAppPrivate *app_priv = MNB_SWITCHER_APP (child)->priv;
  MetaWindow            *meta_win = data;
  MetaWindow            *my_win;

  my_win = mutter_window_get_meta_window (app_priv->mw);

  if (meta_win == my_win)
    {
      clutter_actor_set_name (child, "switcher-application-active");

      app_priv->switcher->priv->selected = app_priv->mw;
    }
  else
    clutter_actor_set_name (child, "");
}

static void
select_outer_foreach_cb (ClutterActor *child, gpointer data)
{
  gint          row;
  ClutterActor *parent;
  MetaWindow   *meta_win = data;

  parent = clutter_actor_get_parent (child);

  clutter_container_child_get (CLUTTER_CONTAINER (parent), child,
                               "row", &row, NULL);

  /* Skip the header row */
  if (!row)
    return;

  if (!NBTK_IS_TABLE (child))
    return;

  clutter_container_foreach (CLUTTER_CONTAINER (child),
                             select_inner_foreach_cb,
                             meta_win);
}

void
mnb_switcher_select_window (MnbSwitcher *switcher, MetaWindow *meta_win)
{
  MnbSwitcherPrivate *priv = switcher->priv;

  if (!priv->table)
    return;

  g_debug ("selecting window %p\n", meta_win);

  clutter_container_foreach (CLUTTER_CONTAINER (priv->table),
                             select_outer_foreach_cb, meta_win);
}

void
mnb_switcher_activate_selection (MnbSwitcher *switcher, gboolean close,
                                 guint timestamp)
{
  MnbSwitcherPrivate *priv = switcher->priv;

  MetaWindow                 *window;
  MetaWorkspace              *workspace;
  MetaWorkspace              *active_workspace;
  MetaScreen                 *screen;
  MutterPlugin               *plugin;

  if (!priv->selected)
    return;

  plugin           = switcher->priv->plugin;
  window           = mutter_window_get_meta_window (priv->selected);
  screen           = meta_window_get_screen (window);
  workspace        = meta_window_get_workspace (window);
  active_workspace = meta_screen_get_active_workspace (screen);

  g_debug ("activating %p\n", window);

  if (close)
    {
      clutter_actor_hide (CLUTTER_ACTOR (switcher));
      hide_panel (plugin);
    }

  if (!active_workspace || (active_workspace == workspace))
    {
      meta_window_activate_with_workspace (window, timestamp, workspace);
    }
  else
    {
      meta_workspace_activate_with_focus (workspace, window, timestamp);
    }
}

MetaWindow *
mnb_switcher_get_selection (MnbSwitcher *switcher)
{
  MnbSwitcherPrivate *priv = switcher->priv;

  if (!priv->selected)
    return NULL;

  g_debug ("currently selected %p\n",
           mutter_window_get_meta_window (priv->selected));

  return mutter_window_get_meta_window (priv->selected);
}

static gint
tablist_find_func (gconstpointer a, gconstpointer b)
{
  ClutterActor          *clone    = CLUTTER_ACTOR (a);
  MetaWindow            *meta_win = META_WINDOW (b);
  MetaWindow            *my_win;
  MnbSwitcherAppPrivate *app_priv = MNB_SWITCHER_APP (clone)->priv;

  my_win = mutter_window_get_meta_window (app_priv->mw);

  if (my_win == meta_win)
    return 0;

  return 1;
}

/*
 * Return the next window that Alt+Tab should advance to.
 *
 * The current parameter indicates where we should start from; if NULL, start
 * from the beginning of our Alt+Tab cycle list.
 *
 * FIXME -- this just a stub using the complete list of mutter windows.
 */
MetaWindow *
mnb_switcher_get_next_window (MnbSwitcher *switcher,
                              MetaWindow  *current,
                              gboolean     backward)
{
  MnbSwitcherPrivate    *priv = switcher->priv;
  GList                 *l;
  ClutterActor          *next = NULL;
  MnbSwitcherAppPrivate *next_priv;

  if (!current)
    {
      if (!priv->selected)
        return NULL;

      return mutter_window_get_meta_window (priv->selected);
    }

  if (!priv->tab_list)
    {
      g_warning ("No tablist in existence!\n");

      return NULL;
    }

  l = g_list_find_custom (priv->tab_list, current, tablist_find_func);

  if (!backward)
    {
      if (!l || !l->next)
        next = priv->tab_list->data;
      else
        next = l->next->data;
    }
  else
    {
      if (!l || !l->prev)
        next = g_list_last (priv->tab_list)->data;
      else
        next = l->prev->data;
    }

  next_priv = MNB_SWITCHER_APP (next)->priv;

  return mutter_window_get_meta_window (next_priv->mw);
}

