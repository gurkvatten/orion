#include <gtk/gtk.h>

#include "app.h"
#include "search_popover.h"
#include "search_recommendations.h"

struct _SearchPopover
{
	GtkPopover parent;
	SearchRecommendations* search_recommendations_box;
};

G_DEFINE_TYPE(SearchPopover, search_popover, GTK_TYPE_POPOVER)

GtkPopover*
search_popover_new()
{
	return GTK_POPOVER(g_object_new(SEARCH_POPOVER_TYPE, NULL));
}

static void
on_search_recommendations_link_clicked(SearchRecommendations* sr, gpointer user_data)
{
    SearchPopover* sp = SEARCH_POPOVER(user_data);
    gtk_popover_popdown(GTK_POPOVER(sp));
}

static void
search_popover_init(SearchPopover* self)
{
    g_type_ensure(SEARCH_RECOMMENDATIONS_TYPE);
    
	gtk_widget_init_template(GTK_WIDGET(self));
    search_recommendations_hide_overlay(self->search_recommendations_box);
    
    g_signal_connect(
        self->search_recommendations_box,
        "link-clicked",
        G_CALLBACK(on_search_recommendations_link_clicked),
        self
    );
}

static void
search_popover_class_init(SearchPopoverClass* klass) //TODO: add emojichooser popover to allow for
{
	gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass), GRESOURCE_PREFIX "/src/window/search_popover.ui");
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), SearchPopover, search_recommendations_box);
}
