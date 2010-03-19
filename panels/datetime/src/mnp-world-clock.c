/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2010 Intel Corporation.
 *
 * Author: Srinivasa Ragavan <srini@linux.intel.com>
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
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <gtk/gtk.h>
#include <gio/gdesktopappinfo.h>
#include <glib/gi18n.h>


#include <moblin-panel/mpl-panel-clutter.h>
#include <moblin-panel/mpl-panel-common.h>
#include <moblin-panel/mpl-entry.h>

#include "mnp-world-clock.h"
#include "mnp-utils.h"
#include "mnp-button-item.h"

#include "mnp-clock-tile.h"
#include "mnp-clock-area.h"


G_DEFINE_TYPE (MnpWorldClock, mnp_world_clock, MX_TYPE_BOX_LAYOUT)

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), MNP_TYPE_WORLD_CLOCK, MnpWorldClockPrivate))

#define TIMEOUT 250


typedef struct _MnpWorldClockPrivate MnpWorldClockPrivate;

struct _MnpWorldClockPrivate {
	MxEntry *search_location;
	MxListView *zones_list;
	ClutterModel *zones_model;
	ClutterActor *scroll;
	ClutterActor *entry_box;

	ClutterActor *add_location;

	const char *search_text;

	MnpClockArea *area;

	GPtrArray *zones;
	guint completion_timeout;
	gboolean completion_inited;
};

static void construct_completion (MnpWorldClock *world_clock);

static void
mnp_world_clock_dispose (GObject *object)
{
  /* MnpWorldClockPrivate *priv = GET_PRIVATE (object); */

  G_OBJECT_CLASS (mnp_world_clock_parent_class)->dispose (object);
}

static void
mnp_world_clock_finalize (GObject *object)
{
  /* MnpWorldClockPrivate *priv = GET_PRIVATE (object); */

  G_OBJECT_CLASS (mnp_world_clock_parent_class)->finalize (object);
}

static void
mnp_world_clock_class_init (MnpWorldClockClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (MnpWorldClockPrivate));

  object_class->dispose = mnp_world_clock_dispose;
  object_class->finalize = mnp_world_clock_finalize;
}


static void
mnp_world_clock_init (MnpWorldClock *self)
{
  MnpWorldClockPrivate *priv = GET_PRIVATE (self);

  priv->search_text = "";
  priv->zones = NULL;
  priv->completion_inited = FALSE;
}

static gboolean
start_search (MnpWorldClock *area)
{
	MnpWorldClockPrivate *priv = GET_PRIVATE (area);

	priv->completion_timeout = 0;

	if (!priv->completion_inited) {
		priv->completion_inited = TRUE;
		construct_completion (area);
	}
	priv->search_text = mx_entry_get_text (priv->search_location);

	if (!priv->search_text || (strlen(priv->search_text) < 3))
		clutter_actor_hide(priv->scroll);
	
	if (priv->search_text && (strlen(priv->search_text) > 2))
		g_signal_emit_by_name (priv->zones_model, "filter-changed");
	if (priv->search_text && (strlen(priv->search_text) > 2)) {
		clutter_actor_show(priv->scroll);
		clutter_actor_raise_top (priv->scroll);
	}


	return FALSE;
}

static void
text_changed_cb (MxEntry *entry, GParamSpec *pspec, void *user_data)
{
	MnpWorldClockPrivate *priv = GET_PRIVATE (user_data);

	if (priv->completion_timeout != 0)
		g_source_remove(priv->completion_timeout);

	priv->completion_timeout = g_timeout_add (500, (GSourceFunc) start_search, user_data);

}

static char *
find_word (const char *full_name, const char *word, int word_len,
	   gboolean whole_word, gboolean is_first_word)
{
    char *p = (char *)full_name - 1;

    while ((p = strchr (p + 1, *word))) {
	if (strncmp (p, word, word_len) != 0)
	    continue;

	if (p > (char *)full_name) {
	    char *prev = g_utf8_prev_char (p);

	    /* Make sure p points to the start of a word */
	    if (g_unichar_isalpha (g_utf8_get_char (prev)))
		continue;

	    /* If we're matching the first word of the key, it has to
	     * match the first word of the location, city, state, or
	     * country. Eg, it either matches the start of the string
	     * (which we already know it doesn't at this point) or
	     * it is preceded by the string ", " (which isn't actually
	     * a perfect test. FIXME)
	     */
	    if (is_first_word) {
		if (prev == (char *)full_name || strncmp (prev - 1, ", ", 2) != 0)
		    continue;
	    }
	}

	if (whole_word && g_unichar_isalpha (g_utf8_get_char (p + word_len)))
	    continue;

	return p;
    }
    return NULL;
}

static gboolean 
filter_zone (ClutterModel *model, ClutterModelIter *iter, gpointer user_data)
{
	MnpWorldClockPrivate *priv = GET_PRIVATE (user_data);
	char *name, *name_mem;
	GWeatherLocation *loc;
	gboolean is_first_word = TRUE, match;
	int len;
	char *key;

	if (!priv->search_text || !*priv->search_text)
		return TRUE;

	key = g_ascii_strdown(priv->search_text, -1);
	clutter_model_iter_get (iter, 
			GWEATHER_LOCATION_ENTRY_COL_COMPARE_NAME, &name_mem,
			GWEATHER_LOCATION_ENTRY_COL_LOCATION, &loc,
			-1);
	name = name_mem;

    	if (!loc) {
		g_free (name_mem);
		g_free (key);
		return FALSE;
    	}

    	/* All but the last word in KEY must match a full word from NAME,
     	 * in order (but possibly skipping some words from NAME).
     	 */
	len = strcspn (key, " ");
	while (key[len]) {
		name = find_word (name, key, len, TRUE, is_first_word);
		if (!name) {
	    		g_free (name_mem);
			g_free (key);
	    		return FALSE;
		}

		key += len;
		while (*key && !g_unichar_isalpha (g_utf8_get_char (key)))
	    		key = g_utf8_next_char (key);
		while (*name && !g_unichar_isalpha (g_utf8_get_char (name)))
	    		name = g_utf8_next_char (name);

		len = strcspn (key, " ");
		is_first_word = FALSE;
    	}

	/* The last word in KEY must match a prefix of a following word in NAME */
	match = find_word (name, key, strlen (key), FALSE, is_first_word) != NULL;
	g_free (name_mem);
	g_free (key);
	return match;
}

static void
clear_btn_clicked_cb (ClutterActor *button, MnpWorldClock *world_clock)
{
	MnpWorldClockPrivate *priv = GET_PRIVATE (world_clock);

  	mx_entry_set_text (priv->search_location, "");
  	clutter_actor_hide (priv->scroll);
}

static void
add_location_clicked_cb (ClutterActor *button, MnpWorldClock *world_clock)
{
	MnpWorldClockPrivate *priv = GET_PRIVATE (world_clock);
	GWeatherLocation *location;
	MnpClockTile *tile;
	MnpZoneLocation *loc = g_new0(MnpZoneLocation, 1);
	const GWeatherTimezone *tzone;

	mx_list_view_set_model (MX_LIST_VIEW (priv->zones_list), NULL);

	priv->search_text = NULL;
	g_signal_emit_by_name (priv->zones_model, "filter-changed");
	
	location = mnp_utils_get_location_from_display (priv->zones_model, mx_entry_get_text (priv->search_location));
	loc->display = g_strdup(mx_entry_get_text (priv->search_location));
	loc->city = g_strdup(gweather_location_get_city_name (location));
	tzone =  gweather_location_get_timezone (location);
	loc->tzid = g_strdup(gweather_timezone_get_tzid (tzone));


	g_ptr_array_add (priv->zones, loc);
	mnp_save_zones(priv->zones);

	mx_entry_set_text (priv->search_location, "");

	tile = mnp_clock_tile_new (loc, mnp_clock_area_get_time(priv->area));
	mnp_clock_area_add_tile (priv->area, tile);

	priv->search_text = "asd";
	g_signal_emit_by_name (priv->zones_model, "filter-changed");	
	mx_list_view_set_model (MX_LIST_VIEW (priv->zones_list), priv->zones_model);
	
	if (priv->zones->len >= 4)
		clutter_actor_hide (priv->entry_box);
}


static void 
mnp_completion_done (gpointer data, const char *zone)
{
	MnpWorldClock *clock = (MnpWorldClock *)data;
	MnpWorldClockPrivate *priv = GET_PRIVATE (clock);
	
	clutter_actor_hide (priv->scroll);
	g_signal_handlers_block_by_func (priv->search_location, text_changed_cb, clock);
	mx_entry_set_text (priv->search_location, zone);
	g_signal_handlers_unblock_by_func (priv->search_location, text_changed_cb, clock);
	add_location_clicked_cb (NULL, clock);

}

static void
insert_in_ptr_array (GPtrArray *array, gpointer data, int pos)
{
	int i;

	g_ptr_array_add (array, NULL);
	for (i=array->len-2; i>=pos ; i--) {
		array->pdata[i+1] = array->pdata[i];
	}
	array->pdata[pos] = data;
}

static void
zone_reordered_cb (MnpZoneLocation *location, int new_pos, MnpWorldClock *clock)
{
	MnpWorldClockPrivate *priv = GET_PRIVATE (clock);
	char *cloc = location->display;
	int i;

	for (i=0; i<priv->zones->len; i++) {
		MnpZoneLocation *loc = (MnpZoneLocation *)priv->zones->pdata[i];
		if (strcmp(cloc, loc->display) == 0) {
			break;
		}
		
	}

	g_ptr_array_remove_index (priv->zones, i);
	insert_in_ptr_array (priv->zones, (gpointer) location, new_pos);

	mnp_save_zones (priv->zones);
}

static void
zone_removed_cb (MnpClockArea *area, char *display, MnpWorldClock *clock)
{
	MnpWorldClockPrivate *priv = GET_PRIVATE (clock);	
	int i;
	MnpZoneLocation *loc;

	for (i=0; i<priv->zones->len; i++) {
		loc = (MnpZoneLocation *)priv->zones->pdata[i];		
		if (strcmp(display, loc->display) == 0) {
			break;
		}
	}

	if (i != priv->zones->len) {
		g_ptr_array_remove_index (priv->zones, i);
		mnp_save_zones(priv->zones);
	}

	if (priv->zones->len < 4)
		clutter_actor_show (priv->entry_box);
	
}

static void
construct_completion (MnpWorldClock *world_clock)
{
	ClutterActor *scroll, *view, *stage;
	MnpWorldClockPrivate *priv = GET_PRIVATE (world_clock);
	ClutterModel *model;
	MnpButtonItem *button_item;

	stage = clutter_stage_get_default ();


	model = mnp_get_world_timezones ();
	priv->zones_model = model;
	clutter_model_set_filter (model, filter_zone, world_clock, NULL);
	priv->search_text = "asd";

	scroll = mx_scroll_view_new ();
	clutter_actor_set_name (scroll, "completion-scroll-bin");
	priv->scroll = scroll;
	clutter_actor_set_size (scroll, -1, 300);	
	clutter_container_add_actor ((ClutterContainer *)stage, scroll);
      	clutter_actor_raise_top((ClutterActor *) scroll);
	clutter_actor_set_position (scroll, 6, 40);  
	clutter_actor_hide (scroll);

	view = mx_list_view_new ();
	clutter_actor_set_name (view, "completion-list-view");
	priv->zones_list = (MxListView *)view;

	clutter_container_add_actor (CLUTTER_CONTAINER (scroll), view);
	mx_list_view_set_model (MX_LIST_VIEW (view), model);

	button_item = mnp_button_item_new ((gpointer)world_clock, mnp_completion_done);
	mx_list_view_set_factory (MX_LIST_VIEW (view), (MxItemFactory *)button_item);
	mx_list_view_add_attribute (MX_LIST_VIEW (view), "label", 0);
}

static void
mnp_world_clock_construct (MnpWorldClock *world_clock)
{
	ClutterActor *entry, *box, *stage;
	MxBoxLayout *table = (MxBoxLayout *)world_clock;
	gfloat width, height;
	MnpWorldClockPrivate *priv = GET_PRIVATE (world_clock);

	stage = clutter_stage_get_default ();

	mx_box_layout_set_orientation ((MxBoxLayout *)world_clock, MX_VERTICAL);
	mx_box_layout_set_pack_start ((MxBoxLayout *)world_clock, FALSE);
	mx_box_layout_set_spacing ((MxBoxLayout *)world_clock, 4);

	priv->completion_timeout = 0;

	box = mx_box_layout_new ();
	priv->entry_box = box;
	clutter_actor_set_name ((ClutterActor *)box, "search-entry-box");
	
	mx_box_layout_set_orientation ((MxBoxLayout *)box, MX_HORIZONTAL);
	mx_box_layout_set_pack_start ((MxBoxLayout *)box, FALSE);
	mx_box_layout_set_spacing ((MxBoxLayout *)box, 4);
	
	entry = mx_entry_new ();
	priv->search_location = (MxEntry *)entry;
	mx_entry_set_hint_text (MX_ENTRY (entry), _("Enter a country or city"));
	mx_entry_set_secondary_icon_from_file (MX_ENTRY (entry),
                                           THEMEDIR"/edit-clear.png");
  	g_signal_connect (entry, "secondary-icon-clicked",
                    	G_CALLBACK (clear_btn_clicked_cb), world_clock);

	/* Set just size more than the hint text */
	clutter_actor_get_size (entry, &width, &height);
	clutter_actor_set_size (entry, width+10, -1);
	g_signal_connect (G_OBJECT (entry),
                    "notify::text", G_CALLBACK (text_changed_cb), world_clock);
	
	clutter_container_add_actor ((ClutterContainer *)box, entry);
	
	priv->add_location = mx_label_new_with_text (_("Search"));
	clutter_actor_set_name ((ClutterActor *)box, "search-entry-label");
	
	clutter_container_add_actor ((ClutterContainer *)box, priv->add_location);
  	/* g_signal_connect (priv->add_location, "clicked",
                    	G_CALLBACK (add_location_clicked_cb), world_clock); */
	clutter_container_child_set (CLUTTER_CONTAINER (box),
                               priv->add_location,
                               "expand", FALSE,
			       "y-fill", FALSE,
			       "y-align", MX_ALIGN_MIDDLE,
                               NULL);
	

	clutter_container_add_actor (CLUTTER_CONTAINER(table), box);
	clutter_container_child_set (CLUTTER_CONTAINER (table),
                               box,
                               "expand", FALSE,
			       "y-fill", FALSE,		
			       "x-fill", TRUE,			       			       
                               NULL);





	priv->area = mnp_clock_area_new ();

	clutter_actor_set_reactive ((ClutterActor *)priv->area, TRUE);
	clutter_actor_set_name ((ClutterActor *)priv->area, "clock-area");

	clutter_container_add_actor ((ClutterContainer *)stage, (ClutterActor *)priv->area);
	
	clutter_actor_lower_bottom ((ClutterActor *)priv->area);
	mx_droppable_enable ((MxDroppable *)priv->area);
	g_object_ref ((GObject *)priv->area);
	clutter_container_remove_actor ((ClutterContainer *)stage, (ClutterActor *)priv->area);
	clutter_container_add_actor (CLUTTER_CONTAINER (table), (ClutterActor *)priv->area);
	clutter_container_child_set (CLUTTER_CONTAINER (table),
                               (ClutterActor *)priv->area,
                               "expand", TRUE,
			       "y-fill", TRUE,
			       "x-fill", TRUE,
                               NULL);


	priv->zones = mnp_load_zones ();
	mnp_clock_area_refresh_time (priv->area);
	mnp_clock_area_set_zone_remove_cb (priv->area, (ZoneRemovedFunc) zone_removed_cb, (gpointer)world_clock);
	mnp_clock_area_set_zone_reordered_cb (priv->area, (ClockZoneReorderedFunc) zone_reordered_cb, (gpointer)world_clock);

	if (priv->zones->len) {
		int i=0;

		for (i=0; i<priv->zones->len; i++) {
			MnpZoneLocation *loc = (MnpZoneLocation *)priv->zones->pdata[i];
			MnpClockTile *tile = mnp_clock_tile_new (loc, mnp_clock_area_get_time(priv->area));
			mnp_clock_area_add_tile (priv->area, tile);
		}
		if (priv->zones->len >= 4)
			clutter_actor_hide (priv->entry_box);
	}

}

ClutterActor *
mnp_world_clock_new (void)
{
  MnpWorldClock *panel = g_object_new (MNP_TYPE_WORLD_CLOCK, NULL);

  mnp_world_clock_construct (panel);

  return (ClutterActor *)panel;
}

static void
dropdown_show_cb (MplPanelClient *client,
                  gpointer        userdata)
{
  //MnpWorldClockPrivate *priv = GET_PRIVATE (userdata);

  /* give focus to the actor */
  /*clutter_actor_grab_key_focus (priv->entry);*/
}


static void
dropdown_hide_cb (MplPanelClient *client,
                  gpointer        userdata)
{
  //MnpWorldClockPrivate *priv = GET_PRIVATE (userdata);

  /* Reset search. */
  /* mpl_entry_set_text (MPL_ENTRY (priv->entry), ""); */
}

