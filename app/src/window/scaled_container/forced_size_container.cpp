#define ENABLE_VERBOSE_LOGS 0

#include "forced_size_container.h"

#include <vector>

struct _ForcedSizeContainer
{
    GtkWidget parent_instance;

    int pixel_size;
};

G_DEFINE_TYPE(ForcedSizeContainer, forced_size_container, GTK_TYPE_WIDGET)

enum {
    PROP_0,
    PROP_PIXEL_SIZE,
    N_PROPS
};

static GParamSpec* props[N_PROPS];

static void
forced_size_container_set_property(GObject* object, guint prop_id, const GValue* value, GParamSpec* pspec)
{
    ForcedSizeContainer* self = APP_FORCED_SIZE_CONTAINER(object);

    switch (prop_id) {
    case PROP_PIXEL_SIZE:
        self->pixel_size = g_value_get_int(value);
        gtk_widget_queue_resize(GTK_WIDGET(self));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
forced_size_conainter_get_property(GObject* object, guint prop_id, GValue *value, GParamSpec* pspec)
{
    ForcedSizeContainer* self = APP_FORCED_SIZE_CONTAINER(object);

    switch (prop_id) {
    case PROP_PIXEL_SIZE:
        g_value_set_int(value, self->pixel_size);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
forced_size_container_measure(GtkWidget *widget,
               GtkOrientation orientation,
               int for_size,
               int *minimum,
               int *natural,
               int *minimum_baseline,
               int *natural_baseline)
{
    ForcedSizeContainer *self = (ForcedSizeContainer *)widget;

    if(self->pixel_size >= 0)
    {
        *minimum = self->pixel_size;
        *natural = self->pixel_size;
    }
    else
    {
        if(GtkWidget* child = gtk_widget_get_first_child(widget))
        {
            gtk_widget_measure(child, orientation, for_size, minimum, natural, minimum_baseline, natural_baseline);
        }
        else
        {
            *minimum = 0;
            *natural = 0;
        }
    }
}

static void
forced_size_container_size_allocate(GtkWidget* widget, int width, int height, int baseline)
{
    ForcedSizeContainer* self = APP_FORCED_SIZE_CONTAINER(widget);

    GtkWidget* child = gtk_widget_get_first_child(widget);
    if(!child) return;

    int child_width, child_height;

    //obtains the natural width and height of the widget 
    gtk_widget_measure(child, GTK_ORIENTATION_HORIZONTAL, -1, 
                       NULL, &child_width, NULL, NULL);
    gtk_widget_measure(child, GTK_ORIENTATION_VERTICAL, -1, 
                       NULL, &child_height, NULL, NULL);

    //0 division prevention
    if(child_width <= 0) child_width = 1;
    if(child_height <= 0) child_height = 1;

    float scale_x = (float)self->pixel_size / child_width;
    float scale_y = (float)self->pixel_size / child_height;
    
    //using minimum to do uniform scaling, ensuring neither side clips
    float scale = MIN(scale_x, scale_y);

    //ensuring centering of widget if one side is smaller than other
    float offset_x = (width - (child_width * scale)) / 2.0f;
    float offset_y = (height - (child_height * scale)) / 2.0f;

    //transform goes from child coords to parent coords (rendering order of ops)
    GskTransform *transform = gsk_transform_new();
    
    if (offset_x > 0 || offset_y > 0) {
        graphene_point_t offset = { offset_x, offset_y };
        transform = gsk_transform_translate(transform, &offset);
    }
    
    transform = gsk_transform_scale(transform, scale, scale);

    gtk_widget_allocate(child, child_width, child_height, baseline, transform);
}

static void
forced_size_container_snapshot(GtkWidget* widget, GtkSnapshot* snapshot)
{
    //no extra logic necessary, everything handled from allocate
    gtk_widget_snapshot_child(widget, gtk_widget_get_first_child(widget), snapshot);
}

static void
forced_size_container_dispose(GObject* object)
{
    GtkWidget* self_widget = GTK_WIDGET(object);
    for(GtkWidget* child = gtk_widget_get_first_child(self_widget); child; child = gtk_widget_get_first_child(self_widget))
    {
        gtk_widget_unparent(child);
    }
    G_OBJECT_CLASS(forced_size_container_parent_class)->dispose(object);
}

static void
forced_size_container_init(ForcedSizeContainer* self)
{
}

static void
forced_size_container_class_init(ForcedSizeContainerClass* klass)
{
    GTK_WIDGET_CLASS(klass)->measure = forced_size_container_measure;
    GTK_WIDGET_CLASS(klass)->size_allocate = forced_size_container_size_allocate;
    GTK_WIDGET_CLASS(klass)->snapshot = forced_size_container_snapshot;

    GObjectClass* object_class = G_OBJECT_CLASS(klass);
    object_class->set_property = forced_size_container_set_property;
    object_class->get_property = forced_size_conainter_get_property;
    object_class->dispose = forced_size_container_dispose;

    props[PROP_PIXEL_SIZE] = g_param_spec_int(
        "pixel-size", NULL, NULL, INT_MIN, INT_MAX, -1,
        GParamFlags(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)
    );

    g_object_class_install_properties(object_class, N_PROPS, props);
}