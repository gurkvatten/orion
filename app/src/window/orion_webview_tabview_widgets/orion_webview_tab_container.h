#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "app.h"

#include "orion_webview_tab_content.h"

#define ORION_WEBVIEW_TAB_CONTAINER_TYPE (orion_webview_tab_container_get_type())

G_DECLARE_FINAL_TYPE(OrionWebviewTabContainer, orion_webview_tab_container, ORION, WEBVIEW_TAB_CONTAINER, GtkWidget)

void orion_webview_tab_container_set_child_content(OrionWebviewTabContainer* container, OrionWebviewTabContent* content);
//can return NULL
OrionWebviewTabContent* orion_webview_tab_container_get_content(OrionWebviewTabContainer* container);
OrionWebviewTabContainer* orion_webview_tab_container_new(orion::TabId id, double seq_num);

orion::TabId orion_webview_tab_container_get_id(OrionWebviewTabContainer* container);
orion::TabId orion_webview_tab_container_get_parent_id(OrionWebviewTabContainer* container);
std::string orion_webview_tab_container_get_title(OrionWebviewTabContainer* container);
GIcon* orion_webview_tab_container_get_icon(OrionWebviewTabContainer* container);
bool orion_webview_tab_container_get_is_loading(OrionWebviewTabContainer* container);
gboolean orion_webview_tab_container_get_is_playing_audio(OrionWebviewTabContainer* container);
gboolean orion_webview_tab_container_get_is_muted(OrionWebviewTabContainer* container);
orion::TlsStatus orion_webview_tab_container_get_tls_status(OrionWebviewTabContainer* container);
orion::BackForwardList* orion_webview_tab_container_get_back_list(OrionWebviewTabContainer* container);
orion::BackForwardList* orion_webview_tab_container_get_forward_list(OrionWebviewTabContainer* container);
void orion_webview_tab_container_set_is_muted(OrionWebviewTabContainer* container, gboolean muted);
double orion_webview_tab_container_get_seq_num(OrionWebviewTabContainer* container);
bool orion_webview_tab_container_get_nested_tabs_expandable(OrionWebviewTabContainer* container);
int orion_webview_tab_container_get_indent(OrionWebviewTabContainer* container);
orion::Uri orion_webview_tab_container_get_uri(OrionWebviewTabContainer* container);
bool orion_webview_tab_container_get_can_go_back(OrionWebviewTabContainer* container);
bool orion_webview_tab_container_get_can_go_forward(OrionWebviewTabContainer* container);
bool orion_webview_tab_container_get_is_suspended(OrionWebviewTabContainer* container);
bool orion_webview_tab_container_get_group_expanded(OrionWebviewTabContainer* container);

void orion_webview_tab_container_set_content_seq_num(
    OrionWebviewTabContainer* container, double seq_num, UpdateBackend update_backend
);
//does not notify backend of change
void orion_webview_tab_container_set_parent_id(OrionWebviewTabContainer* container, orion::TabId parent_id);
//to be exclusively used in OrionTabSelectionModel
void orion_webview_tab_container_set_nested_tabs_expandable(OrionWebviewTabContainer* container, bool expandable);
//returns whether value was modified or not
bool orion_webview_tab_container_set_group_expanded(OrionWebviewTabContainer* container, bool group_expanded);
void orion_webview_tab_container_set_indent(OrionWebviewTabContainer* container, int indent);
void orion_webview_tab_container_set_content_is_suspended(OrionWebviewTabContainer* container, bool is_suspended);

void orion_webview_tab_container_move_to_window(OrionWebviewTabContainer* container, orion::WindowId id, orion::TabId tab_after, orion::TabId parent_id, bool transfer_with_children);
void orion_webview_tab_container_set_last_active_time(OrionWebviewTabContainer* container, orion::UnixTimeUtc64 last_active_time);
orion::UnixTimeUtc64 orion_webview_tab_container_get_last_active_time(OrionWebviewTabContainer* container);