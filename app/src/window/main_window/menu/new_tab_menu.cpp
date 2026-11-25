#include <common/src/defer.h>
#include <common/src/log.h>

#include "new_tab_menu.h"
#include "window/main_window/main_window.h"
#include "util/dialog_util.h"
#include "util/custom_uri_util.h"
#include "util/menu_util.h"
#include "util/text_util.h"

// x, y coordinates are relative to the widget, not the parent window
void
new_tab_button_context_menu(GtkGestureClick* click, gdouble x, gdouble y, GtkPopoverMenu* popover_menu)
{
    GtkWidget* widget = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(click));
    
    MainWindow* win = orion::gtk::get_root_main_window(widget);
    if (!win) {
        orion::log_warning("new_tab_button_context_menu: could not find root main window");
        return;
    }

    GtkWidget* parent = gtk_widget_get_parent(GTK_WIDGET(popover_menu));
    if (!parent) {
        orion::log_warning("new_tab_button_context_menu: popover menu has no parent");
        return;
    }

    // Transform the coordinates from widget to parent space
    graphene_point_t zero{};
    graphene_point_t pos{};
	if (!gtk_widget_compute_point(widget, parent, &zero, &pos)) {
		orion::log_warning("new_tab_button_context_menu: failed to compute point");
        return;
	}

    orion::log_debug("new_tab_button_context_menu: x=%f y=%f pos=(%f, %f)", x, y, pos.x, pos.y);

    GdkRectangle rect = {
        .x = static_cast<int>(pos.x + x),
        .y = static_cast<int>(pos.y + y),
        .width = 1,
        .height = 1,
    };
    orion::ProfileId profile_id = main_window_get_current_profile_id(win);
    orion::gtk::WeakRefWrapper popover_menu_weak(popover_menu);

    orion::GetRecentlyClosedTabIdsQuery query{
        .profile_id = profile_id,
        .window_id = std::nullopt, // do not filter by window
    };

    global_core_api->get_recently_closed_tab_ids(query,
    [rect,popover_menu_weak](orion::GetRecentlyClosedTabIdsReply reply){
        VERIFY_LOG(reply.err);
        global_core_api->get_recently_closed_tabs(reply.ids,
        [rect,popover_menu_weak](orion::GetRecentlyClosedTabsReply reply){
            VERIFY_LOG(reply.err);

            auto popover_menu_strong = popover_menu_weak.lock();
            if (!popover_menu_strong) return;
            GtkPopoverMenu* popover_menu = GTK_POPOVER_MENU(popover_menu_strong.get());

            GMenu* root = g_menu_new();
            defer([root]{ g_object_unref(root); });
            {
                GMenu* section = g_menu_new();
                defer([section]{ g_object_unref(section); });
                {
                    GMenuItem* item = g_menu_item_new(_("New Tab"), "win.new-tab");
                    defer([item]{ g_object_unref(item); });
                    g_menu_append_item(section, item);
                }
                {
                    GMenuItem* item = g_menu_item_new(_("New Private Tab"), "win.new-private-tab");
                    defer([item]{ g_object_unref(item); });
                    g_menu_append_item(section, item);
                }
                g_menu_append_section(root, NULL, G_MENU_MODEL(section));
            }
            {
                GMenu* section = g_menu_new();
                defer([section]{ g_object_unref(section); });
                const int max_items = 5;
                int count = 0;
                for (const auto& tab : reply.tabs) {
                    std::string title = tab.name;
                    if (orion::Uri::parse_strict(tab.uri.c_str()).known_scheme() == orion::Uri::Scheme::orion) {
                        title = orion::custom_uri_get_corresponding_title(tab.uri);
                    } else if (tab.name.empty()) {
                        title = _("<no-title>");
                    }
                    orion::gtk::menu_append_action_with_int64_target(
                        section,
                        orion::text::ellipsize_string(title, MAX_CONTEXT_MENU_USER_STRING_LENGTH).c_str(),
                        "win.open-recently-closed-tab",
                        tab.id.value
                    );
                    count++;
                    if (count >= max_items) break;
                }
                g_menu_append_section(root, _("Recently Closed Tabs"), G_MENU_MODEL(section));
            }

            gtk_popover_set_pointing_to(GTK_POPOVER(popover_menu), &rect);
            gtk_popover_set_has_arrow(GTK_POPOVER(popover_menu), FALSE);
            gtk_popover_menu_set_menu_model(popover_menu, G_MENU_MODEL(root));
            gtk_popover_popup(GTK_POPOVER(popover_menu));
        });
    });
}