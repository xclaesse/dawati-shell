#include "ahoghill-media-tile.h"
#include "ahoghill-results-table.h"

enum {
    PROP_0,
};

enum {
    ITEM_CLICKED,
    LAST_SIGNAL
};

#define TILES_PER_ROW 6
#define ROWS_PER_PAGE 2
#define TILES_PER_PAGE (TILES_PER_ROW * ROWS_PER_PAGE)

#define RESULTS_ROW_SPACING 28
#define RESULTS_COL_SPACING 20

struct _AhoghillResultsTablePrivate {
    AhoghillMediaTile *tiles[TILES_PER_PAGE];
};

#define GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), AHOGHILL_TYPE_RESULTS_TABLE, AhoghillResultsTablePrivate))
G_DEFINE_TYPE (AhoghillResultsTable, ahoghill_results_table, NBTK_TYPE_TABLE);

static guint32 signals[LAST_SIGNAL] = {0, };

static void
ahoghill_results_table_finalize (GObject *object)
{
    g_signal_handlers_destroy (object);
    G_OBJECT_CLASS (ahoghill_results_table_parent_class)->finalize (object);
}

static void
ahoghill_results_table_dispose (GObject *object)
{
    G_OBJECT_CLASS (ahoghill_results_table_parent_class)->dispose (object);
}

static void
ahoghill_results_table_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
    switch (prop_id) {

    default:
        break;
    }
}

static void
ahoghill_results_table_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
    switch (prop_id) {

    default:
        break;
    }
}

static void
ahoghill_results_table_class_init (AhoghillResultsTableClass *klass)
{
    GObjectClass *o_class = (GObjectClass *)klass;

    o_class->dispose = ahoghill_results_table_dispose;
    o_class->finalize = ahoghill_results_table_finalize;
    o_class->set_property = ahoghill_results_table_set_property;
    o_class->get_property = ahoghill_results_table_get_property;

    g_type_class_add_private (klass, sizeof (AhoghillResultsTablePrivate));

    signals[ITEM_CLICKED] = g_signal_new ("item-clicked",
                                          G_TYPE_FROM_CLASS (klass),
                                          G_SIGNAL_RUN_FIRST |
                                          G_SIGNAL_NO_RECURSE, 0, NULL, NULL,
                                          g_cclosure_marshal_VOID__INT,
                                          G_TYPE_NONE, 1,
                                          G_TYPE_INT);
}

static int
find_tile (AhoghillResultsTable *pane,
           NbtkWidget           *widget)
{
    AhoghillResultsTablePrivate *priv = pane->priv;
    int i;

    for (i = 0; i < TILES_PER_PAGE; i++) {
        if (priv->tiles[i] == (AhoghillMediaTile *) widget) {
            return i;
        }
    }

    return -1;
}

static gboolean
tile_pressed_cb (ClutterActor         *actor,
                 ClutterButtonEvent   *event,
                 AhoghillResultsTable *table)
{
    int tileno;

    tileno = find_tile (table, (NbtkWidget *) actor);
    if (tileno == -1) {
        return TRUE;
    }

    return TRUE;
}

static gboolean
tile_released_cb (ClutterActor         *actor,
                  ClutterButtonEvent   *event,
                  AhoghillResultsTable *table)
{
    int tileno;

    tileno = find_tile (table, (NbtkWidget *) actor);
    if (tileno == -1) {
        return TRUE;
    }

    g_signal_emit (table, signals[ITEM_CLICKED], 0, tileno);

    return TRUE;
}

static void
tile_dnd_begin_cb (NbtkWidget           *widget,
                   ClutterActor         *dragged,
                   ClutterActor         *icon,
                   int                   x,
                   int                   y,
                   AhoghillResultsTable *table)
{
    int tileno;

    tileno = find_tile (table, widget);
    if (tileno == -1) {
        return;
    }
}
static void
tile_dnd_motion_cb (NbtkWidget           *widget,
                    ClutterActor         *dragged,
                    ClutterActor         *icon,
                    int                   x,
                    int                   y,
                    AhoghillResultsTable *table)
{
    int tileno;

    tileno = find_tile (table, widget);
    if (tileno == -1) {
        return;
    }
}

static void
tile_dnd_end_cb (NbtkWidget           *widget,
                 ClutterActor         *dragged,
                 ClutterActor         *icon,
                 int                   x,
                 int                   y,
                 AhoghillResultsTable *table)
{
    int tileno;

    tileno = find_tile (table, widget);
    if (tileno == -1) {
        return;
    }
}

static void
ahoghill_results_table_init (AhoghillResultsTable *self)
{
    AhoghillResultsTablePrivate *priv;
    int i;

    self->priv = GET_PRIVATE (self);
    priv = self->priv;

    clutter_actor_set_name (CLUTTER_ACTOR (self), "media-pane-results-table");
    nbtk_table_set_col_spacing (NBTK_TABLE (self), RESULTS_COL_SPACING);
    nbtk_table_set_row_spacing (NBTK_TABLE (self), RESULTS_ROW_SPACING);

    for (i = 0; i < TILES_PER_PAGE; i++) {
        priv->tiles[i] = g_object_new (AHOGHILL_TYPE_MEDIA_TILE, NULL);

        nbtk_widget_set_dnd_threshold (NBTK_WIDGET (priv->tiles[i]), 10);
        g_signal_connect (priv->tiles[i], "button-press-event",
                          G_CALLBACK (tile_pressed_cb), self);
        g_signal_connect (priv->tiles[i], "button-release-event",
                          G_CALLBACK (tile_released_cb), self);
        g_signal_connect (priv->tiles[i], "dnd-begin",
                          G_CALLBACK (tile_dnd_begin_cb), self);
        g_signal_connect (priv->tiles[i], "dnd-motion",
                          G_CALLBACK (tile_dnd_motion_cb), self);
        g_signal_connect (priv->tiles[i], "dnd-end",
                          G_CALLBACK (tile_dnd_end_cb), self);

        nbtk_table_add_actor_with_properties (NBTK_TABLE (self),
                                              (ClutterActor *) priv->tiles[i],
                                              (i / TILES_PER_ROW),
                                              i % TILES_PER_ROW,
                                              "x-align", 0.5,
                                              NULL);
        clutter_actor_hide ((ClutterActor *) priv->tiles[i]);
    }
}

void
ahoghill_results_table_update (AhoghillResultsTable *self,
                               GPtrArray            *results,
                               guint                 page_number)
{
    AhoghillResultsTablePrivate *priv = self->priv;
    int i, count, start;

    start = page_number * TILES_PER_PAGE;
    count = MIN (results->len - start, TILES_PER_PAGE);

    for (i = 0; i < count; i++) {
         g_object_set (priv->tiles[i],
                       "item", results->pdata[i + start],
                       NULL);
         clutter_actor_show ((ClutterActor *) priv->tiles[i]);
    }

    /* Clear the rest of the results */
    for (i = count; i < TILES_PER_PAGE; i++) {
        g_object_set (priv->tiles[i],
                      "item", NULL,
                      NULL);
        clutter_actor_hide ((ClutterActor *) priv->tiles[i]);
    }
}