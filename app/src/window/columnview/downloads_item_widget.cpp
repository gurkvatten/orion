#define ENABLE_VERBOSE_LOGS 0

#include <gtk/gtk.h>

#include <common/src/defer.h>

#include "app.h"
#include "downloads_item_widget.h"
#include "data_item/downloads_item.h"
#include "util/menu_util.h"
#include "util/click_util.h"

struct _DownloadsItemWidget
{
	GtkBox parent;

    GtkImage* file_icon;
    GtkLabel* file_name_label;
    GtkLabel* file_size_label;

    GtkProgressBar* progress_bar;

    GtkPopoverMenu* context_menu;

    DownloadsItem* item;
};

struct _DownloadsItemWidgetClass
{
    GtkBoxClass* parent_class;
};

G_DEFINE_TYPE(DownloadsItemWidget, downloads_item_widget, GTK_TYPE_BOX)

enum {
    PROP_0,
    PROP_ITEM,
    N_PROPS,
};

enum {
    CONTEXT_MENU_CLOSED,
    N_SIGNALS,
};

static GParamSpec* obj_properties[N_PROPS] = { NULL, };
static guint obj_signals[N_SIGNALS]= {0};
//G_FILE_ATTRIBUTE_STANDARD_ICON "," G_FILE_ATTRIBUTE_STANDARD_SIZE

// static void
// get_file_info_for_file_path_async(std::string attributes, std::string path, std::function<void(const GFileInfo*)> cb)
// {
//     auto file_info_cb = [](GObject* object, GAsyncResult* result, gpointer user_data){
//         auto cb_ptr = static_cast<std::function<void(const GFileInfo*)>*>(user_data);
//         defer([cb_ptr]{ delete cb_ptr; });

//         GError* err = NULL;
//         GFileInfo_autoptr file_info = g_file_query_info_finish(G_FILE(object), result, &err);
//         if(err)
//         {
//             (*cb_ptr)(NULL);
//             orion::log_debug("%s", err->message);
//             g_clear_pointer(&err, g_error_free);
//         }
//         (*cb_ptr)(file_info);
//     };

//     GFile_autoptr file = g_file_new_for_path(path.c_str());
//     std::function<void(const GFileInfo*)>* cb_ptr = new std::function<void(const GFileInfo*)>(cb);
//     g_file_query_info_async(file, attributes.c_str(), G_FILE_QUERY_INFO_NONE, G_PRIORITY_DEFAULT, NULL,
//         file_info_cb, cb_ptr);
// }


static void
downloads_item_widget_on_secondary_released(GtkGestureClick* gesture, gint n_press, gdouble x, gdouble y, gpointer user_data)
{
    DownloadsItemWidget* self = APP_DOWNLOADS_ITEM_WIDGET(user_data);

    GMenu* menu = g_menu_new();
    defer([menu]{ g_object_unref(menu); });

    std::filesystem::path full_path = downloads_item_get_downloads_path(self->item);
    full_path /= downloads_item_get_file_name(self->item);

    orion::DownloadId id = downloads_item_get_id(self->item);

    switch(downloads_item_get_operation_state(self->item))
    {
        case orion::DownloadOperationState::finished:
            orion::gtk::menu_append_action_with_int64_target(menu, _("Open"), "win.open-download", id.value);
            orion::gtk::menu_append_action_with_string_target(menu, _("Show in Files"), "win.show-file-in-file-manager", full_path.c_str());
            orion::gtk::menu_append_action_with_string_target(menu, _("Copy Address"), "win.clipboard-copy", downloads_item_get_origin_uri(self->item).c_str());
            orion::gtk::menu_append_action_with_int64_target(menu, _("Remove from List"), "win.remove-download-item", id.value);
            break;
        case orion::DownloadOperationState::downloading:
            orion::gtk::menu_append_action_with_string_target(menu, _("Show in Files"), "win.show-file-in-file-manager", full_path.c_str());
            orion::gtk::menu_append_action_with_int64_target(menu, _("Stop"), "win.stop-download", id.value);
            orion::gtk::menu_append_action_with_string_target(menu, _("Copy Address"), "win.clipboard-copy", downloads_item_get_origin_uri(self->item).c_str());
            break;
        case orion::DownloadOperationState::paused:
            orion::gtk::menu_append_action_with_int64_target(menu, _("Resume"), "win.resume-download", id.value);
            orion::gtk::menu_append_action_with_string_target(menu, _("Copy Address"), "win.clipboard-copy", downloads_item_get_origin_uri(self->item).c_str());
            orion::gtk::menu_append_action_with_int64_target(menu, _("Remove from List"), "win.remove-download-item", id.value);
            break;
        case orion::DownloadOperationState::failed:
            orion::gtk::menu_append_action_with_int64_target(menu, _("Resume"), "win.resume-download", id.value);
            orion::gtk::menu_append_action_with_string_target(menu, _("Copy Address"), "win.clipboard-copy", downloads_item_get_origin_uri(self->item).c_str());
            orion::gtk::menu_append_action_with_int64_target(menu, _("Remove from List"), "win.remove-download-item", id.value);
            break;
        case orion::DownloadOperationState::cancelled:
            orion::gtk::menu_append_action_with_int64_target(menu, _("Restart"), "win.resume-download", id.value);
            orion::gtk::menu_append_action_with_string_target(menu, _("Copy Address"), "win.clipboard-copy", downloads_item_get_origin_uri(self->item).c_str());
            orion::gtk::menu_append_action_with_int64_target(menu, _("Remove from List"), "win.remove-download-item", id.value);
            break;
        case orion::DownloadOperationState::cancelling:
            //should not have a context menu, so this will just early return
            return;
    }

    gtk_popover_menu_set_menu_model(self->context_menu, G_MENU_MODEL(menu));
    orion::gtk::button_released_handler(GTK_WIDGET(self), gesture, x, y, self->context_menu, GTK_WIDGET(self));
}

static void
downloads_item_widget_on_context_menu_closed(GtkPopoverMenu* context_menu, gpointer user_data)
{
    g_signal_emit(user_data, obj_signals[CONTEXT_MENU_CLOSED], 0);
}

//the result of this is typecast to double in blueprint, but for some reason, it instead expects a float
//in other words, until this breaks (seems to be undefined behaviour), ignore the return type
static float
get_progress_bar_fraction(DownloadsItemWidget* self, int64_t total_size, int64_t current_size, int operation_state)
{
    orion::log_verbose("%li ts %li cs", total_size, current_size);
    if(total_size == 0)
    {
        if(orion::DownloadOperationState(operation_state) == orion::DownloadOperationState::downloading)
        {
            GtkProgressBar* progress_bar = self->progress_bar;

            guint timeout_id = gtk_widget_add_tick_callback(GTK_WIDGET(progress_bar), [](GtkWidget* widget, GdkFrameClock* clock, gpointer data) -> gboolean {
                    gtk_progress_bar_pulse(GTK_PROGRESS_BAR(widget));
                    return G_SOURCE_CONTINUE;
                }, NULL, NULL);

            // guint timeout_id = g_timeout_add_full(G_PRIORITY_HIGH_IDLE, 50, [](gpointer user_data){
            //     GtkProgressBar* progress_bar = GTK_PROGRESS_BAR(user_data);
            //     gtk_progress_bar_pulse(progress_bar);
            //     return (int)G_SOURCE_CONTINUE;
            // }, progress_bar, NULL);

            g_object_weak_ref(G_OBJECT(progress_bar), [](gpointer user_data, GObject* old_object){
                //g_source_remove(GPOINTER_TO_UINT(user_data));
                //gtk_widget_remove_tick_callback(GTK_WIDGET(old_object), GPOINTER_TO_UINT(user_data));
            }, GUINT_TO_POINTER(timeout_id));
            //TODO: improve this animation, might be much less broken outside of remote desktop so worth testing on bate metal
            gtk_progress_bar_pulse(progress_bar);
            return 0.0f;
        }
        else
        {
            return 1.0f;
        }
    }
    float unrefined_fraction = (double)current_size/(double)total_size; 
    float refined_fraction = std::clamp(unrefined_fraction, 0.0f, 1.0f);
    // orion::log_debug("ts: %" PRIi64 " cs: %" PRIi64 "rf: %lf", total_size, current_size, refined_fraction);
    // gtk_progress_bar_set_fraction(self->progress_bar, refined_fraction);

    return refined_fraction;
}

static gboolean
get_progress_bar_sensitive(DownloadsItemWidget* self, int operation_state_val)
{
    //intentionally avoiding a default case to force editing this upon any modification of the enum class
    auto operation_state = orion::DownloadOperationState(operation_state_val);
    switch(operation_state)
    {
        case orion::DownloadOperationState::paused:
            return false;
        case orion::DownloadOperationState::failed:
            return false;
        case orion::DownloadOperationState::finished:
            return true;
        case orion::DownloadOperationState::downloading:
            return true;
        case orion::DownloadOperationState::cancelling:
            return false;
        case orion::DownloadOperationState::cancelled:
            return false;
    }
    return false;
}

static gboolean
get_progress_bar_visible(DownloadsItemWidget* self, int operation_state_val)
{
    //intentionally avoiding a default case to force editing this upon any modification of the enum class
    auto operation_state = orion::DownloadOperationState(operation_state_val);
    switch(operation_state)
    {
        case orion::DownloadOperationState::paused:
            return true;
        case orion::DownloadOperationState::failed:
            return false;
        case orion::DownloadOperationState::finished:
            return false;
        case orion::DownloadOperationState::downloading:
            return true;
        case orion::DownloadOperationState::cancelling:
            return true;
        case orion::DownloadOperationState::cancelled:
            return false;
    }
    return false;
}

gchar*
get_download_eta_string(int64_t eta)
{
    if(eta >= 0)
    {
        int64_t seconds = eta%60;
        int64_t minutes = (eta/60)%60;
        int64_t hours = (eta/(60*60))%24;
        int64_t days = eta/(60*60*24);


        //NOTE: this is a purely arbitrary system that does 
        if (days >= 5) {
            return g_strdup_printf(_("%ldd"), days);
        } else if (days > 0) {
            return g_strdup_printf(_("%ld %02ldh"), days, hours);
        } 
        
        else if (hours >= 5) {
            return g_strdup_printf(_("%ldh"), hours);
        } else if (hours > 0) {
            return g_strdup_printf(_("%ldh %02ldm"), hours, minutes);
        } 
        
        else if (minutes >= 5) {
            return g_strdup_printf(_("%ldm"), minutes);
        } else if (minutes > 0) {
            return g_strdup_printf(_("%ldm %02lds"), minutes, seconds);
        } 
        
        else {
            return g_strdup_printf(_("%lds"), seconds);
        }
    }
    else
    {
        return g_strdup(_("Unknown time"));
    }
}

static gchar*
get_size_label(DownloadsItemWidget* self, int64_t total_size, int64_t current_size, int64_t download_speed, int operation_state_val)
{
    auto operation_state = orion::DownloadOperationState(operation_state_val);

    gchar* total_size_string = g_format_size(total_size);
    defer([total_size_string]{ g_free(total_size_string); });
    gchar* current_size_string = g_format_size(current_size);
    defer([current_size_string]{ g_free(current_size_string); });
    gchar* download_speed_string = g_format_size(download_speed);
    defer([download_speed_string]{ g_free(download_speed_string); });

    switch(operation_state)
    {
        case orion::DownloadOperationState::paused:
            return g_strdup_printf(_("Paused: %s of %s"), current_size_string, total_size_string);
        case orion::DownloadOperationState::failed:
            return g_strdup_printf(_("Failed: %s of %s"), current_size_string, total_size_string);
        case orion::DownloadOperationState::finished:
            return g_strdup_printf(_("Finished: %s"), total_size_string);
        case orion::DownloadOperationState::downloading:
            {
                int64_t eta;
                if(download_speed)
                {
                    eta = (total_size-current_size)/download_speed;
                }
                else
                {
                    eta = -1;
                }
                gchar* eta_string = get_download_eta_string(eta);
                defer([eta_string]{ g_free(eta_string); });
                return g_strdup_printf("%s: %s of %s, %s/s", eta_string, current_size_string, total_size_string, download_speed_string);
            }
        case orion::DownloadOperationState::cancelled:
            return g_strdup_printf(_("Cancelled"));
        case orion::DownloadOperationState::cancelling:
            return g_strdup_printf(_("Cancelling: %s of %s"), current_size_string, total_size_string);
    }
    return NULL;
}

static void
on_show_in_files_button_clicked(GtkButton* button, DownloadsItemWidget* self)
{
    std::filesystem::path path = downloads_item_get_downloads_path(self->item)/downloads_item_get_file_name(self->item);
    
    orion::log_verbose("on_show_in_files_button_clicked(): attempting to show for path %s", path.c_str());

    GVariant* parameter = g_variant_new_string(path.c_str());
    //fun fact: glib is horrible, we get a floating ref, gtk_widget_activate_action_variant only sinks this on success,
    //meaning that we'd still need to defensively do memory management here in case it is unsuccessful, better to just sink the ref
    g_variant_ref_sink(parameter);
    defer([parameter]{ g_variant_unref(parameter); });
    gtk_widget_activate_action_variant(GTK_WIDGET(self), "win.show-file-in-file-manager", parameter);
}

static void
downloads_item_widget_init(DownloadsItemWidget* win)
{
	gtk_widget_init_template(GTK_WIDGET(win));
}

static void
downloads_item_widget_dispose(GObject* object)
{
    DownloadsItemWidget* self = APP_DOWNLOADS_ITEM_WIDGET(object);
    g_clear_object(&self->item);
    G_OBJECT_CLASS(downloads_item_widget_parent_class)->dispose(object);
}

static void
downloads_item_widget_get_property(GObject* object, guint property_id, GValue* val, GParamSpec* pspec)
{
    DownloadsItemWidget* self = APP_DOWNLOADS_ITEM_WIDGET(object);

    switch(property_id)
    {
        case PROP_ITEM:
            g_value_set_object(val, self->item);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
downloads_item_widget_set_item(DownloadsItemWidget* self, DownloadsItem* item)
{
    if(self->item != item)
    {
        g_clear_object(&self->item);
        if(item)
        {
            self->item = g_object_ref(item);
        }
        g_object_notify_by_pspec(G_OBJECT(self), obj_properties[PROP_ITEM]);
    }
}

static void
downloads_item_widget_set_property(GObject* object, guint property_id, const GValue* val, GParamSpec* pspec)
{
    DownloadsItemWidget* self = APP_DOWNLOADS_ITEM_WIDGET(object);

    switch(property_id)
    {
        case PROP_ITEM:
            downloads_item_widget_set_item(self, APP_DOWNLOADS_ITEM(g_value_get_object(val)));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
downloads_item_widget_class_init(DownloadsItemWidgetClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);
    object_class->get_property = downloads_item_widget_get_property;
    object_class->set_property = downloads_item_widget_set_property;
    object_class->dispose = downloads_item_widget_dispose;

    obj_properties[PROP_ITEM] = g_param_spec_object("item", NULL, NULL, APP_TYPE_DOWNLOADS_ITEM,
        GParamFlags(G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY));
    g_object_class_install_properties(object_class, N_PROPS, obj_properties);

    obj_signals[CONTEXT_MENU_CLOSED] = g_signal_new("context-menu-closed", G_OBJECT_CLASS_TYPE(object_class), GSignalFlags(0), 0, NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

    GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);
    gtk_widget_class_set_template_from_resource(widget_class, GRESOURCE_PREFIX "/src/window/columnview/downloads_item_widget.ui");
    gtk_widget_class_bind_template_child(widget_class, DownloadsItemWidget, file_icon);
    gtk_widget_class_bind_template_child(widget_class, DownloadsItemWidget, file_name_label);
    gtk_widget_class_bind_template_child(widget_class, DownloadsItemWidget, file_size_label);
    gtk_widget_class_bind_template_child(widget_class, DownloadsItemWidget, progress_bar);
    gtk_widget_class_bind_template_child(widget_class, DownloadsItemWidget, context_menu);
    gtk_widget_class_bind_template_callback(widget_class, on_show_in_files_button_clicked);
    gtk_widget_class_bind_template_callback(widget_class, get_progress_bar_fraction);
    gtk_widget_class_bind_template_callback(widget_class, get_progress_bar_sensitive);
    gtk_widget_class_bind_template_callback(widget_class, get_progress_bar_visible);
    gtk_widget_class_bind_template_callback(widget_class, get_size_label); //TODO: add state, ETA, estimated download speed
    gtk_widget_class_bind_template_callback(widget_class, downloads_item_widget_on_secondary_released);
    gtk_widget_class_bind_template_callback(widget_class, downloads_item_widget_on_context_menu_closed);
}
