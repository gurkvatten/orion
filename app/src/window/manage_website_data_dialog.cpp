#include <gdk/gdk.h>

#include "../app.h"

#include <common/src/defer.h>

#include "manage_website_data_dialog.h"

#include "window/main_window/orion_window_iface.h"

#include "data_item/website_data_storage_item.h"

#include "util/coro_util.h"
#include "util/favicon_util.h"

struct _ManageWebsiteDataDialog
{
	AdwDialog parent;

    GtkListView* view;

    GtkCustomFilter* search_filter;
};

G_DEFINE_TYPE(ManageWebsiteDataDialog, manage_website_data_dialog, ADW_TYPE_DIALOG)

AdwDialog*
manage_website_data_dialog_new()
{
	return ADW_DIALOG(g_object_new(MANAGE_WEBSITE_DATA_DIALOG_TYPE, NULL));
}

static void
on_done_clicked(GtkButton* button, gpointer user_data)
{
    adw_dialog_close(ADW_DIALOG(user_data));
}

namespace orion::gtk {
static void webkit_website_data_unref_wrapper(gpointer data) {
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    webkit_website_data_unref(static_cast<WebKitWebsiteData*>(data));
#endif
};
}


static gboolean
g_list_contains(GList* list, gpointer data)
{
    for(; list; list = list->next)
    {
        if(list->data == data) return TRUE;
    }
    return FALSE;
}

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
static detached_task
remove_website_data(ManageWebsiteDataDialog* self, GList* list)
{
    g_object_ref(self);
    defer([self]{ g_object_unref(self); });

    defer([list]{ g_list_free_full(list, orion::gtk::webkit_website_data_unref_wrapper); });
    OrionWindow* win = ORION_WINDOW(gtk_widget_get_root(GTK_WIDGET(self)));
    if(!ORION_IS_WINDOW(win)) co_return;
    

    WebExtensions::WebExtensionWebViewFactory& factory = orion_window_get_webview_factory(win);
    WebKitNetworkSession* session = WEBKIT_NETWORK_SESSION(factory.network_session());
    WebKitWebsiteDataManager* mgr = webkit_network_session_get_website_data_manager(session);


    auto res = co_await orion::gtk::await<webkit_website_data_manager_remove, webkit_website_data_manager_remove_finish>(
        mgr, WEBKIT_WEBSITE_DATA_ALL, list, nullptr);

    if(!orion::gtk::log_result_error(res) && res.value())
    {
        GtkMultiSelection* selection = GTK_MULTI_SELECTION(gtk_list_view_get_model(self->view));
        GtkFilterListModel* filter_model = GTK_FILTER_LIST_MODEL(gtk_multi_selection_get_model(selection));
        GListStore* store = G_LIST_STORE(gtk_filter_list_model_get_model(filter_model));

        if(!store) co_return;

        GListModel* store_model = G_LIST_MODEL(store);

        guint n_items = g_list_model_get_n_items(store_model);

        for(guint i = 0; i<n_items; )
        {
            WebsiteDataStorageItem* item = APP_WEBSITE_DATA_STORAGE_ITEM(g_list_model_get_item(store_model, i));
            defer([item]{ g_object_unref(item); });

            WebKitWebsiteData* data = website_data_storage_item_get_data(item);

            if(data && g_list_contains(list, data))
            {
                g_list_store_remove(store, i);
                n_items--;
            }
            else
            {
                i++;
            }
        }
    }
}
#endif

static void
on_remove_clicked(GtkButton* button, gpointer user_data)
{
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    ManageWebsiteDataDialog* self = MANAGE_WEBSITE_DATA_DIALOG(user_data);

    GtkSelectionModel* selection = gtk_list_view_get_model(self->view);
    if(!selection) return;

    GtkBitset* bitset = gtk_selection_model_get_selection(selection);
    defer([bitset]{ gtk_bitset_unref(bitset); });

    guint64 selection_size = gtk_bitset_get_size(bitset);

    GList* list = NULL;

    for(guint64 i = 0; i<selection_size; i++)
    {
        guint index = gtk_bitset_get_nth(bitset, (guint)i); //comedic looking cast because somehow, get_nth takes a smaller data type than get_size returns

        WebsiteDataStorageItem* item = APP_WEBSITE_DATA_STORAGE_ITEM(g_list_model_get_item(G_LIST_MODEL(selection), index));
        defer([item]{ g_object_unref(item); });

        WebKitWebsiteData* data = website_data_storage_item_get_data(item);
        if(data)
        {
            list = g_list_prepend(list, webkit_website_data_ref(data));
        }
    }
    
    remove_website_data(self, list);
#endif
}

static void
on_remove_all_clicked(GtkButton* button, gpointer user_data)
{
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    ManageWebsiteDataDialog* self = MANAGE_WEBSITE_DATA_DIALOG(user_data);

    GListModel* model = G_LIST_MODEL(gtk_list_view_get_model(self->view));
    if(!model) return;


    guint n_items = g_list_model_get_n_items(model);

    GList* list = NULL;

    for(guint i = 0; i<n_items; i++)
    {

        WebsiteDataStorageItem* item = APP_WEBSITE_DATA_STORAGE_ITEM(g_list_model_get_item(model, i));
        defer([item]{ g_object_unref(item); });

        WebKitWebsiteData* data = website_data_storage_item_get_data(item);
        if(data)
        {
            list = g_list_prepend(list, webkit_website_data_ref(data));
        }
    }
    
    remove_website_data(self, list);
#endif
}

static void
on_entry_search_changed(GtkSearchEntry* entry, ManageWebsiteDataDialog* self)
{
    char* data = g_utf8_casefold(gtk_editable_get_text(GTK_EDITABLE(entry)), -1);
    gtk_custom_filter_set_filter_func(self->search_filter, website_data_storage_item_filter_func, data, g_free);
}

static void
manage_website_data_dialog_init(ManageWebsiteDataDialog* win)
{
    win->search_filter = gtk_custom_filter_new(website_data_storage_item_filter_func, NULL, NULL);
    
	gtk_widget_init_template(GTK_WIDGET(win));
}

static detached_task
manage_website_data_dialog_populate(ManageWebsiteDataDialog* self)
{
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    g_object_ref(self);
    defer([self]{ g_object_unref(self); });

    //separate scope to better showcase lifetimes in the coroutine
    //mgr's lifetime is also not preserved past the await, but moving this inside the separate scope would require further infrastructure in GlibResult
    WebKitWebsiteDataManager* mgr = NULL;
    {
        OrionWindow* win = ORION_WINDOW(gtk_widget_get_root(GTK_WIDGET(self)));
        if(!ORION_IS_WINDOW(win)) co_return;
        

        WebExtensions::WebExtensionWebViewFactory& factory = orion_window_get_webview_factory(win);
        WebKitNetworkSession* session = WEBKIT_NETWORK_SESSION(factory.network_session());
        if(!session) co_return;
        mgr = webkit_network_session_get_website_data_manager(session);
    }
    
    auto res = co_await orion::gtk::await<webkit_website_data_manager_fetch, webkit_website_data_manager_fetch_finish>(mgr, WEBKIT_WEBSITE_DATA_ALL, nullptr);

    if(!orion::gtk::log_result_error(res))
    {
        GList* list = res.value();
        defer([list]{ g_list_free_full(list, orion::gtk::webkit_website_data_unref_wrapper); });

        
        GListStore* store = g_list_store_new(G_TYPE_OBJECT);

        for(GList* current = list; current; current = current->next)
        {
            WebKitWebsiteData* data = static_cast<WebKitWebsiteData*>(current->data);

            WebsiteDataStorageItem* item = APP_WEBSITE_DATA_STORAGE_ITEM(g_object_new(APP_TYPE_WEBSITE_DATA_STORAGE_ITEM,
                "data", data,
                NULL
            ));

            g_list_store_append(store, item);

            g_object_unref(item);
        }

        GtkFilterListModel* filter_model = gtk_filter_list_model_new(G_LIST_MODEL(store), GTK_FILTER(g_object_ref(self->search_filter)));
        GtkSelectionModel* selection = GTK_SELECTION_MODEL(gtk_multi_selection_new(G_LIST_MODEL(filter_model)));
        gtk_list_view_set_model(self->view, selection);
        g_object_unref(selection);
    }
#else
    return {};
#endif
}

static GIcon*
get_uri_favicon(GtkListItem* closure_object, const gchar* name, GtkImage* image)
{
    OrionWindow* win = ORION_WINDOW(gtk_widget_get_root(GTK_WIDGET(image)));
    if(!ORION_IS_WINDOW(win)) return NULL;

    if(name)
    {
        orion::Uri uri = orion::Uri::parse_user_input(name); //website data names exclude a protocol for some reason

        orion::gtk::load_gtk_image_favicon_from_uri(win, image, uri.uri().c_str());
    }
    return NULL;
}


static void
manage_website_data_dialog_map(GtkWidget* widget)
{
    GTK_WIDGET_CLASS(manage_website_data_dialog_parent_class)->map(widget);
    
    ManageWebsiteDataDialog* self = MANAGE_WEBSITE_DATA_DIALOG(widget);
    manage_website_data_dialog_populate(self);
}

static void
manage_website_data_dialog_unmap(GtkWidget* widget)
{
    GTK_WIDGET_CLASS(manage_website_data_dialog_parent_class)->unmap(widget);
    
    ManageWebsiteDataDialog* self = MANAGE_WEBSITE_DATA_DIALOG(widget);
    gtk_list_view_set_model(self->view, NULL);
}

static void
manage_website_data_dialog_finalize(GObject* object)
{
    ManageWebsiteDataDialog* self = MANAGE_WEBSITE_DATA_DIALOG(object);
    g_clear_object(&self->search_filter);

    G_OBJECT_CLASS(manage_website_data_dialog_parent_class)->finalize(object);
}

static void
manage_website_data_dialog_class_init(ManageWebsiteDataDialogClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);
    object_class->finalize = manage_website_data_dialog_finalize;

    GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);
    widget_class->map = manage_website_data_dialog_map;
    widget_class->unmap = manage_website_data_dialog_unmap;

	gtk_widget_class_set_template_from_resource(widget_class, GRESOURCE_PREFIX "/src/window/manage_website_data_dialog.ui");
    gtk_widget_class_bind_template_child(widget_class, ManageWebsiteDataDialog, view);
    gtk_widget_class_bind_template_callback(widget_class, on_done_clicked);
    gtk_widget_class_bind_template_callback(widget_class, on_remove_clicked);
    gtk_widget_class_bind_template_callback(widget_class, on_remove_all_clicked);
    gtk_widget_class_bind_template_callback(widget_class, get_uri_favicon);
    gtk_widget_class_bind_template_callback(widget_class, on_entry_search_changed);
}
