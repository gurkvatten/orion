#include "sidebar_windows_list_view.h"
#include "window/window_list_box.h"
#include "data_item/window_item.h"

void
window_item_factory_setup(GtkSignalListItemFactory* factory, GtkListItem* list_item, gpointer user_data)
{
    GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_list_item_set_child(list_item, row);

    {
        // image icon
        GtkWidget* image = gtk_image_new();
        gtk_widget_set_halign(image, GTK_ALIGN_CENTER);
        gtk_widget_set_valign(image, GTK_ALIGN_CENTER);
        gtk_image_set_pixel_size(GTK_IMAGE(image), 16);
        gtk_box_append(GTK_BOX(row), image);
    }
    {
        // label icon (emoji)
        GtkWidget* label = gtk_label_new(NULL);
        gtk_widget_set_halign(GTK_WIDGET(label), GTK_ALIGN_CENTER);
        gtk_widget_set_valign(GTK_WIDGET(label), GTK_ALIGN_CENTER);
        gtk_box_append(GTK_BOX(row), label);
    }
    {
        // gesture click for right-click context menu
        GtkGesture* click = gtk_gesture_click_new();
        gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(click), GDK_BUTTON_SECONDARY);
        gtk_widget_add_controller(row, GTK_EVENT_CONTROLLER(click));
        g_signal_connect(click, "released", G_CALLBACK(on_windows_list_view_row_secondary_released), list_item);
    }
    
    {
        // popover menu
        // must be last child of the GtkBox root (referenced in code using gtk_widget_get_last_child)
        GtkWidget* popover_menu = gtk_popover_menu_new_from_model(NULL);
        gtk_box_append(GTK_BOX(row), popover_menu);
    }
}

void
window_item_factory_bind(GtkSignalListItemFactory* factory, GtkListItem* list_item, gpointer user_data)
{
    GtkWidget* row = gtk_list_item_get_child(list_item);
    WindowItem* window_item = WINDOW_ITEM(gtk_list_item_get_item(list_item));
    gtk_widget_set_tooltip_text(row, window_item_get_title(window_item));

    bool is_image_icon = window_item_get_is_image_icon(window_item);

    GtkWidget* image_icon = gtk_widget_get_first_child(row);
    gtk_image_set_from_icon_name(GTK_IMAGE(image_icon), window_item_get_icon_name(window_item));
    gtk_widget_set_visible(image_icon, is_image_icon);
    if (window_item_get_is_suspended(window_item)) {
        gtk_widget_add_css_class(image_icon, "dim-icon");
    }

    GtkWidget* label_icon = gtk_widget_get_next_sibling(image_icon);
    gtk_label_set_text(GTK_LABEL(label_icon), window_item_get_icon_name(window_item));
    gtk_widget_set_visible(label_icon, !is_image_icon);
    if (window_item_get_is_suspended(window_item)) {
        gtk_widget_add_css_class(label_icon, "dim-icon");
    }
}