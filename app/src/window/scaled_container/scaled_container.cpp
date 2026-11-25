#define ENABLE_VERBOSE_LOGS 0

#include "scaled_container.h"

// Attempt to implement scaling widget container that could be used in Mini Toolbar implementation.
// Visually and layout-wise, it is fine but it does not work because mouse events are not properly transformed.

struct _ScaledContainer
{
    GtkWidget parent_instance;

    GtkWidget* child;
    double scale;
};

G_DEFINE_TYPE(ScaledContainer, scaled_container, GTK_TYPE_WIDGET)

void
scaled_container_set_child(ScaledContainer* self, GtkWidget* child)
{
    g_return_if_fail(GTK_IS_WIDGET(self));
    g_return_if_fail(!child || GTK_IS_WIDGET(child));

    if (self->child) {
        gtk_widget_unparent(self->child);
    }

    self->child = child;
    if (child) {
        gtk_widget_set_parent(child, GTK_WIDGET(self));
    }
}

enum {
    PROP_0,
    PROP_CHILD,
    PROP_SCALE,
    N_PROPS
};

static GParamSpec* props[N_PROPS];

static void
scaled_container_set_property(GObject* object, guint prop_id, const GValue* value, GParamSpec* pspec)
{
    ScaledContainer* self = APP_SCALED_CONTAINER(object);

    switch (prop_id) {
    case PROP_CHILD:
        scaled_container_set_child(self, GTK_WIDGET(g_value_get_object(value)));
        break;
    case PROP_SCALE:
        self->scale = g_value_get_double(value);
        gtk_widget_queue_resize(GTK_WIDGET(self));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
scaled_conainter_get_property(GObject* object, guint prop_id, GValue *value, GParamSpec* pspec)
{
    ScaledContainer* self = APP_SCALED_CONTAINER(object);

    switch (prop_id) {
    case PROP_CHILD:
        g_value_set_object(value, self->child);
        break;
    case PROP_SCALE:
        g_value_set_double(value, self->scale);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

// static int
// clamp(int x, int min, int max)
// {
//     if (x < min) return min;
//     if (x > max) return max;
//     return x;
// }

static void
scaled_container_measure(GtkWidget *widget,
               GtkOrientation orientation,
               int for_size,
               int *minimum,
               int *natural,
               int *minimum_baseline,
               int *natural_baseline)
{
    ScaledContainer *self = (ScaledContainer *)widget;

    if (!self->child) {
        if (minimum) *minimum = 0;
        if (natural) *natural = 0;
        if (minimum_baseline) *minimum_baseline = -1;
        if (natural_baseline) *natural_baseline = -1;
        return;
    }

    gtk_widget_measure(self->child,
                       orientation,
                       for_size,
                       minimum,
                       natural,
                       minimum_baseline,
                       natural_baseline);
}

// allocate the child a proportionally larger area (we give child size / scale)
static void
scaled_container_size_allocate(GtkWidget* widget, int width, int height, int baseline)
{
    ScaledContainer* self = APP_SCALED_CONTAINER(widget);
    if (!self->child) return;

    GtkAllocation alloc = {0, 0, width, height};
    gtk_widget_size_allocate(self->child, &alloc, baseline);
    GTK_WIDGET_CLASS(scaled_container_parent_class)->size_allocate(widget, width, height, baseline);
}

// scale the snapshot coordinate system, snapshot the child, then pop
static void
scaled_container_snapshot(GtkWidget* widget, GtkSnapshot* snapshot)
{
    ScaledContainer* self = APP_SCALED_CONTAINER(widget);
    if (!self->child) return;

    // Scale coordinate system for subsequent nodes; recorded until pop()
    if (self->scale != 1.0)
        gtk_snapshot_scale(snapshot, (float)self->scale, (float)self->scale);

    // Snapshot the child into the scaled coordinate system
    gtk_widget_snapshot_child(widget, self->child, snapshot);

    // safer way (safer isolation) would something along these lines:
    /*
    GskTransform *transform =
        gsk_transform_scale(NULL, self->scale, self->scale);
    gtk_snapshot_push_transform(snapshot, transform);
    gsk_transform_unref(transform);

    gtk_widget_snapshot_child(widget, self->child, snapshot);

    gtk_snapshot_pop(snapshot);
    */
}

// Override contains() to map pointer events
static gboolean
scaled_container_contains(GtkWidget* widget, double x, double y)
{
    ScaledContainer* self = APP_SCALED_CONTAINER(widget);

    if (!self->child)
        return FALSE;

    /* Inverse scale pointer coordinates to child space */
    double child_x = x / self->scale;
    double child_y = y / self->scale;

    return gtk_widget_contains(self->child, child_x, child_y);
}

static void
scaled_container_dispose(GObject* gobject)
{
    ScaledContainer* self = APP_SCALED_CONTAINER(gobject);
    g_clear_pointer(&self->child, gtk_widget_unparent);
    G_OBJECT_CLASS(scaled_container_parent_class)->dispose(gobject);
}

static void
scaled_container_init(ScaledContainer* self)
{
    self->scale = 1.0;
}

static void
scaled_container_class_init(ScaledContainerClass* klass)
{
    GTK_WIDGET_CLASS(klass)->measure = scaled_container_measure;
    GTK_WIDGET_CLASS(klass)->size_allocate = scaled_container_size_allocate;
    GTK_WIDGET_CLASS(klass)->snapshot = scaled_container_snapshot;
    GTK_WIDGET_CLASS(klass)->contains = scaled_container_contains;

    G_OBJECT_CLASS(klass)->set_property = scaled_container_set_property;
    G_OBJECT_CLASS(klass)->get_property = scaled_conainter_get_property;
    G_OBJECT_CLASS(klass)->dispose = scaled_container_dispose;

    props[PROP_CHILD] = g_param_spec_object(
        "child", NULL, NULL, GTK_TYPE_WIDGET,
        GParamFlags(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)
    );

    props[PROP_SCALE] = g_param_spec_double(
        "scale", NULL, NULL, 0.0, G_MAXDOUBLE, 1.0,
        GParamFlags(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)
    );

    g_object_class_install_properties(G_OBJECT_CLASS(klass), N_PROPS, props);
}