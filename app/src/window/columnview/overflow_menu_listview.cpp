#include <common/src/defer.h>
#include <core/settings.h>

#include "global_core_api.h"
#include "overflow_menu_listview.h"
#include "data_item/toolbar_item.h"
#include "window/main_window/main_window.h"

void
on_overflow_menu_listview_map(GtkListView* self, gpointer user_data)
{
    GtkRoot* root = gtk_widget_get_root(GTK_WIDGET(self));
    if (!MAIN_IS_WINDOW(root)) {
        orion::log_warning("on_overflow_menu_listview_map: root is not a MainWindow");
        return;
    }

    orion::ProfileId profile_id = main_window_get_current_profile_id(MAIN_WINDOW(root));
    orion::gtk::WeakRefWrapper self_weak(self);

    global_core_api->get_setting(profile_id, orion::settings::tabs::tab_layout,
    [profile_id,self_weak](orion::GetSettingReply<orion::settings::TabLayout> reply){
        VERIFY_LOG(reply.err);

        bool is_compact_tab_layout = (reply.value == orion::settings::TabLayout::compact);

        orion::GetToolbarItemsQuery query{
            .profile_id = profile_id,
            .is_compact_tab_layout = is_compact_tab_layout,
        };

        global_core_api->get_toolbar_items(query, [self_weak](orion::GetToolbarItemsReply reply){
            VERIFY_LOG(reply.err);

            auto self_strong = self_weak.lock();
            if (!self_strong) return;
            GtkListView* self = GTK_LIST_VIEW(self_strong.get());

            GListStore* store = g_list_store_new(TOOLBAR_ITEM_TYPE);
            std::vector<orion::ProgrammableButtonId> ids{};
            for (const orion::ToolbarItemDto& dto : reply.items) {
                if (!dto.in_overflow_menu) continue; // only show items that are in the overflow menu
                
                ToolbarItem* item = toolbar_item_new_from_dto(dto);
                defer([item]{ g_object_unref(item); });
                toolbar_item_set_source(item, ToolbarItemSource::overflow_menu_listview);
                g_list_store_append(store, item);
                ids.push_back(PROGRAMMABLE_BUTTON_ID(dto.id));
            }

            orion::gtk::WeakRefWrapper store_weak(store);
            orion::GetProgrammableButtonsQuery query{};
            query.ids = std::move(ids);
            global_core_api->get_programmable_buttons(query, [store_weak](orion::GetProgrammableButtonsReply reply){
                VERIFY_LOG(reply.err);

                auto store_strong = store_weak.lock();
                if(!store_strong) return;

                GListModel* store = G_LIST_MODEL(store_strong.get());

                guint n_items = g_list_model_get_n_items(store);
                for(guint i = 0; i<n_items; i++)
                {
                    ToolbarItem* item = APP_TOOLBAR_ITEM(g_list_model_get_item(store, i));
                    defer([item]{ g_object_unref(item); });
                    if(toolbar_item_get_item_type(item) == orion::ToolbarItemType::programmable_button)
                    {
                        auto it = std::find_if(reply.items.cbegin(), reply.items.cend(),
                            [id = PROGRAMMABLE_BUTTON_ID(toolbar_item_get_id(item))](orion::ProgrammableButtonDto dto){ return dto.id.value() == id; });

                        // Update the name of the item to reflect the title of the Programmable Button, but only if the
                        // button has a non-empty name
                        if(it != reply.items.cend() && !it->title.empty())
                        {
                            g_object_set(item, "translated-name", it->title.c_str(), NULL);
                        }
                    }
                }
            });

            GtkNoSelection* selection = gtk_no_selection_new(G_LIST_MODEL(store));
            gtk_list_view_set_model(self, GTK_SELECTION_MODEL(selection));
        });
    });
}

void
overflow_menu_listview_unmap(GtkListView* self, gpointer user_data)
{
}

void
overflow_menu_listview_dispose(GtkListView* self)
{
    gtk_list_view_set_model(self, NULL);
    gtk_list_view_set_factory(self, NULL);
}

void
overflow_menu_listview_finalize(GtkListView* self)
{
    // TODO:
}

void
overflow_menu_listview_init(GtkListView* self)
{
    // TODO:
}