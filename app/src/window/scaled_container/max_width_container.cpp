#include "max_width_container.h"

// This works basically likg AdwClamp at the moment unfortunately
struct _MaxWidthContainer
{
    GtkWidget parent_instance;

    GtkWidget *child;
    int max_width;
};

G_DEFINE_TYPE(
    MaxWidthContainer,
    max_width_container,
    GTK_TYPE_WIDGET
)

static void
max_width_container_dispose(GObject *object)
{
    MaxWidthContainer *self = APP_MAX_WIDTH_CONTAINER(object);

    if (self->child)
    {
        gtk_widget_unparent(self->child);
        self->child = NULL;
    }

    G_OBJECT_CLASS(max_width_container_parent_class)
        ->dispose(object);
}

static void
max_width_container_measure(
    GtkWidget* widget,
    GtkOrientation orientation,
    int for_size,
    int* minimum,
    int* natural,
    int* minimum_baseline,
    int* natural_baseline)
{
    MaxWidthContainer* self = APP_MAX_WIDTH_CONTAINER(widget);

    if (!self->child)
    {
        *minimum = 0;
        *natural = 0;
        return;
    }

    gtk_widget_measure(
        self->child,
        orientation,
        for_size,
        minimum,
        natural,
        minimum_baseline,
        natural_baseline);


    if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
        *natural = MIN(*natural, self->max_width);
        *minimum = MIN(*minimum, self->max_width);
    }
}

static void
max_width_container_size_allocate(
    GtkWidget* widget,
    int width,
    int height,
    int baseline)
{
    MaxWidthContainer* self = APP_MAX_WIDTH_CONTAINER(widget);

    if (!self->child)
        return;

    int child_width = MIN(width, self->max_width);

    int x = (width - child_width) / 2;

    graphene_point_t point = GRAPHENE_POINT_INIT(static_cast<float>(x), 0);

    GskTransform* transform =
        gsk_transform_translate(
            NULL,
            &point);


    gtk_widget_allocate(
        self->child,
        child_width,
        height,
        baseline,
        transform);

    gsk_transform_unref(transform);
}

static void
max_width_container_snapshot(
    GtkWidget* widget,
    GtkSnapshot* snapshot)
{
    MaxWidthContainer* self = APP_MAX_WIDTH_CONTAINER(widget);

    if (self->child)
        gtk_widget_snapshot_child(
            widget,
            self->child,
            snapshot);
}

void
max_width_container_set_child(
    MaxWidthContainer* self,
    GtkWidget* child)
{
    if (self->child)
        gtk_widget_unparent(self->child);

    self->child = child;

    if (child)
        gtk_widget_set_parent(
            child,
            GTK_WIDGET(self));
}

static void
max_width_container_init(MaxWidthContainer* self)
{
    self->max_width = 800;
}

GtkWidget*
max_width_container_new(int max_width)
{
    MaxWidthContainer* self =
        APP_MAX_WIDTH_CONTAINER(g_object_new(
            MAX_WIDTH_CONTAINER_TYPE,
            NULL));

    self->max_width = max_width;

    return GTK_WIDGET(self);
}

void
max_width_container_set_max_width(
    MaxWidthContainer* self,
    int max_width)
{
    self->max_width = max_width;

    gtk_widget_queue_resize(
        GTK_WIDGET(self));
}

static void
max_width_container_class_init(
    MaxWidthContainerClass* klass)
{
    GtkWidgetClass* widget_class =
        GTK_WIDGET_CLASS(klass);

    GObjectClass* object_class =
        G_OBJECT_CLASS(klass);

    object_class->dispose =
        max_width_container_dispose;

    widget_class->measure =
        max_width_container_measure;

    widget_class->size_allocate =
        max_width_container_size_allocate;

    widget_class->snapshot =
        max_width_container_snapshot;
}