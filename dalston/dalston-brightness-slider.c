#include "dalston-brightness-slider.h"
#include <dalston/dalston-brightness-manager.h>

G_DEFINE_TYPE (DalstonBrightnessSlider, dalston_brightness_slider, GTK_TYPE_HSCALE)

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), DALSTON_TYPE_BRIGHTNESS_SLIDER, DalstonBrightnessSliderPrivate))

typedef struct _DalstonBrightnessSliderPrivate DalstonBrightnessSliderPrivate;

struct _DalstonBrightnessSliderPrivate {
  DalstonBrightnessManager *manager;

  gint num_levels;
};

enum
{
  PROP_0,
  PROP_MANAGER
};

static void
dalston_brightness_slider_get_property (GObject *object, guint property_id,
                              GValue *value, GParamSpec *pspec)
{
  DalstonBrightnessSliderPrivate *priv = GET_PRIVATE (object);

  switch (property_id) {
    case PROP_MANAGER:
      g_value_set_object (value, priv->manager);
      break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
dalston_brightness_slider_set_property (GObject *object, guint property_id,
                              const GValue *value, GParamSpec *pspec)
{
  DalstonBrightnessSliderPrivate *priv = GET_PRIVATE (object);

  switch (property_id) {
    case PROP_MANAGER:
      priv->manager = g_value_dup_object (value);
      break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
dalston_brightness_slider_dispose (GObject *object)
{
  DalstonBrightnessSliderPrivate *priv = GET_PRIVATE (object);

  if (priv->manager)
  {
    g_object_unref (priv->manager);
    priv->manager = NULL;
  }

  G_OBJECT_CLASS (dalston_brightness_slider_parent_class)->dispose (object);
}

static void
dalston_brightness_slider_finalize (GObject *object)
{
  G_OBJECT_CLASS (dalston_brightness_slider_parent_class)->finalize (object);
}

static void
_manager_brightness_changed_cb (DalstonBrightnessManager *manager,
                                gint                      value,
                                gpointer                  userdata)
{
  DalstonBrightnessSlider *slider = (DalstonBrightnessSlider *)userdata;
  DalstonBrightnessSliderPrivate *priv = GET_PRIVATE (userdata);

  if (priv->num_levels > 0)
  {
    gtk_range_set_value (GTK_RANGE (slider), value);
  }
}

static void
_manager_num_levels_changed_cb (DalstonBrightnessManager *manager,
                                gint                      num_levels,
                                gpointer                  userdata)
{
  DalstonBrightnessSlider *slider = (DalstonBrightnessSlider *)userdata;
  DalstonBrightnessSliderPrivate *priv = GET_PRIVATE (userdata);

  priv->num_levels = num_levels;
  gtk_range_set_range (GTK_RANGE (slider), 0, num_levels - 1);
}

static void
dalston_brightness_slider_constructed (GObject *object)
{
  DalstonBrightnessSliderPrivate *priv = GET_PRIVATE (object);

  g_signal_connect (priv->manager,
                    "num-levels-changed",
                    (GCallback)_manager_num_levels_changed_cb,
                    object);

  g_signal_connect (priv->manager,
                    "brightness-changed",
                    (GCallback)_manager_brightness_changed_cb,
                    object);

  if (G_OBJECT_CLASS (dalston_brightness_slider_parent_class)->constructed)
  {
    G_OBJECT_CLASS (dalston_brightness_slider_parent_class)->constructed (object);
  }
}

static void
dalston_brightness_slider_class_init (DalstonBrightnessSliderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec *pspec;

  g_type_class_add_private (klass, sizeof (DalstonBrightnessSliderPrivate));

  object_class->get_property = dalston_brightness_slider_get_property;
  object_class->set_property = dalston_brightness_slider_set_property;
  object_class->dispose = dalston_brightness_slider_dispose;
  object_class->finalize = dalston_brightness_slider_finalize;
  object_class->constructed = dalston_brightness_slider_constructed;

  pspec = g_param_spec_object ("manager",
                               "Brightness manager",
                               "The brightness manager that we're going "
                               "to read from / control",
                               DALSTON_TYPE_BRIGHTNESS_MANAGER,
                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_property (object_class, PROP_MANAGER, pspec);
}

static void
dalston_brightness_slider_init (DalstonBrightnessSlider *self)
{
  gtk_scale_set_digits (GTK_SCALE (self), 0);
}

DalstonBrightnessSlider *
dalston_brightness_slider_new (DalstonBrightnessManager *manager)
{
  return g_object_new (DALSTON_TYPE_BRIGHTNESS_SLIDER, 
                       "manager",
                       manager,
                       NULL);
}


