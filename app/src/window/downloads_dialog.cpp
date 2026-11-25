#include <gtk/gtk.h>

#include <common/src/defer.h>

#include "app.h"
#include "downloads_dialog.h"
#include "move_downloads_to_trash_dialog.h"
#include "main_window/main_window.h"
#include "data_item/downloads_item.h"
#include "columnview/downloads_item_widget.h"
//#include "scaled_container/popover_listview_container.h"

namespace {
    struct CppWrapper {
        orion::ObserverId downloads_observer;
    };
}

struct _DownloadsDialog
{
	AdwDialog parent;

    GtkListView* downloads_listview;
    AdwDialog* move_to_trash_dialog;
    GtkExpressionWatch* profile_id_watch;

    uint download_count;

    CppWrapper cpp_data;
};

G_DEFINE_TYPE(DownloadsDialog, downloads_dialog, ADW_TYPE_DIALOG)

enum {
    PROP_0,
    PROP_SHOW_LISTVIEW,
    N_PROPS,
};

static GParamSpec* obj_properties[N_PROPS];

AdwDialog*
downloads_dialog_new()
{
	return ADW_DIALOG(g_object_new(DOWNLOADS_DIALOG_TYPE, NULL));
}

static void
on_move_to_trash_button_clicked(GtkButton* button, DownloadsDialog* self)
{
    // Close the popup to avoid consuming the first click on the newly-created
    // confirmation modal dialog below
    //adw_dialog_close(ADW_DIALOG(self));

    adw_dialog_present(self->move_to_trash_dialog, GTK_WIDGET(self));
}

static void
on_context_menu_closed(GtkWidget* downloads_item_widget, gpointer user_data)
{
    // orion::log_verbose("context menu closed");
    // AdwDialog* self = ADW_DIALOG(gtk_widget_get_ancestor(downloads_item_widget, GTK_TYPE_DIALOG));
    // //gtk_popover_set_autohide(self, false);
    // //gtk_popover_popup(self);

    // auto* widget_weak = new orion::gtk::WeakRefWrapper(downloads_item_widget);
    // g_idle_add_once([](gpointer user_data)->void{
    //     auto* widget_weak = static_cast<orion::gtk::WeakRefWrapper*>(user_data);
    //     defer([widget_weak]{ delete widget_weak; });
    //     auto widget_strong = widget_weak->lock();
    //     if(widget_strong)
    //     {
    //         gtk_widget_grab_focus(GTK_WIDGET(widget_strong.get()));
    //     }
    // }, widget_weak);
    // gtk_widget_grab_focus(downloads_item_widget);
    // gtk_popover_set_autohide(self, true);
}


static void
on_clear_button_clicked(GtkButton* button, DownloadsDialog* self)
{
    if(self->downloads_listview)
    {
        GListModel* model = G_LIST_MODEL(gtk_list_view_get_model(self->downloads_listview));
        if(model)
        {
            uint n_items = g_list_model_get_n_items(model);
            std::vector<orion::DownloadId> ids{};
            for(uint i = 0; i < n_items; i++)
            {
                DownloadsItem* item = APP_DOWNLOADS_ITEM(g_list_model_get_item(model, i));
                defer([item]{ g_object_unref(item); });
                
                if(downloads_item_get_operation_state(item) != orion::DownloadOperationState::downloading)
                {
                    ids.push_back(downloads_item_get_id(item));
                }
            }
            orion::DeleteDownloadsQuery query{
                .ids = ids,
            };
            global_core_api->delete_downloads(query, [](orion::DeleteDownloadsReply reply){ VERIFY_LOG(reply.err); });
        }
    }
}

static void
on_file_trash_finish(GFile* file, GAsyncResult* result, gpointer data)
{
    GError* err = NULL;
    if(!g_file_trash_finish(file, result, &err))
    {
        defer([err]{ g_error_free(err); });
        orion::log_error("on_move_to_trash_dialog_finished(): failed to delete file");
        orion::log_error("%s", err->message);
    }
}

static void
on_move_to_trash_dialog_finished(MoveDownloadsToTrashDialog* move_to_trash_dialog, MoveDownloadsToTrashDialogResult result, DownloadsDialog* self)
{
    if(result == MoveDownloadsToTrashDialogResult::confirm)
    {
        GListModel* model = G_LIST_MODEL(gtk_list_view_get_model(self->downloads_listview));
        if(model)
        {
            uint n_items = g_list_model_get_n_items(model);
            std::vector<orion::DownloadId> downloads;
            for(uint i = 0; i<n_items; i++)
            {
                DownloadsItem* item = APP_DOWNLOADS_ITEM(g_list_model_get_item(model, i));
                defer([item]{ g_object_unref(item); });
                if(downloads_item_get_operation_state(item) != orion::DownloadOperationState::downloading)
                {
                    downloads.push_back(downloads_item_get_id(item));
                    std::filesystem::path path = downloads_item_get_downloads_path(item)/downloads_item_get_file_name(item);

                    GFile* file = g_file_new_for_path(path.c_str());
                    defer([file]{ g_object_unref(file); });
                    g_file_trash_async(file, G_PRIORITY_DEFAULT, NULL, GAsyncReadyCallback(on_file_trash_finish), NULL);
                }
            }
            if(n_items)
            {
                orion::DeleteDownloadsQuery query{
                    .ids = downloads,
                };
                global_core_api->delete_downloads(query, [](orion::DeleteDownloadsReply reply){ VERIFY_LOG(reply.err); });
            }
        }
    }
}

static void
downloads_dialog_bind_to_profile_id(DownloadsDialog* self, orion::ProfileId profile_id)
{
    orion::log_verbose("downloads_dialog_bind_to_profile_id()");
    //NOTE: this currently assumes that main window's profile id is constant,
    //meaning that we do not need to worry about multiple attempts at binding our list view to a downloads list
    orion::gtk::WeakRefWrapper self_weak(self);

    orion::GetDownloadIdsQuery query{};
    query.profile_id = profile_id;
    global_core_api->get_download_ids(query, [self_weak, profile_id](orion::GetDownloadIdsReply reply){
        VERIFY_LOG(reply.err);
        global_core_api->get_downloads(reply.ids, [self_weak, profile_id](orion::GetDownloadsReply reply){
            VERIFY_LOG(reply.err);
            auto self_strong = self_weak.lock();
            if(self_strong)
            {
                DownloadsDialog* self = DOWNLOADS_DIALOG(self_strong.get());

                if(self->cpp_data.downloads_observer != orion::ObserverId(0))
                {
                    global_core_api->downloads_change().remove_observer(self->cpp_data.downloads_observer);
                    self->cpp_data.downloads_observer = orion::ObserverId(0);
                }

                GListStore* store = g_list_store_new(downloads_item_get_type());
                for(auto& download_item : reply.downloads)
                {
                    DownloadsItem* item_object = downloads_item_new_from_dto(download_item);
                    defer([item_object]{ g_object_unref(item_object); });
                    g_list_store_insert_sorted(store, item_object, downloads_item_compare, NULL);
                }

                GtkNoSelection* selection = gtk_no_selection_new(G_LIST_MODEL(store));
                gtk_list_view_set_model(self->downloads_listview, GTK_SELECTION_MODEL(selection));
                g_object_notify_by_pspec(G_OBJECT(self), obj_properties[PROP_SHOW_LISTVIEW]);

                orion::gtk::WeakRefWrapper store_weak(store);
                self->cpp_data.downloads_observer = global_core_api->downloads_change().add_observer(
                [self_weak, store_weak, profile_id](const orion::DownloadsChange& change) {
                    auto store_strong = store_weak.lock();
                    auto self_strong = self_weak.lock();
                    if(store_strong)
                    {
                        GListStore* store = G_LIST_STORE(store_strong.get());
                        GListModel* model_iface = G_LIST_MODEL(store);
                        
                        for(auto& created : change.created)
                        {
                            if(created.profile_id == profile_id)
                            {
                                DownloadsItem* item_object = downloads_item_new_from_dto(created);
                                defer([item_object]{ g_object_unref(item_object); });
                                g_list_store_insert_sorted(store, item_object, downloads_item_compare, NULL);
                            }
                        }
                        for(auto& updated : change.updated)
                        {
                            if(updated.profile_id == profile_id)
                            {
                                uint n_items = g_list_model_get_n_items(model_iface);
                                uint n = 0;
                                for(; n<n_items; n++)
                                {
                                    DownloadsItem* item_object = APP_DOWNLOADS_ITEM(g_list_model_get_item(model_iface, n));
                                    defer([item_object]{ g_object_unref(item_object); });
                                    if(downloads_item_get_id(item_object) == updated.id)
                                    {
                                        downloads_item_update_from_dto(item_object, updated);
                                        break;
                                    }
                                    else
                                    {
                                    }
                                }
                                if(n == n_items)
                                {
                                    DownloadsItem* item_object = downloads_item_new_from_dto(updated);
                                    defer([item_object]{ g_object_unref(item_object); });
                                    g_list_store_insert_sorted(store, item_object, downloads_item_compare, NULL);
                                }
                            }
                        }
                        for(auto& deleted : change.deleted)
                        {
                            uint n_items = g_list_model_get_n_items(model_iface);
                            for(uint n = 0; n<n_items; n++)
                            {
                                DownloadsItem* item_object = APP_DOWNLOADS_ITEM(g_list_model_get_item(model_iface, n));
                                defer([item_object]{ g_object_unref(item_object); });
                                if(downloads_item_get_id(item_object) == deleted)
                                {
                                    g_list_store_remove(store, n);
                                    break;
                                }
                            }
                        }
                        if(self_strong)
                        {
                            g_object_notify_by_pspec(G_OBJECT(self_strong.get()), obj_properties[PROP_SHOW_LISTVIEW]);
                        }
                    }
                });
            }
        });
    });
}

static void
downloads_dialog_root(GtkWidget* widget)
{
    //DownloadsDialog* self = DOWNLOADS_DIALOG(widget);
    GTK_WIDGET_CLASS(downloads_dialog_parent_class)->root(widget);
}


static void
downloads_dialog_set_property(GObject* object, guint prop_id, const GValue* val, GParamSpec* pspec)
{ 
    //DownloadsDialog* self = DOWNLOADS_DIALOG(object);

    switch(prop_id)
    {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}

static gboolean
invert_bool(DownloadsDialog* self, gboolean value)
{
    return !value;
}

static void
downloads_dialog_get_property(GObject* object, guint prop_id, GValue* val, GParamSpec* pspec)
{
    DownloadsDialog* self = DOWNLOADS_DIALOG(object);

    switch(prop_id)
    {
        case PROP_SHOW_LISTVIEW:
            {
                bool result = false;

                if(self->downloads_listview)
                {
                    GListModel* model = G_LIST_MODEL(gtk_list_view_get_model(self->downloads_listview));
                    if(model)
                    {
                        result = g_list_model_get_n_items(model);
                    }
                }

                g_value_set_boolean(val, result);
            }
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}


static void
downloads_dialog_dispose(GObject* object)
{
    DownloadsDialog* self = DOWNLOADS_DIALOG(object);
    if(self->cpp_data.downloads_observer != orion::ObserverId(0))
    {
        global_core_api->downloads_change().remove_observer(self->cpp_data.downloads_observer);
        self->cpp_data.downloads_observer = orion::ObserverId(0);
    }
    if(self->move_to_trash_dialog)
    {
        adw_dialog_force_close(ADW_DIALOG(self->move_to_trash_dialog));
    }
    g_clear_object(&self->move_to_trash_dialog);
    if(self->profile_id_watch)
    {
        gtk_expression_watch_unwatch(self->profile_id_watch);
        self->profile_id_watch = NULL;
    }
    G_OBJECT_CLASS(downloads_dialog_parent_class)->dispose(object);
}

static void
downloads_dialog_finalize(GObject* object)
{
    DownloadsDialog* self = DOWNLOADS_DIALOG(object);
    std::destroy_at(&self->cpp_data);
    G_OBJECT_CLASS(downloads_dialog_parent_class)->finalize(object);
}

static void
on_root_profile_id_changed(gpointer user_data)
{
    DownloadsDialog* dialog = DOWNLOADS_DIALOG(user_data);
    GValue val = G_VALUE_INIT;
    if(gtk_expression_watch_evaluate(dialog->profile_id_watch, &val))
    {
        downloads_dialog_bind_to_profile_id(dialog, orion::ProfileId(g_value_get_int64(&val)));
    }
    g_value_unset(&val);
}

static void
downloads_dialog_init(DownloadsDialog* self)
{
    std::construct_at(&self->cpp_data);
    g_type_ensure(downloads_item_widget_get_type());
    //g_type_ensure(APP_TYPE_DIALOG_LISTVIEW_CONTAINER);
	gtk_widget_init_template(GTK_WIDGET(self));
    self->move_to_trash_dialog = g_object_ref_sink(move_downloads_to_trash_dialog_new());
    g_signal_connect(self->move_to_trash_dialog, "finished", G_CALLBACK(on_move_to_trash_dialog_finished), self);

    //when the source expression for property is set to null, it will automatically take the 'this' argument passed in gtk_expression_watch
    GtkExpression* root_expression = gtk_property_expression_new(GTK_TYPE_WIDGET, NULL, "root");
    GtkExpression* profile_id_expression = gtk_property_expression_new(MAIN_WINDOW_TYPE, root_expression, "profile-id");
    defer([profile_id_expression]{ gtk_expression_unref(profile_id_expression); });
    self->profile_id_watch = gtk_expression_watch(profile_id_expression, self, on_root_profile_id_changed, self, NULL);
}

static void
downloads_dialog_on_dialog_clicked(GtkWidget* box, gint n_press, gdouble x, gdouble y, GtkEventController* gesture)
{
    orion::log_verbose("clicked dialog");
    GtkWidget* dialog = gtk_event_controller_get_widget(gesture);
    graphene_point_t dialog_point{.x = (float)x, .y = (float)y}, box_point{};

    if(!gtk_widget_compute_point(dialog, box,  &dialog_point, &box_point))
    {
        orion::log_error("downloads_dialog_on_dialog_clicked: failed to compute point");
        return;
    }
    if(!gtk_widget_contains(box, box_point.x, box_point.y))
    {
        adw_dialog_close(ADW_DIALOG(dialog));
        gtk_gesture_set_state(GTK_GESTURE(gesture), GTK_EVENT_SEQUENCE_CLAIMED);
    }
}

static void
on_downloads_listview_activate(GtkListView* listview, uint pos, DownloadsDialog* self)
{
    GListModel* model = G_LIST_MODEL(gtk_list_view_get_model(listview));
    DownloadsItem* item = APP_DOWNLOADS_ITEM(g_list_model_get_item(model, pos));
    defer([item]{ g_object_unref(item); });
    gtk_widget_activate_action(GTK_WIDGET(self), "win.open-download", "x",
        downloads_item_get_id(item).value);
}

static void
downloads_dialog_class_init(DownloadsDialogClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);
    object_class->set_property = downloads_dialog_set_property;
    object_class->get_property = downloads_dialog_get_property;
    object_class->dispose = downloads_dialog_dispose;
    object_class->finalize = downloads_dialog_finalize;
    
    obj_properties[PROP_SHOW_LISTVIEW] = g_param_spec_boolean("show-listview", NULL, NULL,
        false, GParamFlags(G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));
    g_object_class_install_properties(object_class, N_PROPS, obj_properties);

    GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);
    widget_class->root = downloads_dialog_root;

    gtk_widget_class_set_template_from_resource(widget_class, GRESOURCE_PREFIX "/src/window/downloads_dialog.ui");
    gtk_widget_class_bind_template_child(widget_class, DownloadsDialog, downloads_listview);
    gtk_widget_class_bind_template_callback(widget_class, on_move_to_trash_button_clicked);
    gtk_widget_class_bind_template_callback(widget_class, on_clear_button_clicked);
    gtk_widget_class_bind_template_callback(widget_class, on_context_menu_closed);
    gtk_widget_class_bind_template_callback(widget_class, on_downloads_listview_activate);
    gtk_widget_class_bind_template_callback(widget_class, invert_bool);
    gtk_widget_class_bind_template_callback(widget_class, downloads_dialog_on_dialog_clicked);
}
