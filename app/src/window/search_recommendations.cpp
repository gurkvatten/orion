#include <gtk/gtk.h>
#include <common/src/defer.h>

#include "app.h"
#include "search_recommendations.h"
#include "start_page_settings_popover.h"
#include "util/click_util.h"
#include "util/variant_util.h"
#include "window/main_window/orion_window_iface.h"

struct _SearchRecommendations
{
	GtkBox parent;
	GtkPopover* show_preferences_popover;
    GtkPopoverMenu* context_menu;
    GtkBox* overlay_box;
    GtkBox* private_browsing_info_box;
    GtkLabel* beta_disclaimer_label;
};

G_DEFINE_TYPE(SearchRecommendations, search_recommendations, GTK_TYPE_BOX)

enum {
    LINK_CLICKED_SIGNAL,
    N_SIGNALS
};

static guint signals[N_SIGNALS] = {};

static void
search_recommendations_dispose(GObject* gobject)
{
    gtk_widget_dispose_template(GTK_WIDGET(gobject), SEARCH_RECOMMENDATIONS_TYPE);
    G_OBJECT_CLASS(search_recommendations_parent_class)->dispose(gobject);
}

static void
on_search_recommendations_secondary_released(GtkWidget* widget, gint n_press, gdouble x, gdouble y, GtkGestureClick* click)
{
    SearchRecommendations* recs = SEARCH_RECOMMENDATIONS(widget);
	orion::gtk::button_released_handler(widget, click, x, y, recs->context_menu, widget);
}

SearchRecommendations*
search_recommendations_new()
{
	return SEARCH_RECOMMENDATIONS(g_object_new(SEARCH_RECOMMENDATIONS_TYPE, NULL));
}

static void
search_recommendations_init(SearchRecommendations* self)
{
	gtk_widget_init_template(GTK_WIDGET(self));

    gtk_widget_set_visible(GTK_WIDGET(self->beta_disclaimer_label), APPLICATION_IS_BETA);
}

static void
on_show_preferences_popover_button_clicked(GtkButton* button, SearchRecommendations* sr)
{

	if (sr->show_preferences_popover == NULL) {
		sr->show_preferences_popover = start_page_settings_popover_new();
		gtk_widget_set_parent(GTK_WIDGET(sr->show_preferences_popover), GTK_WIDGET(sr));
	}
	graphene_point_t zero{};
	graphene_point_t pos{};
	if (!gtk_widget_compute_point(GTK_WIDGET(button), GTK_WIDGET(sr), &zero, &pos)) {
        orion::log_warning("failed to compute position for search recommendations preferences popover");
	}

	// Point the popover arrow to the button's bottom (vertically) center (horizontally)
	GdkRectangle rect{
		.x = int(pos.x + gtk_widget_get_width(GTK_WIDGET(button)) / 2),
		.y = int(pos.y),
		.width = 1,
		.height = 1,
	};
	gtk_popover_set_pointing_to(sr->show_preferences_popover, &rect);
	gtk_popover_set_has_arrow(sr->show_preferences_popover, TRUE);
	gtk_popover_popup(sr->show_preferences_popover);


}

void
search_recommendations_hide_overlay(SearchRecommendations *self)
{
   gtk_widget_set_visible(GTK_WIDGET(self->overlay_box), false);
}

void
on_test_web_button_1_clicked(GtkButton* self, gpointer user_data)
{
    OrionWindow* win = ORION_WINDOW(gtk_widget_get_root(GTK_WIDGET(self)));
    SearchRecommendations* sr = SEARCH_RECOMMENDATIONS(user_data);

    orion::gtk::VariantWrapper target("https://orionfeedback.org/");

    g_action_group_activate_action(
        G_ACTION_GROUP(win),
        "open-uri-in-current-tab",
        target.get()
    );

    g_signal_emit(sr, signals[LINK_CLICKED_SIGNAL], 0);
}

void
on_test_web_button_2_clicked(GtkButton* self, gpointer user_data)
{
    OrionWindow* win = ORION_WINDOW(gtk_widget_get_root(GTK_WIDGET(self)));
    SearchRecommendations* sr = SEARCH_RECOMMENDATIONS(user_data);

    orion::gtk::VariantWrapper target("https://help.kagi.com/orion/");

    g_action_group_activate_action(
        G_ACTION_GROUP(win),
        "open-uri-in-current-tab",
        target.get()
    );

    g_signal_emit(sr, signals[LINK_CLICKED_SIGNAL], 0);
}

static void
search_recommendations_root(GtkWidget* widget)
{
    GTK_WIDGET_CLASS(search_recommendations_parent_class)->root(widget); // chain up first

    GtkRoot* root = gtk_widget_get_root(widget);
    if (!root) {
        orion::log_error("search_recommendations_root: failed to get root for widget %p", widget);
        return;
    }
    if (!ORION_IS_WINDOW(root)) {
        orion::log_error("search_recommendations_root: root is not OrionWindow");
        return;
    }

    OrionWindow* win = ORION_WINDOW(root);
    orion::WindowId window_id = orion_window_get_window_id(win);
    orion::gtk::WeakRefWrapper self_weak = SEARCH_RECOMMENDATIONS(widget);

    global_core_api->get_windows({window_id}, [window_id,self_weak](orion::GetWindowsReply reply){
        VERIFY_LOG(reply.err);
        if (reply.windows.empty()) {
            orion::log_error("search_recommendations_root: window with id %lld not found", window_id.value);
            return;
        }
        const auto& window = reply.windows.at(0);

        auto self_strong = self_weak.lock();
        if (!self_strong) return;
        SearchRecommendations* self = SEARCH_RECOMMENDATIONS(self_strong.get());

        gtk_widget_set_visible(GTK_WIDGET(self->private_browsing_info_box), window.is_private);
    });
}

static void
search_recommendations_class_init(SearchRecommendationsClass* klass)
{
    G_OBJECT_CLASS(klass)->dispose = search_recommendations_dispose;

    GTK_WIDGET_CLASS(klass)->root = search_recommendations_root;

	gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass), GRESOURCE_PREFIX "/src/window/search_recommendations.ui");

    gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), on_show_preferences_popover_button_clicked);
    gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), on_search_recommendations_secondary_released);
    gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), on_test_web_button_1_clicked);
    gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), on_test_web_button_2_clicked);

    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), SearchRecommendations, overlay_box);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), SearchRecommendations, context_menu);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), SearchRecommendations, private_browsing_info_box);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), SearchRecommendations, beta_disclaimer_label);

    signals[LINK_CLICKED_SIGNAL] = g_signal_new(
        "link-clicked",
        G_TYPE_FROM_CLASS(klass),
        G_SIGNAL_RUN_FIRST,
        0,
        nullptr,
        nullptr,
        nullptr,
        G_TYPE_NONE,
        0 // n_params
    );
}
