#include <gtk/gtk.h>

#include <memory>
#include <sstream>
#include <string>

#include "app.h"
#include "import_profile_data_dialog.h"
#include <core/sync/sync_service.h>

namespace
{
    struct ImportProfileDataDialogPrivate
    {
        std::string file_name;
        orion::ProfileId profile_id{};
        std::shared_ptr<orion::SyncService> sync_service;
        orion::SyncService::OperationHandle operation_handle;
        bool import_running = false;
        bool cancel_requested = false;
    };
}

struct _ImportProfileDataDialog
{
    AdwDialog parent;

    GtkTextView* log_text_view;
    GtkButton* ok_button;
    GtkButton* cancel_button;
    ImportProfileDataDialogPrivate private_data;
};

G_DEFINE_TYPE(ImportProfileDataDialog, import_profile_data_dialog, ADW_TYPE_DIALOG)

static void
append_log_line(ImportProfileDataDialog* self, const std::string& line)
{
    GtkTextBuffer* buffer = gtk_text_view_get_buffer(self->log_text_view);
    GtkTextIter end_iter;
    gtk_text_buffer_get_end_iter(buffer, &end_iter);
    gtk_text_buffer_insert(buffer, &end_iter, line.c_str(), -1);
    gtk_text_buffer_insert(buffer, &end_iter, "\n", 1);

    g_idle_add_once([](gpointer data){
        ImportProfileDataDialog* self = APP_IMPORT_PROFILE_DATA_DIALOG(data);
        if (!gtk_widget_get_root(GTK_WIDGET(self))) {
            g_object_unref(self);
            return;
        }
        GtkTextBuffer* buffer = gtk_text_view_get_buffer(self->log_text_view);
        GtkTextIter end_iter;
        gtk_text_buffer_get_end_iter(buffer, &end_iter);
        gtk_text_view_scroll_to_iter(self->log_text_view, &end_iter, 0.0, FALSE, 0.0, 1.0);
        g_object_unref(self);
    }, g_object_ref(self));
}

AdwDialog*
import_profile_data_dialog_new()
{
    return ADW_DIALOG(g_object_new(IMPORT_PROFILE_DATA_DIALOG_TYPE, NULL));
}

AdwDialog*
import_profile_data_dialog_new_for_file_name(const char* file_name, orion::ProfileId profile_id)
{
    ImportProfileDataDialog* self = APP_IMPORT_PROFILE_DATA_DIALOG(import_profile_data_dialog_new());
    self->private_data.file_name = file_name ? file_name : "";
    self->private_data.profile_id = profile_id;

    GtkTextBuffer* buffer = gtk_text_view_get_buffer(self->log_text_view);
    gtk_text_buffer_set_text(buffer, "", -1);

    return ADW_DIALOG(self);
}

static void
on_cancel_clicked(GtkButton* button, gpointer user_data)
{
    ImportProfileDataDialog* self = APP_IMPORT_PROFILE_DATA_DIALOG(user_data);
    if (self->private_data.import_running && self->private_data.sync_service) {
        if (!self->private_data.cancel_requested) {
            self->private_data.cancel_requested = true;
            append_log_line(self, "Status: Cancelling import...");
            if (self->private_data.operation_handle) {
                self->private_data.operation_handle->cancel();
            }
        }
        gtk_widget_set_sensitive(GTK_WIDGET(self->cancel_button), false);
        return;
    }
    adw_dialog_close(ADW_DIALOG(self));
}

static void
on_ok_clicked(GtkButton* button, gpointer user_data)
{
    adw_dialog_close(ADW_DIALOG(user_data));
}

static void
import_profile_data_dialog_finalize(GObject* object)
{
    ImportProfileDataDialog* self = APP_IMPORT_PROFILE_DATA_DIALOG(object);

    if (self->private_data.operation_handle) {
        self->private_data.operation_handle->cancel();
    }
    std::destroy_at(&self->private_data);

    G_OBJECT_CLASS(import_profile_data_dialog_parent_class)->finalize(object);
}

static void
import_from(ImportProfileDataDialog* self, orion::ProfileId profile_id, const std::string& source, orion::SyncService::ProgressHandler progress_handler, orion::SyncService::ImportFromCompletionHandler reply)
{
    if (!self->private_data.sync_service) {
        reply(MAKE_ERROR(orion::ErrorCode::invalid_state, "Sync service is not initialized"));
        return;
    }
    append_log_line(self, "Preparing profile import");
    append_log_line(self, std::string("Selected source: ") + (source.empty() ? "<unknown>" : source));
    append_log_line(self, "Status: Waiting for import...");
    self->private_data.operation_handle = self->private_data.sync_service->import_from(profile_id, source, progress_handler, reply);
    if (!self->private_data.operation_handle) {
        reply(MAKE_ERROR(orion::ErrorCode::invalid_state, "Failed to start import"));
        return;
    }
    if (self->private_data.cancel_requested) {
        self->private_data.operation_handle->cancel();
    }
}

static void
import_from_progress(ImportProfileDataDialog* self, int done, int total, const std::string& description)
{
    std::stringstream ss;
    ss << "[" << done << "/" << total << "] " << description;
    append_log_line(self, ss.str());
}

static detached_task
import_task(ImportProfileDataDialog* self)
{
    self->private_data.sync_service = global_core_api->make_sync_service();
    self->private_data.operation_handle.reset();
    self->private_data.import_running = true;
    self->private_data.cancel_requested = false;

    gtk_widget_set_sensitive(GTK_WIDGET(self->ok_button), false);
    gtk_widget_set_sensitive(GTK_WIDGET(self->cancel_button), true);
    orion::gtk::WeakRefWrapper weak_ref_self(self);
    auto progress_cb = [weak_ref_self](int done, int total, const std::string& description){
        auto ref_self = weak_ref_self.lock();
        if(!ref_self) {
            return;
        }
        ImportProfileDataDialog* self = APP_IMPORT_PROFILE_DATA_DIALOG(ref_self.get());
        import_from_progress(self, done, total, description);
    };
    orion::Error result = co_await async(
        import_from, self, self->private_data.profile_id, self->private_data.file_name, progress_cb
    );
    self->private_data.operation_handle.reset();
    self->private_data.import_running = false;
    if(result == orion::Error::ok()) {
        append_log_line(self, "Status: Import successful");
        gtk_widget_set_sensitive(GTK_WIDGET(self->ok_button), true);
        gtk_widget_set_sensitive(GTK_WIDGET(self->cancel_button), false);
    }
    else if (result.code() == orion::ErrorCode::cancelled) {
        append_log_line(self, "Status: Import cancelled");
        gtk_widget_set_sensitive(GTK_WIDGET(self->ok_button), true);
        gtk_widget_set_sensitive(GTK_WIDGET(self->cancel_button), false);
    }
    else {
        append_log_line(self, "Status: Import failed with: " + result.message());
        gtk_widget_set_sensitive(GTK_WIDGET(self->ok_button), true);
        gtk_widget_set_sensitive(GTK_WIDGET(self->cancel_button), false);
    }
}

static void
import_profile_data_dialog_attached(GtkWidget* widget)
{
    GTK_WIDGET_CLASS(import_profile_data_dialog_parent_class)->root(widget);

    ImportProfileDataDialog* self = APP_IMPORT_PROFILE_DATA_DIALOG(widget);

    import_task(self);
}

static void
import_profile_data_dialog_detached(GtkWidget* widget)
{
    GTK_WIDGET_CLASS(import_profile_data_dialog_parent_class)->unroot(widget);
}

static void
import_profile_data_dialog_init(ImportProfileDataDialog* self)
{
    std::construct_at(&self->private_data);

    gtk_widget_init_template(GTK_WIDGET(self));
}

static void
import_profile_data_dialog_class_init(ImportProfileDataDialogClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);
    object_class->finalize = import_profile_data_dialog_finalize;

    GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);
    widget_class->root = import_profile_data_dialog_attached;
    widget_class->unroot = import_profile_data_dialog_detached;
    gtk_widget_class_set_template_from_resource(widget_class, GRESOURCE_PREFIX "/src/window/import_profile_data_dialog.ui");

    gtk_widget_class_bind_template_child(widget_class, ImportProfileDataDialog, log_text_view);
    gtk_widget_class_bind_template_child(widget_class, ImportProfileDataDialog, ok_button);
    gtk_widget_class_bind_template_child(widget_class, ImportProfileDataDialog, cancel_button);

    gtk_widget_class_bind_template_callback(widget_class, on_cancel_clicked);
    gtk_widget_class_bind_template_callback(widget_class, on_ok_clicked);
}
