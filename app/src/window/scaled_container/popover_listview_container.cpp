#define ENABLE_VERBOSE_LOGS 1

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include <common/src/defer.h>

#include "popover_listview_container.h"

//NOTE: this custom widget is completely unnecessary, the issue is with how we present popovers, scrolled window works fine

struct _PopoverListviewContainer
{
	GtkWidget parent;

    GtkListView* child;
    GtkScrollbar* vscrollbar;

    int64_t max_height;
};

enum {
    PROP_0,
    PROP_MAX_HEIGHT,
    N_PROPS,
};


static GParamSpec* obj_properties[N_PROPS];

static void popover_listview_container_buildable_iface_init(GtkBuildableIface* iface);

G_DEFINE_TYPE_WITH_CODE(PopoverListviewContainer, popover_listview_container, GTK_TYPE_WIDGET,
    G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE, popover_listview_container_buildable_iface_init);
    );

#define APP_TYPE_POPOVER_LISTVIEW_LAYOUT_MANAGER (popover_listview_layout_manager_get_type())

G_DECLARE_FINAL_TYPE(PopoverListviewLayoutManager, popover_listview_layout_manager, APP, POPOVER_LISTVIEW_LAYOUT_MANAGER, GtkLayoutManager)

static gboolean
on_popover_listview_container_scroll(GtkEventControllerScroll* controller, gdouble dx, gdouble dy, gpointer user_data)
{
    PopoverListviewContainer* container = APP_POPOVER_LISTVIEW_CONTAINER(user_data);
    GdkScrollUnit scroll_unit = gtk_event_controller_scroll_get_unit(controller);
    GtkAdjustment* vadjustment = gtk_scrollbar_get_adjustment(container->vscrollbar);

    if(scroll_unit == GDK_SCROLL_UNIT_WHEEL)
    {
        double step_increment = gtk_adjustment_get_step_increment(vadjustment);
        gtk_adjustment_set_value(vadjustment, gtk_adjustment_get_value(vadjustment)+step_increment*dy);
    }
    else if(scroll_unit == GDK_SCROLL_UNIT_SURFACE)
    {
        gtk_adjustment_set_value(vadjustment, gtk_adjustment_get_value(vadjustment)+dy);
    }
    return true;
}

static void
popover_listview_container_init(PopoverListviewContainer* container)
{
    GtkWidget* scrollbar = gtk_scrollbar_new(GTK_ORIENTATION_VERTICAL, NULL);
    container->vscrollbar = GTK_SCROLLBAR(scrollbar);
    gtk_widget_set_parent(scrollbar, GTK_WIDGET(container));

    GtkEventController* scroll_controller = gtk_event_controller_scroll_new(GTK_EVENT_CONTROLLER_SCROLL_VERTICAL);
    gtk_event_controller_set_propagation_phase(scroll_controller, GTK_PHASE_CAPTURE);
    g_signal_connect(scroll_controller, "scroll", G_CALLBACK(on_popover_listview_container_scroll), container);
    gtk_widget_add_controller(GTK_WIDGET(container), scroll_controller);
}

static void
popover_listview_container_dispose(GObject* object)
{
    PopoverListviewContainer* container = APP_POPOVER_LISTVIEW_CONTAINER(object);
    (void)container;
    G_OBJECT_CLASS(popover_listview_container_parent_class)->dispose(object);
}

static void
popover_listview_container_finalize(GObject* object)
{
    PopoverListviewContainer* container = APP_POPOVER_LISTVIEW_CONTAINER(object);
    (void)container;
    G_OBJECT_CLASS(popover_listview_container_parent_class)->dispose(object);
}

static void
popover_listview_container_set_property(GObject* object, guint property_id, const GValue* value, GParamSpec* pspec)
{
    orion::log_verbose("popover_listview_container_set_property: property_id=%u", property_id);
    PopoverListviewContainer* self = APP_POPOVER_LISTVIEW_CONTAINER(object);

    switch (property_id)
    {
        case PROP_MAX_HEIGHT:
            self->max_height = g_value_get_int64(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}


static void
popover_listview_container_get_property(GObject* object, guint prop_id, GValue* value, GParamSpec* pspec)
{
    PopoverListviewContainer* self = APP_POPOVER_LISTVIEW_CONTAINER(object);

    switch (prop_id)
    {
        case PROP_MAX_HEIGHT:
            g_value_set_int64(value, self->max_height);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}

static void
popover_listview_container_add_child(GtkBuildable* buildable, GtkBuilder* builder, GObject* child, const char* type)
{
    PopoverListviewContainer* self = APP_POPOVER_LISTVIEW_CONTAINER(buildable);
    if(self->child)
    {
        orion::log_error("popover_listview_container_add_child: attempted adding multiple children to container");
    }
    else if(GTK_IS_LIST_VIEW(child))
    {
        //TODO: hide/reduce scrollbar when unnecessary
        gtk_widget_set_parent(GTK_WIDGET(child), GTK_WIDGET(buildable));
        self->child = GTK_LIST_VIEW(child);
        gtk_scrollbar_set_adjustment(self->vscrollbar, gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(child)));
    }
}

static void
popover_listview_container_buildable_iface_init(GtkBuildableIface* iface)
{
    iface->add_child = popover_listview_container_add_child;
}

static void
popover_listview_container_class_init(PopoverListviewContainerClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = popover_listview_container_dispose;
    object_class->finalize = popover_listview_container_finalize;
    object_class->set_property = popover_listview_container_set_property;
    object_class->get_property = popover_listview_container_get_property;

    //NOTE: since we almost always want to constraint the height of an listview inside a popover,
    //this property is required to be set upon container creation
    obj_properties[PROP_MAX_HEIGHT] = g_param_spec_int64("max-height", NULL, NULL, -1, INT64_MAX, -1, GParamFlags(G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

    g_object_class_install_properties(object_class, N_PROPS, obj_properties);

    GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);

    gtk_widget_class_set_layout_manager_type(widget_class, APP_TYPE_POPOVER_LISTVIEW_LAYOUT_MANAGER);
}

struct _PopoverListviewLayoutManager {
    GtkLayoutManager parent;

    //bool has_measured;
};

G_DEFINE_TYPE(PopoverListviewLayoutManager, popover_listview_layout_manager, GTK_TYPE_LAYOUT_MANAGER)

static void
popover_listview_layout_manager_allocate(GtkLayoutManager* manager, GtkWidget* widget, int width, int height, int baseline)
{
    PopoverListviewContainer* custom_container = APP_POPOVER_LISTVIEW_CONTAINER(widget);
    if(custom_container->child)
    {
        orion::log_verbose("custom container got width %i height %i baseline %i", width, height, baseline);
        gtk_widget_allocate(GTK_WIDGET(custom_container->child), width, height, baseline, NULL);

        GtkWidget* scrollbar = GTK_WIDGET(custom_container->vscrollbar);
        int min, nat, min_baseline, nat_baseline;
        gtk_widget_measure(scrollbar, GTK_ORIENTATION_HORIZONTAL, -1, &min, &nat, &min_baseline, &nat_baseline);
        int scrollbar_width = CLAMP(nat, min, width);

        //translates the scrollbar all the way over to the right
        int x_offset = width - scrollbar_width;
        graphene_point_t scrollbar_origin = GRAPHENE_POINT_INIT((float)x_offset, 0.0f);
        GskTransform* scrollbar_offset_transform = gsk_transform_translate(NULL, &scrollbar_origin);

        gtk_widget_allocate(scrollbar, scrollbar_width, height, baseline, scrollbar_offset_transform);
    }
}

static void
popover_listview_layout_manager_measure(GtkLayoutManager* manager, GtkWidget* widget, GtkOrientation orientation, int for_size,
    int* minimum, int* natural, int* minimum_baseline, int* natural_baseline)
{
    // /PopoverListviewLayoutManager* custom_manager = APP_POPOVER_LISTVIEW_LAYOUT_MANAGER(manager);
    PopoverListviewContainer* custom_container = APP_POPOVER_LISTVIEW_CONTAINER(widget);
    if(custom_container->child)
    {
        //on the first attempt of a sequence, we should just request our full height, 
        gtk_widget_measure(GTK_WIDGET(custom_container->child), orientation, for_size, minimum, natural, minimum_baseline, natural_baseline);
        orion::log_verbose("list view measured for orientation %s for size %i: min %i nat %i min_baseline %i nat_baseline %i",
            orientation==GTK_ORIENTATION_HORIZONTAL?"horizontal":"vertical", for_size, *minimum, *natural, *minimum_baseline, *natural_baseline);
        if(orientation == GTK_ORIENTATION_VERTICAL)
        {
            *minimum = 0;
            if(custom_container->max_height)
            *natural = CLAMP(*natural, *minimum, custom_container->max_height);
        }
        // if(!custom_manager->has_measured)
        // {
        //     *natural = 450;
        //     custom_manager->has_measured = true;
        //     gtk_widget_queue_resize(widget);
        // }
    }
}

static void
popover_listview_layout_manager_init(PopoverListviewLayoutManager* self)
{
}

static void
popover_listview_layout_manager_class_init(PopoverListviewLayoutManagerClass* klass)
{
    GtkLayoutManagerClass* manager_class = GTK_LAYOUT_MANAGER_CLASS(klass);
    manager_class->allocate = popover_listview_layout_manager_allocate;
    manager_class->measure = popover_listview_layout_manager_measure;
}