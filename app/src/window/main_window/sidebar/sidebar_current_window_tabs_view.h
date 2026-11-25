#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "app.h"
#include "window/main_window/sidebar/sidebar.h"

struct WindowIdChangedData
{
    GtkListView* tabs_list_view;
    GtkLabel* tabs_label;
    GtkGridView* pinned_tabs_grid_view;
};

void sidebar_tabs_list_view_setup_tabs_lists(Sidebar* self, OrionWebviewTabView* orion_webview_tab_view);
void sidebar_tabs_list_view_map(GtkListView* view, gpointer user_data);
void on_tab_expander_notify_expanded(GtkExpander* expander, GParamSpec* pspec, gpointer user_data);
void on_tab_expander_notify_root(GtkExpander* expander, GParamSpec* pspec, gpointer user_data);
void on_tab_favicon_notify_root(GtkImage* expander, GParamSpec* pspec, gpointer user_data);
void on_tab_title_notify_root(GtkLabel* label, GParamSpec* pspec, gpointer user_data);
bool get_tab_expander_expanded(GtkListItem* item);
bool get_tab_icon_visible(GtkListItem* item);
void on_close_page_button_clicked(GtkButton* self, gpointer user_data);
void on_sidebar_tab_audio_button_clicked(GtkButton* self, gpointer user_data);
GdkContentProvider* on_pinned_tab_drag_source_prepare(GtkDragSource* self, gdouble x, gdouble y, gpointer user_data);
void on_pinned_tab_drag_source_drag_begin(GtkDragSource* self, GdkDrag* drag, gpointer user_data);
void on_pinned_tab_drag_source_drag_end(GtkDragSource* self, GdkDrag* drag, gpointer user_data);
gboolean on_pinned_tab_drop_target_drop(GtkDropTarget* self, const GValue* val, gdouble x, gdouble y, gpointer user_data);
GdkContentProvider* on_unpinned_tab_drag_source_prepare(GtkDragSource* self, gdouble x, gdouble y, gpointer user_data);
void on_unpinned_tab_drag_source_drag_begin(GtkDragSource* self, GdkDrag* drag, gpointer user_data);
void on_unpinned_tab_drag_source_drag_end(GtkDragSource* self, GdkDrag* drag, gpointer user_data);
gboolean on_unpinned_tab_drop_target_drop(GtkDropTarget* self, const GValue* val, gdouble x, gdouble y, gpointer user_data);
GdkDragAction on_tab_drop_target_motion(GtkDropTarget* self, gdouble x, gdouble y, gpointer user_data);
GdkDragAction on_tab_drop_target_leave(GtkDropTarget* self, gdouble x, gdouble y, gpointer user_data);
gboolean on_tabs_list_view_drop_target_drop(GtkDropTarget* self, const GValue* val, gdouble x, gdouble y, gpointer user_data);
GdkDragAction on_tabs_list_view_drop_target_motion(GtkDropTarget* self, gdouble x, gdouble y, gpointer user_data);
GdkDragAction on_tabs_list_view_drop_target_leave(GtkDropTarget* self, gdouble x, gdouble y, gpointer user_data);
void on_tab_event_controller_motion_enter(GtkListItem* item);
void on_tabs_list_view_secondary_released(Sidebar* sidebar, gint n_press, gdouble x, gdouble y, GtkGestureClick* click);
void on_new_window_button_clicked(GtkButton* button, gpointer user_data);
void on_tabs_list_view_row_secondary_pressed(GtkGestureClick* gesture, gint n_press, gdouble x, gdouble y, gpointer user_data);
void on_tabs_list_view_row_secondary_released(GtkGestureClick* gesture, gint n_press, gdouble x, gdouble y, gpointer user_data);
void on_tabs_list_view_row_middle_released(GtkGestureClick* gesture, gint n_press, gdouble x, gdouble y, gpointer user_data);
void on_new_tab_button_secondary_pressed(GtkGestureClick* click, gint n_press, gdouble x, gdouble y, gpointer user_data);
void on_new_tab_button_secondary_released(GtkGestureClick* click, gint n_press, gdouble x, gdouble y, gpointer user_data);
void on_pinned_tabs_list_view_item_secondary_pressed(GtkGestureClick* gesture, gint n_press, gdouble x, gdouble y, gpointer user_data);
void on_tab_state_flags_changed(GtkPopover* tab_preview, GtkStateFlags old_flags, GtkWidget* tab_box);
