#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include <vector>

// Update: This macro does not work in flatpak-webkit-release build as of 2026-06-23 in the current form,
// compilation fails with error: "dereferencing type-punned pointer will break strict-aliasing rules".
// Use simple g_clear_pointer(&ptr, orion::gtk::widget_unparent_wrapper) instead of this macro.
//
// Avoids warning (error) in release builds about breaking strict-aliasing rules
//edited to unset the original pointer, requires ptr to be an lvalue this unsets,
//fails to expose this in the code pattern but done so to maintain same syntax
#define CLEAR_GTK_WIDGET_PTR_DO_NOT_USE(ptr, destroy) \
do { \
    using ptr_type__ = std::remove_reference_t<decltype(ptr)>; \
    static_assert(std::is_pointer_v<ptr_type__> && \
                  !std::is_pointer_v<std::remove_pointer_t<ptr_type__>>, \
                  "CLEAR_GTK_WIDGET_PTR_DO_NOT_USE expects single depth pointer (e.g. GtkWidget*), clears it in its original storage");\
    g_clear_pointer((GtkWidget**)&ptr, destroy); \
} while(0)

#undef CLEAR_GTK_WIDGET_PTR_DO_NOT_USE
#define CLEAR_GTK_WIDGET_PTR_DO_NOT_USE(ptr, destroy) \
do { \
    static_assert(false, "CLEAR_GTK_WIDGET_PTR_DO_NOT_USE is broken in flatpak builds") \
} while(0)


namespace orion::gtk {

// Returns NULL if the parent window could not be found. This function is useful
// to determine the parent of AdwDialog instance.
GtkWindow* get_parent_window(GtkWidget* widget);

// Returns NULL if the parent AdwDialog could not be found. This function is useful
// to determine the parent of a widget within an AdwDialog.
AdwDialog* get_parent_adw_dialog(GtkWidget* widget);

GtkWidget* get_child_by_name(GtkWidget* parent, const char* name);
std::vector<GtkWidget*> get_descendants_by_name(GtkWidget* root, const char* type_name);

// Appends all descendants to the out_result vector. The out_result vector is not cleared before appending.
void get_descendants(GtkWidget* root, std::vector<GtkWidget*>* out_result);
gboolean is_widget_at_position(GtkWidget* widget, double x, double y, GtkWidget* origin);

int get_children_count(GtkWidget* parent);

// Returns the (horizontal) index of the given AdwTab in the AdwTabBar based on x coordinates.
// Returns -1 if the tab is not found in the tab bar.
int get_adw_tab_index(GtkWidget* adw_tab, GtkWidget* root);

// Returns NULL if no matching page is found.
AdwTabPage* get_adw_tab_page_by_adw_tab(GtkWidget* adw_tab, AdwTabBar* tab_bar);

bool is_widget_safe_for_popover_parent(GtkWidget* widget);

// Wrapper for g_clear_pointer with gtk_widget_unparent as the destroy function
void widget_unparent_wrapper(gpointer ptr);

void remove_all_children_from_box(GtkBox* box);
// Appends to the end of the target box
void move_child_from_box_to_box(GtkBox* source, GtkWidget* child, GtkBox* target);
// Prepends to the start of the target box
void move_child_from_box_to_box_start(GtkBox* source, GtkWidget* child, GtkBox* target);
// Moves the child to the target box after the specified sibling widget
void move_child_from_box_to_box_after_sibling(GtkBox* source, GtkWidget* child, GtkBox* target, GtkWidget* sibling);
// Moves the child to the target box before the specified sibling widget
void move_child_from_box_to_box_before_sibling(GtkBox* source, GtkWidget* child, GtkBox* target, GtkWidget* sibling);

void move_all_children_from_box_to_box(GtkBox* source, GtkBox* target);

} // namespace orion::gtk