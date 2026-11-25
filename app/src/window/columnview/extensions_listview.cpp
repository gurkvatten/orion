#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <vector>
#include <string>
#include <iostream>
#include <functional>

#include <common/src/log.h>

#include "app.h"
#include "data_item/extension_item.h"
#include "util/text_util.h"
#include "util/menu_util.h"
#include "window/main_window/main_window.h"
#include "window/extensions_keyboard_shortcuts_dialog.h"

char*
get_extension_title_label(GtkListItem* listitem)
{
    ExtensionItem* item = EXTENSION_ITEM(gtk_list_item_get_item(listitem));
    if(item)
    {
        if(extension_item_get_version(item)->len) 
        {
            return g_strdup_printf("%s (%s)", extension_item_get_title(item)->str, extension_item_get_version(item)->str);
        }
        else
        {
            return g_strdup(extension_item_get_title(item)->str);
        }
    }
    else
    {
        return NULL;
    }
}

char*
get_extension_source_label(GtkListItem* listitem)
{
    ExtensionItem* item = EXTENSION_ITEM(gtk_list_item_get_item(listitem));
    if(item)
    {
        char* source_name;
        orion::ExtensionType type = extension_item_get_extension_type(item);
        switch(type) {
            case orion::ExtensionType::chrome:
                source_name = g_strdup("Chrome");
                break;
            case orion::ExtensionType::firefox:
                source_name = g_strdup("Firefox");
                break;
            case orion::ExtensionType::gresources:
                source_name = g_strdup(_("Built-in"));
                break;
            case orion::ExtensionType::not_set:
            default:
                source_name = g_strdup(_("unknown"));
                break;
        }

        orion::UnixTimeUtc64 last_updated_at = extension_item_get_last_updated_at(item);
        if(int64_t(last_updated_at) == 0)
        {
            return source_name;
        }
        else
        {
            GDateTime* date_time = g_date_time_new_from_unix_utc(int64_t(last_updated_at));
            char* date = g_date_time_format(date_time, "%c");
            g_date_time_unref(date_time);
            std::string return_string = orion::text::translatef("%1$s - Last updated on %2$s", source_name, date);
            g_free(source_name);
            return g_strdup(return_string.c_str());
        }
    }
    else
    {
        return NULL;
    }
}

char*
get_extension_icon_ref(GtkListItem* listitem)
{
    ExtensionItem* item = EXTENSION_ITEM(gtk_list_item_get_item(listitem));
    if(item)
    {
        return g_strdup(extension_item_get_icon_ref(item)->str);
    }
    else
    {
        return NULL;
    }
}

bool
get_extension_keyboard_shortcuts_button_visible(GtkListItem* listitem)
{
    ExtensionItem* item = EXTENSION_ITEM(gtk_list_item_get_item(listitem));
    if(item)
    {
        //TODO: implement this 
        return true;
    }
    else
    {
        return false;
    }
}

void
on_extension_keyboard_shortcuts_button_clicked(GtkButton* self, gpointer user_data)
{
    ExtensionItem* item = EXTENSION_ITEM(gtk_list_item_get_item(GTK_LIST_ITEM(user_data)));
    orion::ExtensionId* id = extension_item_get_id(item);
    AdwDialog* dialog = extensions_keyboard_shortcuts_dialog_new(*id);
    adw_dialog_present(dialog, GTK_WIDGET(gtk_widget_get_root(GTK_WIDGET(self))));
}


void
on_extension_uninstall_button_clicked(GtkButton* self, gpointer user_data)
{
    GtkListItem* listitem = GTK_LIST_ITEM(user_data);
    ExtensionItem* item = EXTENSION_ITEM(gtk_list_item_get_item(listitem));
    orion::ExtensionId* id = extension_item_get_id(item);

    orion::UninstallExtensionQuery query{};
    query.id = *id;
    global_core_api->uninstall_extension(query, [](orion::UninstallExtensionReply reply){VERIFY_LOG(reply.err);});
}

bool
on_extension_enable_switch_state_set(GtkSwitch* self, gboolean state, gpointer user_data)
{
    GtkListItem* listitem = GTK_LIST_ITEM(user_data);
    ExtensionItem* item = EXTENSION_ITEM(gtk_list_item_get_item(listitem));
    if(!item) return true;
    orion::ExtensionId* id = extension_item_get_id(item);

    orion::UpdateExtensionQuery query{};
    query.id = *id;
    query.is_enabled = gtk_switch_get_active(self);
    query.update_is_enabled = true;
    global_core_api->update_extension(query, [](orion::UpdateExtensionReply reply){VERIFY_LOG(reply.err);});

    return true;
}

void
on_extension_enable_switch_map(GtkSwitch* self, gpointer user_data)
{
    ExtensionItem* item = EXTENSION_ITEM(gtk_list_item_get_item(GTK_LIST_ITEM(user_data)));
    g_signal_handlers_block_by_func(self, (void*)on_extension_enable_switch_state_set, user_data);
    gtk_switch_set_active(self, extension_item_get_is_enabled(item));
    g_signal_handlers_unblock_by_func(self, (void*)on_extension_enable_switch_state_set, user_data);
}


static void
on_extension_menu_button_unmap(GtkMenuButton* self, gpointer user_data)
{
    gtk_widget_insert_action_group(GTK_WIDGET(self), static_cast<const char*>(user_data), NULL);
    g_signal_handlers_disconnect_by_data(self, user_data);
}

struct ExtensionConsoleActionData {
    orion::ExtensionId extension_id;
    orion::gtk::WeakRefWrapper menu_button;
};

static void
on_extension_menu_button_console_action_activate(GAction* action, GVariant* parameter, gpointer user_data)
{
    auto* data = static_cast<ExtensionConsoleActionData*>(user_data);
    auto menu_button = data->menu_button.lock();
    if(!menu_button) {
        return;
    }

    GtkRoot* root = gtk_widget_get_root(GTK_WIDGET(menu_button.get()));
    if(!root || !MAIN_IS_WINDOW(root)) {
        return;
    }
    main_window_get_current_webview_factory(MAIN_WINDOW(root))
        .show_extension_background_web_inspector(data->extension_id);
}

static void
on_extension_menu_button_do_not_update_setting_action_activate(GAction* action, GVariant* parameter, gpointer user_data)
{
    ExtensionItem* item = EXTENSION_ITEM(user_data);
    orion::ExtensionId* id = extension_item_get_id(item);

    orion::UpdateExtensionQuery query{};
    query.id = *id;
    query.do_not_update = !extension_item_get_do_not_update(item);
    query.update_do_not_update = true;
    global_core_api->update_extension(query, [](orion::UpdateExtensionReply reply){VERIFY_LOG(reply.err);});
}

void
on_extension_menu_button_map(GtkMenuButton* self, gpointer user_data)
{
    GtkListItem* listitem = GTK_LIST_ITEM(user_data);
    ExtensionItem* item = EXTENSION_ITEM(gtk_list_item_get_item(listitem));

    std::string id = extension_item_get_id(item)->value;
    std::string base_action_name = "do-not-update-setting";
    std::string action_name = id + "." + base_action_name;
    GVariant* var = g_variant_new_boolean(extension_item_get_do_not_update(item));
    GSimpleAction* action = g_simple_action_new_stateful(base_action_name.c_str(), NULL, var);
    g_signal_connect(action, "activate", G_CALLBACK(on_extension_menu_button_do_not_update_setting_action_activate), item);
    GSimpleActionGroup* action_group = g_simple_action_group_new();
    g_action_map_add_action(G_ACTION_MAP(action_group), G_ACTION(action));
    g_object_unref(action);

    action = g_simple_action_new("console", NULL);
    g_signal_connect_data(
        action,
        "activate",
        G_CALLBACK(on_extension_menu_button_console_action_activate),
        new ExtensionConsoleActionData{
            .extension_id = *extension_item_get_id(item),
            .menu_button = orion::gtk::WeakRefWrapper(self),
        },
        [](gpointer data, GClosure* closure) {
            delete static_cast<ExtensionConsoleActionData*>(data);
        },
        G_CONNECT_DEFAULT
    );
    g_action_map_add_action(G_ACTION_MAP(action_group), G_ACTION(action));
    g_object_unref(action);

    gtk_widget_insert_action_group(GTK_WIDGET(self), id.c_str(), G_ACTION_GROUP(action_group));
    g_object_unref(action_group);
    
    
    GMenu* menu = g_menu_new();
    
    GMenu* section = g_menu_new();
    if(extension_item_get_has_options(item))
    {
        g_menu_append(section, _("Options"), NULL);
    }
    std::string console_action_name = id + ".console";
    g_menu_append(section, _("Console"), console_action_name.c_str());
    //TODO: properly handle this action
    g_menu_append(section, _("Permissions"), "win.extension-permissions");
    g_menu_append_section(menu, NULL, G_MENU_MODEL(section));
    g_object_unref(section);

    section = g_menu_new();
    //TODO: figure out why this doesn't work
    g_menu_append(section, _("Do not update"), action_name.c_str());
    g_menu_append_section(menu, NULL, G_MENU_MODEL(section));
    g_object_unref(section);

    gtk_menu_button_set_menu_model(self, G_MENU_MODEL(menu));
    g_object_unref(menu);
    
    g_signal_connect_data(self, "unmap", G_CALLBACK(on_extension_menu_button_unmap), g_strdup(id.c_str()),
        [](gpointer data, GClosure* closure){
            g_free(data);
        }, G_CONNECT_DEFAULT);
}

static void
on_extensions_list_view_unmap(GtkListView* view, gpointer user_data)
{
    gtk_list_view_set_model(view, NULL);
    g_signal_handlers_disconnect_by_data(view, user_data);
}

static void
extensions_list_view_extension_created(GListStore* store, const orion::ExtensionDto& extension_created, orion::ProfileId profile_id)
{
    if(extension_created.profile_id!=profile_id) return;

    UNIQUE_PTR(ExtensionItem, g_object_unref) item(extension_item_new_with_desc(extension_created));
    g_list_store_insert_sorted(store, item.get(), extension_item_compare, NULL);
}

static void
extensions_list_view_extension_deleted(GListStore* store, const orion::ExtensionId extension_deleted)
{
    GListModel* model = G_LIST_MODEL(store);
    uint len = g_list_model_get_n_items(model);
    for(uint i = 0; i<len; i++)
    {
        ExtensionItem* item = EXTENSION_ITEM(g_list_model_get_item(model, i));
        defer([item]{ g_object_unref(item); });
        orion::ExtensionId* id = extension_item_get_id(item);
        if(extension_deleted==*id)
        {
            g_list_store_remove(store, i);
            return;
        }
    }
}

static void
extensions_list_view_handle_change(GListStore* store, const orion::ExtensionsChange& change, orion::ProfileId profile_id)
{
    for(orion::ExtensionDto extension_created : change.created)
    {
        extensions_list_view_extension_created(store, extension_created, profile_id);
    }
    for(orion::ExtensionId extension_deleted : change.deleted)
    {
        extensions_list_view_extension_deleted(store, extension_deleted);
    }
    for(orion::ExtensionDto extension_updated : change.updated)
    {
        extensions_list_view_extension_deleted(store, extension_updated.id);
        extensions_list_view_extension_created(store, extension_updated, profile_id);
    }
}

detached_task
on_extensions_list_view_map(GtkListView* view, gpointer user_data)
{
    orion::ProfileId profile_id = main_window_get_current_profile_id(MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(view))));
    auto wrapped_ref = std::make_shared<orion::gtk::WeakRefWrapper>(view);

    orion::ObserverId* id = new orion::ObserverId;
    *id = global_core_api->extensions_change().add_observer([wrapped_ref, profile_id](const orion::ExtensionsChange& change){
        GtkListView* view = GTK_LIST_VIEW(wrapped_ref->get());
        if(view)
        {
            GtkNoSelection* selection = GTK_NO_SELECTION(gtk_list_view_get_model(view));
            if(selection)
            {
                GListStore* store = G_LIST_STORE(gtk_no_selection_get_model(selection));

                extensions_list_view_handle_change(store, change, profile_id);
            }
            g_object_unref(view);
        }
    });

    g_signal_connect_data(view, "unmap", G_CALLBACK(on_extensions_list_view_unmap), id,
        [](gpointer data, GClosure* closure){
            orion::ObserverId* id = static_cast<orion::ObserverId*>(data);
            global_core_api->extensions_change().remove_observer(*id);
            delete id;
        }, G_CONNECT_DEFAULT);

    orion::GetInstalledExtensionIdsQuery all_extension_ids_query{};
    all_extension_ids_query.profile_id = profile_id;
    orion::GetAllInstalledExtensionIdsReply all_extension_ids_reply = AWAIT(get_installed_extension_ids, all_extension_ids_query);
    CO_VERIFY_LOG(all_extension_ids_reply.err);
    orion::GetInstalledExtensionsQuery all_extensions_query{};
    all_extensions_query.ids = all_extension_ids_reply.ids;
    orion::GetInstalledExtensionsReply all_extensions_reply = AWAIT(get_installed_extensions, all_extensions_query);
    CO_VERIFY_LOG(all_extensions_reply.err);
    auto ref = wrapped_ref->lock();
    if(!ref) {
        co_return;
    }
    view = GTK_LIST_VIEW(ref.get());
    GListStore* store = g_list_store_new(G_TYPE_OBJECT);
    for(orion::ExtensionDto extension : all_extensions_reply.extensions) {
        UNIQUE_PTR(ExtensionItem, g_object_unref) item(extension_item_new_with_desc(extension));
        g_list_store_append(store, item.get());
    }
    GtkNoSelection* selection = gtk_no_selection_new(G_LIST_MODEL(store));
    gtk_list_view_set_model(view, GTK_SELECTION_MODEL(selection));
    
    co_return;
}
