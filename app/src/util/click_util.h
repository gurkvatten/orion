#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>

#define ORION_CONTEXT_MENU_CONTROLLER_NAME "orion_context_menu_controller"

namespace orion::gtk {

struct ReleasedCallbackData
{
    GtkWidget* parent;
    gpointer child; // for example GtkColumnViewCell*, GtkWidget*, or whatever is needed
    GtkPopoverMenu* popover_menu;
    bool unparent_popover_menu_on_release{};
    gpointer user_data;
};

typedef void (*ReleasedCallback)(GtkWidget* widget, gint n_press, gdouble x, gdouble y, GtkGestureClick* click);
typedef void (*ReleasedCallbackWithUserData)(GtkGestureClick* click, gint n_press, gdouble x, gdouble y, gpointer user_data);

void button_pressed_default_handler_with_user_data(GtkGestureClick* click, gint n_press, gdouble x, gdouble y, gpointer user_data);
void register_right_click(GtkWidget* parent, GtkWidget* widget, ReleasedCallback cb);
void register_right_click_with_user_data(GtkWidget* parent, GtkWidget* widget, ReleasedCallbackWithUserData cb, gpointer user_data);
void register_right_click_with_user_data_and_closure(
    GtkWidget* parent, GtkWidget* widget, ReleasedCallbackWithUserData cb, gpointer user_data, GClosureNotify closure
);
void register_left_click(GtkWidget* parent, GtkWidget* widget, ReleasedCallback cb);
void register_left_and_right_click(GtkWidget* parent, GtkWidget* widget, ReleasedCallback cb);
void register_left_long_click(GtkWidget* parent, GtkWidget* widget, ReleasedCallback cb);
void button_released_handler(
    GtkWidget* parent, GtkGestureClick* click, gdouble x, gdouble y, GtkPopoverMenu* popover_menu, GtkWidget* widget
);
void button_released_handler_manual_pos(GtkGestureClick* click, GtkPopoverMenu* popover_menu, GtkWidget* widget);
void setup_context_menu(GtkBuilder* builder, GtkWidget* parent, GtkPopoverMenu** popover_out, const char* object_name);
void setup_context_popover_menu(GtkBuilder* builder, GtkWidget* parent, GtkPopoverMenu** popover_out, const char* object_name);
void remove_click_controller(GtkWidget* widget);
void remove_multiple_click_controllers(GtkWidget* widget);
void remove_dnd_controllers(GtkWidget* widget);

//This function should not be relied on as it will most likely break in the future
//From what I have found, this is the most reasonable method to get a row widget
//needed to apply a context menu to a whole columnview row without any non reactive spaces
GtkWidget* widget_get_column_view_row_widget(GtkWidget* widget);

// Returns default initialized rectangle on failure
GdkRectangle compute_pointing_rect(GtkWidget* source_widget, GtkWidget* target_widget, double x, double y);

} // namespace orion::gtk