#include "reading_list_toggle_group.h"

struct _ReadingListToggleGroup {
    GtkBox parent;
    AdwToggleGroup* toggle_group;
};

G_DEFINE_TYPE(ReadingListToggleGroup, reading_list_toggle_group, GTK_TYPE_BOX)

AdwToggleGroup*
reading_list_toggle_group_get_unread_toggle_group(ReadingListToggleGroup *self)
{
    return self->toggle_group;
}

static void
reading_list_toggle_group_dispose(GObject* gobject)
{
    gtk_widget_dispose_template(GTK_WIDGET(gobject), READING_LIST_TOGGLE_GROUP_TYPE);
    G_OBJECT_CLASS(reading_list_toggle_group_parent_class)->dispose(gobject);
}

static void
reading_list_toggle_group_init(ReadingListToggleGroup* self)
{
    gtk_widget_init_template(GTK_WIDGET(self));
}

static void
reading_list_toggle_group_class_init(ReadingListToggleGroupClass* klass)
{
    G_OBJECT_CLASS(klass)->dispose = reading_list_toggle_group_dispose;
    GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);
    gtk_widget_class_set_template_from_resource(widget_class, GRESOURCE_PREFIX "/src/window/main_window/sidebar/reading_list_toggle_group.ui");

    gtk_widget_class_bind_template_child(widget_class, ReadingListToggleGroup, toggle_group);
    
}
