#include <gtk/gtk.h>
#include <common/src/log.h>
#include <common/src/error.h>
#include <core/settings.h>
#include <common/src/defer.h>

#include "app.h"
#include "add_bookmark_dialog.h"
#include "main_window/main_window.h"
#include "data_item/add_bookmark_location_dropdown_item.h"
#include "util/html_description_util.h"
#include "util/tree_list_row_util.h"

#include "orion_webview_tabview_widgets/orion_webview_tab_view.h"

struct _AddBookmarkDialog
{
	AdwDialog parent;

	GtkEntry* name_entry;
    GtkEntry* address_entry;
    GtkEntry* invocation_keyword_entry;
    GtkDropDown* location_dropdown;

    std::optional<orion::BookmarkFolderId> folder_id;
};

G_DEFINE_TYPE(AddBookmarkDialog, add_bookmark_dialog, ADW_TYPE_DIALOG)

static void
on_add_button_clicked(GtkButton* button, gpointer user_data)
{
    AddBookmarkDialog* dialog = ADD_BOOKMARK_DIALOG(user_data);

    gpointer thing = g_object_ref(gtk_drop_down_get_selected_item(dialog->location_dropdown));
    while(GTK_IS_TREE_LIST_ROW(thing))
    {
        defer([thing]{ g_object_unref(thing); });
        thing = gtk_tree_list_row_get_item(GTK_TREE_LIST_ROW(thing));
    }
    Folder* folder = FOLDER_TYPE(thing);
    defer([folder]{ g_object_unref(folder); });
    
    GValue val = G_VALUE_INIT;
    g_value_init(&val, G_TYPE_ENUM);
    g_object_get_property(G_OBJECT(folder), "type", &val);
    AddBookmarkFolder folder_type = (AddBookmarkFolder)g_value_get_enum(&val);
    if(folder_type==ADD_BOOKMARK_FOLDER_NORMAL || folder_type==ADD_BOOKMARK_FOLDER_BOOKMARKS)
    {
        orion::CreateBookmarkQuery query{};

        query.name = gtk_editable_get_text(GTK_EDITABLE(dialog->name_entry));
        query.address = gtk_editable_get_text(GTK_EDITABLE(dialog->address_entry));
        query.profile_id = main_window_get_current_profile_id(MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(user_data))));
        query.invocation_keyword = gtk_editable_get_text(GTK_EDITABLE(dialog->invocation_keyword_entry));

        GValue folder_id_val = G_VALUE_INIT;
        g_value_init(&folder_id_val, G_TYPE_INT64);
        g_object_get_property(G_OBJECT(folder), "id", &folder_id_val);
        int64_t parent_folder_id = g_value_get_int64(&folder_id_val);
        if(parent_folder_id) query.parent_folder_id = orion::BookmarkFolderId(parent_folder_id);
        g_value_unset(&folder_id_val);

        global_core_api->create_bookmark(query, [](orion::CreateBookmarkReply reply){VERIFY_LOG(reply.err);});
    }
    else if(folder_type == ADD_BOOKMARK_FOLDER_READING_LIST)
    {
        orion::CreateReadingListEntryQuery query{};
        std::string uri = gtk_editable_get_text(GTK_EDITABLE(dialog->address_entry));
        GDateTime* now = g_date_time_new_now_utc();
        query.added_on = orion::UnixTimeUtc64(g_date_time_to_unix(now));
        g_date_time_unref(now);
        
        query.title = gtk_editable_get_text(GTK_EDITABLE(dialog->name_entry));
        query.address = uri;
        query.profile_id = main_window_get_current_profile_id(MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(user_data))));

        global_core_api->create_reading_list_entry(query, [uri](orion::CreateReadingListEntryReply reply){
            VERIFY_LOG(reply.err);
            orion::ReadingListEntryId* id = new orion::ReadingListEntryId(reply.id);
            orion::gtk::execute_with_uri_description<orion::ReadingListEntryId>([](const char* desc, orion::ReadingListEntryId* id){
                if(desc)
                {
                    if(g_utf8_validate(desc, -1, NULL))
                    {
                        const int max_utf8_str_len = 80;
                        //NOTE: max len chosen to not fill database with unnecessarily long data,
                        //refer to g_utf8_strncpy for documentation about the size of the allocated char*
                        char* shrunk_desc = g_new(char, max_utf8_str_len*4+1);
                        g_utf8_strncpy(shrunk_desc, desc, max_utf8_str_len);

                        orion::UpdateReadingListEntryQuery query{};
                        query.id = *id;
                        query.description = shrunk_desc;
                        query.update_description = true;
                        global_core_api->update_reading_list_entry(query, [](orion::Error err){VERIFY_LOG(err);});

                        g_free(shrunk_desc);
                    }
                }
                delete id;
            }, uri.c_str(), id);
        });
    }

    g_value_unset(&val);
    adw_dialog_close(ADW_DIALOG(user_data));
}

static void
on_cancel_button_clicked(GtkButton* button, gpointer user_data)
{
    adw_dialog_close(ADW_DIALOG(user_data));
}

void
add_bookmark_dialog_set_bookmark_folder_id(AddBookmarkDialog* dialog, orion::BookmarkFolderId id)
{
    ADD_BOOKMARK_DIALOG(dialog)->folder_id = id;
}

AdwDialog*
add_bookmark_dialog_new_init_from_current_page(MainWindow* win)
{
	AddBookmarkDialog* dialog = ADD_BOOKMARK_DIALOG(g_object_new(ADD_BOOKMARK_DIALOG_TYPE, NULL));

    OrionWebviewTabView* view = main_window_get_webview_tab_view(MAIN_WINDOW(win));

    orion::Uri current_uri = orion_webview_tab_view_get_uri(view);
    gtk_editable_set_text(GTK_EDITABLE(dialog->address_entry), current_uri.uri().c_str());

    orion::TabId tab_id = main_window_get_current_tab_id(win);
    OrionWebviewTabContent* content = main_window_get_orion_webview_tab_content_by_tab_id(win, tab_id);
    if (!content) {
        orion::log_warning(
            "add_bookmark_dialog_new_init_from_current_page: cannot find tab content for current tab_id=%lld",
            tab_id.value
        );
        return ADW_DIALOG(dialog);
    }

    std::string name = orion_webview_tab_content_get_title(content);
    if(!name.empty())
    {
        gtk_editable_set_text(GTK_EDITABLE(dialog->name_entry), name.c_str());
    }
    return ADW_DIALOG(dialog);
}

AdwDialog*
add_bookmark_dialog_new_init_from_tab_dto(orion::TabDto tab)
{
	AddBookmarkDialog* dialog = ADD_BOOKMARK_DIALOG(g_object_new(ADD_BOOKMARK_DIALOG_TYPE, NULL));

    gtk_editable_set_text(GTK_EDITABLE(dialog->address_entry), tab.uri.c_str());

    gtk_editable_set_text(GTK_EDITABLE(dialog->name_entry), tab.name.c_str());
    return ADW_DIALOG(dialog);
}

static void
on_dropdown_notify_selected_item(GtkDropDown* dropdown, GParamSpec* pspec, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(dropdown)));
    orion::ProfileId profile_id = main_window_get_current_profile_id(win);
    gpointer thing = g_object_ref(gtk_drop_down_get_selected_item(dropdown));
    while(GTK_IS_TREE_LIST_ROW(thing))
    {
        defer([thing]{ g_object_unref(thing); });
        thing = gtk_tree_list_row_get_item(GTK_TREE_LIST_ROW(thing));
    }
    defer([thing]{ g_object_unref(thing); });
    Folder* selected_item = FOLDER_TYPE(thing);

    AddBookmarkFolder type;
    g_object_get(selected_item, "type", &type, NULL);
    global_core_api->set_setting(profile_id, orion::settings::dialogs::add_bookmark::is_reading_list,
        type==ADD_BOOKMARK_FOLDER_READING_LIST, [](orion::SetSettingReply<bool> reply){VERIFY_LOG(reply.err);});

    if(type!=ADD_BOOKMARK_FOLDER_READING_LIST)
    {
        int64_t id;
        g_object_get(selected_item, "id", &id, NULL);
        global_core_api->set_setting(profile_id, orion::settings::dialogs::add_bookmark::default_bookmark_folder_id,
            orion::BookmarkFolderId(id), [](orion::SetSettingReply<orion::BookmarkFolderId>reply){VERIFY_LOG(reply.err);});
    }
}

static void
set_checkmark_icon(GtkDropDown* dropdown, GParamSpec* pspec, GtkListItem* listitem)
{
    //uint selected_pos = gtk_drop_down_get_selected(dropdown);
    //uint this_pos = gtk_list_item_get_position(listitem);

    
    GtkWidget* top_box = gtk_list_item_get_child(listitem);

    GtkWidget* checkmark = gtk_widget_get_first_child(top_box);

    if(gtk_drop_down_get_selected_item(dropdown)==gtk_list_item_get_item(listitem))
    {
        gtk_widget_set_opacity(checkmark, 1.0);
    }
    else
    {
        gtk_widget_set_opacity(checkmark, 0.0);
    }
    
}

static gchar*
get_icon_name(GtkListItem* listitem)
{
    gpointer item = gtk_list_item_get_item(listitem);
    if(!item) return NULL;
    g_object_ref(item);
    while(GTK_IS_TREE_LIST_ROW(item))
    {
        defer([item]{ g_object_unref(item); });
        item = gtk_tree_list_row_get_item(GTK_TREE_LIST_ROW(item));
    }
    defer([item]{ g_object_unref(item); });
    if(item)
    {
        GValue val = G_VALUE_INIT;
        g_value_init(&val, G_TYPE_STRING);
        g_object_get_property(G_OBJECT(item), "icon-name", &val);
        const char* string = g_value_get_string(&val);
        return g_strdup(string);
    }
    else
    {
        return NULL;
    }
}

static gchar*
get_label(GtkListItem* listitem)
{   
    gpointer item = gtk_list_item_get_item(listitem);
    if(!item) return NULL;
    g_object_ref(item);
    while(GTK_IS_TREE_LIST_ROW(item))
    {
        defer([item]{ g_object_unref(item); });
        item = gtk_tree_list_row_get_item(GTK_TREE_LIST_ROW(item));
    }
    defer([item]{ g_object_unref(item); });
    if(item)
    {
        GValue val = G_VALUE_INIT;
        g_value_init(&val, G_TYPE_STRING);
        g_object_get_property(G_OBJECT(item), "name", &val);
        const char* string = g_value_get_string(&val);
        return g_strdup(string);
    }
    else
    {
        return NULL;
    }
}

static gboolean
get_separator_visibility(GtkListHeader* header)
{
    //NOTE: relies on hardcoded position of the "special folders"
    switch(gtk_list_header_get_start(header))
    {
        case 1:
        case 2:
            return true;
        default:
            return false;
    }
}

static void
factory_setup(GtkSignalListItemFactory* factory, GtkListItem* item, gpointer user_data)
{
    GtkWidget* top_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    GtkWidget* low_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);

    GtkWidget* expander = gtk_tree_expander_new();
    GtkWidget* label = gtk_label_new(NULL);
    gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);

    GtkWidget* checkmark = gtk_image_new();
    gtk_image_set_pixel_size(GTK_IMAGE(checkmark), 16);
    gtk_image_set_from_icon_name(GTK_IMAGE(checkmark), "check-plain-symbolic");
    GtkWidget* folder_icon = gtk_image_new();
    gtk_image_set_pixel_size(GTK_IMAGE(folder_icon), 16);

    gtk_box_append(GTK_BOX(top_box), checkmark);
    gtk_box_append(GTK_BOX(top_box), expander);

    gtk_tree_expander_set_child(GTK_TREE_EXPANDER(expander), low_box);

    gtk_box_append(GTK_BOX(low_box), folder_icon);
    gtk_box_append(GTK_BOX(low_box), label);

    gtk_list_item_set_child(item, top_box);
}

static void
factory_bind(GtkSignalListItemFactory* factory, GtkListItem* listitem, gpointer user_data)
{
    GtkWidget* top_box = gtk_list_item_get_child(listitem);

    GtkTreeExpander* expander = GTK_TREE_EXPANDER(gtk_widget_get_last_child(top_box));

    GtkWidget* low_box = gtk_tree_expander_get_child(expander);

    GtkImage* folder_icon = GTK_IMAGE(gtk_widget_get_first_child(low_box));
    GtkLabel* label = GTK_LABEL(gtk_widget_get_last_child(low_box));

    AddBookmarkDialog* dialog = ADD_BOOKMARK_DIALOG(user_data);
    GtkDropDown* dropdown = GTK_DROP_DOWN(dialog->location_dropdown);

    
    gpointer row = g_object_ref(gtk_list_item_get_item(listitem));
    if(GTK_IS_TREE_LIST_ROW(row)) gtk_tree_expander_set_list_row(expander, GTK_TREE_LIST_ROW(row));
    while(GTK_IS_TREE_LIST_ROW(row))
    {
        defer([row]{ g_object_unref(row); });
        row = gtk_tree_list_row_get_item(GTK_TREE_LIST_ROW(row));
    }
    defer([row]{ g_object_unref(row); });
    Folder* item = FOLDER_TYPE(row);



    GValue val = G_VALUE_INIT;
    g_value_init(&val, G_TYPE_STRING);
    g_object_get_property(G_OBJECT(item), "name", &val);
    gtk_label_set_text(label, g_value_get_string(&val));
    g_value_unset(&val);

    val = G_VALUE_INIT;
    g_value_init(&val, G_TYPE_STRING);
    g_object_get_property(G_OBJECT(item), "icon-name", &val);
    gtk_image_set_from_icon_name(folder_icon, g_value_get_string(&val));
    g_value_unset(&val);

    if(gtk_list_item_get_position(listitem)>=1)
    {
        int64_t id;
        g_object_get(G_OBJECT(item), "id", &id, NULL);
        if(dialog->folder_id == orion::BookmarkFolderId(id))
        {
            uint pos = gtk_list_item_get_position(listitem);
            orion::gtk::WeakRefWrapper dropdown_ref(dropdown);
            //NOTE: needs to run async to not run into issues with setting selection in the bind signal
            global_core_api->async([dropdown_ref, pos]{
                auto ptr = dropdown_ref.lock();
                if(!ptr) {
                    return;
                }
                GtkDropDown* dropdown = GTK_DROP_DOWN(ptr.get());
                gtk_drop_down_set_selected(dropdown, pos);
            });
            dialog->folder_id.reset();
        }
    }

    g_signal_connect(dropdown, "notify::selected-item", G_CALLBACK(set_checkmark_icon), listitem);
    set_checkmark_icon(dropdown, NULL, listitem);
    
    //NOTE: relies on hardcoded position of the "special folders"
}

static void
factory_unbind(GtkSignalListItemFactory* factory, GtkListItem* listitem, gpointer user_data)
{
    GtkDropDown* dropdown = GTK_DROP_DOWN(user_data);
    g_signal_handlers_disconnect_by_func(dropdown, (gpointer)set_checkmark_icon, listitem);
}



static GListModel*
location_dropdown_tree_list_model_create_submodel(gpointer item, gpointer user_data)
{
    Folder* parent_folder = FOLDER_TYPE(item);
    MainWindow* win = MAIN_WINDOW(user_data);

    GValue* val = g_new0(GValue, 1);
    g_value_init(val, ADD_BOOKMARK_FOLDER_TYPE);
    g_object_get_property(G_OBJECT(parent_folder), "type", val);

    if(g_value_get_enum(val)==ADD_BOOKMARK_FOLDER_NORMAL)
    {
        GListStore* store = g_list_store_new(folder_get_type());
        auto wrapped_ref = std::make_shared<orion::gtk::WeakRefWrapper>(store);

        
        val = g_new0(GValue, 1);
        g_value_init(val, G_TYPE_INT64);
        g_object_get_property(G_OBJECT(parent_folder), "id", val);

        orion::BookmarkFolderId folder_id = orion::BookmarkFolderId(g_value_get_int64(val));
        g_free(val);

        global_core_api->get_bookmark_folder_ids(main_window_get_current_profile_id(win),
            [folder_id, wrapped_ref](orion::GetBookmarkFolderIdsReply reply){
                VERIFY_LOG(reply.err);
                global_core_api->get_bookmark_folders(reply.ids, [folder_id, wrapped_ref](orion::GetBookmarkFoldersReply reply){
                    VERIFY_LOG(reply.err);
                    GListStore* store = G_LIST_STORE(wrapped_ref->get());
                    if(store)
                    {
                        for(orion::BookmarkFolderDto folder : reply.folders)
                        {
                            if(folder.parent_folder_id==folder_id)
                            {
                                gpointer item = folder_new_from_bookmark_folder_dto(folder);
                                g_list_store_append(store, item);
                                g_object_unref(item);
                            }
                        }
                        g_object_unref(store);
                    }
                });
            });
        return G_LIST_MODEL(store);
    }
    else
    {
        g_free(val);
        return NULL;
    }
}

static void
reload_location_dropdown_options(GtkDropDown* self)
{
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(self)));
    GListStore* root = g_list_store_new(folder_get_type());

    auto wrapped_ref = std::make_shared<orion::gtk::WeakRefWrapper>(root);
    global_core_api->get_bookmark_folder_ids(main_window_get_current_profile_id(win), [wrapped_ref](orion::GetBookmarkFolderIdsReply reply){
        VERIFY_LOG(reply.err);
        global_core_api->get_bookmark_folders(reply.ids, [wrapped_ref](orion::GetBookmarkFoldersReply reply){
            VERIFY_LOG(reply.err);
            GListStore* root = G_LIST_STORE(wrapped_ref->get());
            if(root)
            {
                //these should always be ordered by seq num
                for(orion::BookmarkFolderDto folder : reply.folders)
                {
                    if(folder.parent_folder_id.value == 0)
                    {
                        gpointer item = folder_new_from_bookmark_folder_dto(folder);
                        g_list_store_append(root, item);
                        g_object_unref(item);
                    }
                }
                g_object_unref(root);
            }
        });
    });

    
    GListStore* model_store = g_list_store_new(G_TYPE_LIST_MODEL);

    GListStore* tmp = g_list_store_new(folder_get_type());
    gpointer item = folder_new_reading_list();
    g_list_store_append(tmp, item);
    g_object_unref(item);
    g_list_store_append(model_store, tmp);
    g_object_unref(tmp);

    tmp = g_list_store_new(folder_get_type());
    item = folder_new_bookmarks();
    g_list_store_append(tmp, item);
    g_object_unref(item);
    g_list_store_append(model_store, tmp);
    g_object_unref(tmp);

    GtkTreeListModel* bookmarks_model = gtk_tree_list_model_new(G_LIST_MODEL(root), false, true,
        location_dropdown_tree_list_model_create_submodel, win, NULL);
    g_list_store_append(model_store, bookmarks_model);
    g_object_unref(bookmarks_model);
    GtkFlattenListModel* model = gtk_flatten_list_model_new(G_LIST_MODEL(model_store));
    gtk_drop_down_set_model(self, G_LIST_MODEL(model));
    gtk_widget_queue_resize(GTK_WIDGET(self));
}

static void
on_add_bookmark_dialog_map(AddBookmarkDialog* dialog)
{
    if(!dialog->folder_id.has_value())
    {
        MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(dialog)));
        orion::ProfileId profile_id = main_window_get_current_profile_id(win);
        orion::gtk::WeakRefWrapper dialog_ref(dialog);
        global_core_api->get_setting(profile_id, orion::settings::dialogs::add_bookmark::is_reading_list,
            [dialog_ref, profile_id](orion::GetSettingReply<bool> reply){
                VERIFY_LOG(reply.err);
                if(!reply.value)
                {
                    global_core_api->get_setting(profile_id, orion::settings::dialogs::add_bookmark::default_bookmark_folder_id,
                        [dialog_ref, profile_id](orion::GetSettingReply<orion::BookmarkFolderId> reply){
                            VERIFY_LOG(reply.err);
                            auto a = dialog_ref.lock();
                            if(!a) {
                                return;
                            }
                            AddBookmarkDialog* dialog = ADD_BOOKMARK_DIALOG(a.get());
                            if(dialog)
                            {
                                dialog->folder_id = reply.value;
                                reload_location_dropdown_options(dialog->location_dropdown);
                            }
                        });
                }
                else
                {
                    auto a = dialog_ref.lock();
                    if(!a) {
                        return;
                    }
                    AddBookmarkDialog* dialog = ADD_BOOKMARK_DIALOG(a.get());
                    if(dialog)
                    {
                        reload_location_dropdown_options(dialog->location_dropdown);
                    }
                }
            });
    }
    else
    {
        reload_location_dropdown_options(dialog->location_dropdown);
    }
}

static void
run_later(gpointer user_data)
{
	auto win = ADD_BOOKMARK_DIALOG(user_data);
	gtk_widget_grab_focus(GTK_WIDGET(win->name_entry));
}

static void
add_bookmark_dialog_init(AddBookmarkDialog* win)
{
    std::construct_at(&win->folder_id);

    g_type_ensure(folder_get_type());
	gtk_widget_init_template(GTK_WIDGET(win));

    g_signal_connect(win, "map", G_CALLBACK(on_add_bookmark_dialog_map), NULL);
    //NOTE: while this is not the the cleanest way to write this, parameter count matches exactly
    g_signal_connect(win->location_dropdown, "unmap", G_CALLBACK(gtk_drop_down_set_model), NULL);
    g_signal_connect(win->location_dropdown, "notify::selected-item", G_CALLBACK(on_dropdown_notify_selected_item), NULL);

    GtkListItemFactory* factory = gtk_signal_list_item_factory_new();
    gtk_drop_down_set_list_factory(win->location_dropdown, factory);
    g_signal_connect(factory, "setup", G_CALLBACK(factory_setup), NULL);
    g_signal_connect(factory, "bind", G_CALLBACK(factory_bind), win);
    g_signal_connect(factory, "unbind", G_CALLBACK(factory_unbind), win->location_dropdown);




	// Focus name entry and select all text - this cannot be done
	// directly in the init function here because it does not work.
	// Perform this operation later.
	// This kind of 'once' timer is removed automatically.
	g_timeout_add_once(100, run_later, win);
}

static void
add_bookmark_dialog_finalize(GObject* object)
{
    AddBookmarkDialog* self = ADD_BOOKMARK_DIALOG(object);
    std::destroy_at(&self->folder_id);
    G_OBJECT_CLASS(add_bookmark_dialog_parent_class)->finalize(object);
}

static void
add_bookmark_dialog_class_init(AddBookmarkDialogClass* klass)
{
    G_OBJECT_CLASS(klass)->finalize = add_bookmark_dialog_finalize;
	gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass), GRESOURCE_PREFIX "/src/window/add_bookmark_dialog.ui");

	gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), AddBookmarkDialog, name_entry);
	gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), AddBookmarkDialog, address_entry);
	gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), AddBookmarkDialog, invocation_keyword_entry);
	gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), AddBookmarkDialog, location_dropdown);

	gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), on_cancel_button_clicked);
	gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), on_add_button_clicked);
	gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), get_icon_name);
	gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), get_label);
	gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), get_separator_visibility);
}
