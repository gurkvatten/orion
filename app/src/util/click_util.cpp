#include "click_util.h"

#include <common/src/log.h>
#include <vector>
#include <functional>
#include <cstring>
#include <common/src/defer.h>

#include "window/main_window/main_window.h"

namespace orion::gtk {

//TODO: fix context menus
//replicate bug by doing this over a widget with a context menu:
//starting with a popover open, press and hold lmb, press and hold rmb, release lmb, release rmb
//this will spam warnings in console and also break mouse clicks entirely when done over webkit

static void
button_pressed_default_handler(GtkGestureClick* click, gint n_press, gdouble x, gdouble y, gpointer user_data)
{
	//orion::log_debug("button_pressed_default_handler: %p", user_data);
	// Claiming is crucial, otherwise no more than a single right-click event will be triggered
	gtk_gesture_set_state(GTK_GESTURE(user_data), GTK_EVENT_SEQUENCE_CLAIMED);
}

void
button_pressed_default_handler_with_user_data(GtkGestureClick* click, gint n_press, gdouble x, gdouble y, gpointer user_data)
{
	// Claiming is crucial, otherwise no more than a single right-click event will be triggered
	gtk_gesture_set_state(GTK_GESTURE(click), GTK_EVENT_SEQUENCE_CLAIMED);
}

void
register_right_click(GtkWidget* parent, GtkWidget* widget, ReleasedCallback cb)
{
    if (!widget) {
        orion::log_warning("register_right_click: widget is NULL");
        return;
    }

	GtkGesture* click_gesture = gtk_gesture_click_new();
	gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(click_gesture), GDK_BUTTON_SECONDARY); // right-click mouse button
    gtk_gesture_single_set_exclusive(GTK_GESTURE_SINGLE(click_gesture), TRUE);

	// The "pressed" action has to be connected and handled (and claimed using GTK_EVENT_SEQUENCE_CLAIMED)
	// otherwise no more than a single right-click event will be triggered
	g_signal_connect_object(click_gesture, "pressed", G_CALLBACK(button_pressed_default_handler), parent, G_CONNECT_SWAPPED);

	g_signal_connect_object(click_gesture, "released", G_CALLBACK(cb), parent, G_CONNECT_SWAPPED);
	gtk_widget_add_controller(widget, GTK_EVENT_CONTROLLER(click_gesture));
}

void
register_right_click_with_user_data(GtkWidget* parent, GtkWidget* widget, ReleasedCallbackWithUserData cb, gpointer user_data)
{
	GtkGesture* click_gesture = gtk_gesture_click_new();
    gtk_event_controller_set_name(GTK_EVENT_CONTROLLER(click_gesture), ORION_CONTEXT_MENU_CONTROLLER_NAME);
	gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(click_gesture), GDK_BUTTON_SECONDARY); // right-click mouse button
    gtk_gesture_single_set_exclusive(GTK_GESTURE_SINGLE(click_gesture), true);

	// The "pressed" action has to be connected and handled (and claimed using GTK_EVENT_SEQUENCE_CLAIMED)
	// otherwise no more than a single right-click event will be triggered
    g_signal_connect(click_gesture, "pressed", G_CALLBACK(button_pressed_default_handler_with_user_data), user_data);

    g_signal_connect(click_gesture, "released", G_CALLBACK(cb), user_data);
	gtk_widget_add_controller(widget, GTK_EVENT_CONTROLLER(click_gesture));
}

void
register_right_click_with_user_data_and_closure(
    GtkWidget* parent, GtkWidget* widget, ReleasedCallbackWithUserData cb, gpointer user_data, GClosureNotify closure)
{
	GtkGesture* click_gesture = gtk_gesture_click_new();
    gtk_event_controller_set_name(GTK_EVENT_CONTROLLER(click_gesture), ORION_CONTEXT_MENU_CONTROLLER_NAME);
	gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(click_gesture), GDK_BUTTON_SECONDARY); // right-click mouse button
    gtk_gesture_single_set_exclusive(GTK_GESTURE_SINGLE(click_gesture), true);

	// The "pressed" action has to be connected and handled (and claimed using GTK_EVENT_SEQUENCE_CLAIMED)
	// otherwise no more than a single right-click event will be triggered
    g_signal_connect(click_gesture, "pressed", G_CALLBACK(button_pressed_default_handler_with_user_data), NULL);

    g_signal_connect_data(click_gesture, "released", G_CALLBACK(cb), user_data, closure, G_CONNECT_DEFAULT);
	gtk_widget_add_controller(widget, GTK_EVENT_CONTROLLER(click_gesture));
}

void
register_left_click(GtkWidget* parent, GtkWidget* widget, ReleasedCallback cb)
{
	auto ids = std::vector<gint>{GDK_BUTTON_PRIMARY};

	for (auto id : ids) {
		GtkGesture* click_gesture = gtk_gesture_click_new();
        gtk_event_controller_set_name(GTK_EVENT_CONTROLLER(click_gesture), ORION_CONTEXT_MENU_CONTROLLER_NAME);
		gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(click_gesture), id);

		// The "pressed" action has to be connected and handled (and claimed using GTK_EVENT_SEQUENCE_CLAIMED)
		// otherwise no more than a single right-click event will be triggered
		g_signal_connect_object(click_gesture, "pressed", G_CALLBACK(button_pressed_default_handler), parent, G_CONNECT_SWAPPED);

		g_signal_connect_object(click_gesture, "released", G_CALLBACK(cb), parent, G_CONNECT_SWAPPED);
		gtk_widget_add_controller(widget, GTK_EVENT_CONTROLLER(click_gesture));
	}
}

void
register_left_and_right_click(GtkWidget* parent, GtkWidget* widget, ReleasedCallback cb)
{
	auto ids = std::vector<gint>{GDK_BUTTON_PRIMARY, GDK_BUTTON_SECONDARY};

	for (auto id : ids) {
		GtkGesture* click_gesture = gtk_gesture_click_new();
        gtk_event_controller_set_name(GTK_EVENT_CONTROLLER(click_gesture), ORION_CONTEXT_MENU_CONTROLLER_NAME);
		gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(click_gesture), id);

		// The "pressed" action has to be connected and handled (and claimed using GTK_EVENT_SEQUENCE_CLAIMED)
		// otherwise no more than a single right-click event will be triggered
		g_signal_connect_object(click_gesture, "pressed", G_CALLBACK(button_pressed_default_handler), parent, G_CONNECT_SWAPPED);

		g_signal_connect_object(click_gesture, "released", G_CALLBACK(cb), parent, G_CONNECT_SWAPPED);
		gtk_widget_add_controller(widget, GTK_EVENT_CONTROLLER(click_gesture));
	}
}

void
register_left_long_click(GtkWidget* parent, GtkWidget* widget, ReleasedCallback cb)
{
    GtkGesture* gesture = gtk_gesture_long_press_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(gesture), GDK_BUTTON_PRIMARY); // left-click mouse button
    gtk_gesture_single_set_exclusive(GTK_GESTURE_SINGLE(gesture), TRUE);

	// The "pressed" action has to be connected and handled (and claimed using GTK_EVENT_SEQUENCE_CLAIMED)
	// otherwise no more than a single click event will be triggered
	g_signal_connect_object(gesture, "pressed", G_CALLBACK(cb), parent, G_CONNECT_SWAPPED);

	gtk_widget_add_controller(widget, GTK_EVENT_CONTROLLER(gesture));
}

void
remove_click_controller(GtkWidget* widget)
{
    GListModel* controllers = gtk_widget_observe_controllers(widget);
    defer([controllers]{ g_object_unref(controllers); });
    uint n_items = g_list_model_get_n_items(controllers);
    for(uint i = 0; i<n_items; i++)
    {
        GtkEventController* controller = GTK_EVENT_CONTROLLER(g_list_model_get_item(controllers, i));
        defer( [controller]{g_object_unref(controller); });
        if(gtk_event_controller_get_name(controller))
        {
            if(GTK_IS_GESTURE_CLICK(controller) && strcmp(ORION_CONTEXT_MENU_CONTROLLER_NAME, gtk_event_controller_get_name(controller)) == 0)
            {
                gtk_widget_remove_controller(widget, controller);
                break;
            }
        }
    }
}

void
remove_multiple_click_controllers(GtkWidget* widget)
{
    gboolean flag = true;
    while(flag)
    {
        //not pretty code but gtk_widget_observe_controllers gives no guarantees so I have to reinit every time I want to delete another one
        GListModel* controllers = gtk_widget_observe_controllers(widget);
        defer( [controllers]{ g_object_unref(controllers); });
        uint n_items = g_list_model_get_n_items(controllers);
        flag = false;
        for(uint i = 0; i<n_items; i++)
        {
            GtkEventController* controller = GTK_EVENT_CONTROLLER(g_list_model_get_item(controllers, i));
            defer( [controller]{g_object_unref(controller); });
            if(gtk_event_controller_get_name(controller))
            {
                if(GTK_IS_GESTURE_CLICK(controller) && strcmp(ORION_CONTEXT_MENU_CONTROLLER_NAME, gtk_event_controller_get_name(controller)) == 0)
                {
                    gtk_widget_remove_controller(widget, controller);
                    flag = true;
                    break;
                }
            }
        }
    }
}

void
remove_dnd_controllers(GtkWidget* widget)
{
    GListModel* controllers = gtk_widget_observe_controllers(widget);
    defer([controllers]{ g_object_unref(controllers); });
    uint n_items = g_list_model_get_n_items(controllers);
    for(uint i = n_items - 1; ; i--)
    {
        GtkEventController* controller = GTK_EVENT_CONTROLLER(g_list_model_get_item(controllers, i));
        defer( [controller]{g_object_unref(controller); });
        if(GTK_IS_DRAG_SOURCE(controller) || GTK_IS_DROP_TARGET(controller))
        {
            gtk_widget_remove_controller(widget, controller);
        }

        if(i==0) break;
    }
}

typedef void (*ButtonReleasedHandler)(
    GtkWidget* parent, GtkGestureClick* click, gdouble x, gdouble y, GtkPopoverMenu* popover_menu, GtkWidget* widget
);

void
button_released_handler(
    GtkWidget* parent,
    GtkGestureClick* click,
    gdouble x,
    gdouble y,
    GtkPopoverMenu* popover_menu,
    GtkWidget* widget)
{
    if (!popover_menu) {
        orion::log_warning("button_released_handler: popover_menu is null");
        return;
    }

	// Display the popup menu here in "released" callback - in "pressed" callback, GTK would
	// generate "Broken accounting for active state" warnings
	//orion::log_debug("button_released_handler: %p", parent);

	graphene_point_t zero{};
	graphene_point_t pos{};
	if (!gtk_widget_compute_point(widget, parent, &zero, &pos)) {
		orion::log_warning("button_released_handler: failed to compute point");
	}

	// Position of the widget + position of the click
	GdkRectangle rect{
		.x = int(pos.x + x),
		.y = int(pos.y + y),
		.width = 1,
		.height = 1,
	};
	gtk_popover_set_pointing_to(GTK_POPOVER(popover_menu), &rect);
	gtk_popover_set_has_arrow(GTK_POPOVER(popover_menu), FALSE);
	gtk_popover_popup(GTK_POPOVER(popover_menu));

	// Claiming is crucial, otherwise no more than a single right-click event will be triggered
	gtk_gesture_set_state(GTK_GESTURE(click), GTK_EVENT_SEQUENCE_CLAIMED);
    main_window_set_last_right_clicked_widget(MAIN_WINDOW(gtk_widget_get_root(widget)),GTK_WIDGET(widget));
}

void
button_released_handler_manual_pos(GtkGestureClick* click, GtkPopoverMenu* popover_menu, GtkWidget* widget)
{
	gtk_popover_set_has_arrow(GTK_POPOVER(popover_menu), FALSE);
	gtk_popover_popup(GTK_POPOVER(popover_menu));

	// Claiming is crucial, otherwise no more than a single right-click event will be triggered
	gtk_gesture_set_state(GTK_GESTURE(click), GTK_EVENT_SEQUENCE_CLAIMED);
    main_window_set_last_right_clicked_widget(MAIN_WINDOW(gtk_widget_get_root(widget)),GTK_WIDGET(widget));
}

void
setup_context_menu(GtkBuilder* builder, GtkWidget* parent, GtkPopoverMenu** popover_out, const char* object_name)
{
	GMenuModel* context_menu = G_MENU_MODEL(gtk_builder_get_object(builder, object_name));
	GtkWidget* popover = gtk_popover_menu_new_from_model_full(context_menu, GTK_POPOVER_MENU_NESTED);
	gtk_widget_set_parent(popover, parent);
	*popover_out = GTK_POPOVER_MENU(popover);
}

void
setup_context_popover_menu(GtkBuilder* builder, GtkWidget* parent, GtkPopoverMenu** popover_out, const char* object_name)
{
	GtkWidget* popover = GTK_WIDGET(gtk_builder_get_object(builder, object_name));
	gtk_widget_set_parent(popover, parent);
	*popover_out = GTK_POPOVER_MENU(popover);
}

//This function should not be relied on as it will most likely break in the future
//From what I have found, this is the most reasonable method to get a row widget
//needed to apply a context menu to a whole columnview row, avoiding any non interactive spaces
GtkWidget*
widget_get_column_view_row_widget(GtkWidget* widget)
{
    if (!widget) {
        orion::log_error("widget_get_column_view_row_widget: null widget");
        return NULL;
    }

    for(int i = 0; (i<20)&&(strcmp(G_OBJECT_TYPE_NAME(widget),"GtkColumnViewRowWidget")!=0);i++)
    {
        widget = gtk_widget_get_parent(GTK_WIDGET(widget));
        if (!widget) {
            orion::log_error("widget_get_column_view_row_widget: could not find column view row");
            return NULL;
        }
    }
    if(strcmp(G_OBJECT_TYPE_NAME(widget),"GtkColumnViewRowWidget")==0)
    {
        return widget;
    }
    else
    {
        orion::log_error("widget_get_column_view_row_widget: could not find column view row, likely removed in this gtk version");
        return NULL;
    }
}

GdkRectangle
compute_pointing_rect(GtkWidget* source_widget, GtkWidget* target_widget, double x, double y)
{
    graphene_point_t zero{};
    graphene_point_t pos{};
    if (!gtk_widget_compute_point(source_widget, target_widget, &zero, &pos)) {
        orion::log_warning("compute_pointing_rect: failed to compute point");
        return GdkRectangle{};
    }

    GdkRectangle rect{
        .x = int(pos.x + x),
        .y = int(pos.y + y),
        .width = 1,
        .height = 1,
    };
    return rect;
}

} // namespace orion::gtk