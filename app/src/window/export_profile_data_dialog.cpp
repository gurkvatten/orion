#include <gtk/gtk.h>

#include <memory>
#include <sstream>
#include <string>

#include "app.h"
#include "export_profile_data_dialog.h"
#include <core/sync/sync_service.h>

namespace
{
    struct ExportProfileDataDialogPrivate
    {
        std::string file_name;
        orion::ProfileId profile_id{};
        std::shared_ptr<orion::SyncService> sync_service;
        orion::SyncService::OperationHandle operation_handle;
        bool export_running = false;
        bool cancel_requested = false;
    };
}

struct _ExportProfileDataDialog
{
    AdwDialog parent;

    GtkTextView* log_text_view;
    GtkButton* ok_button;
    GtkButton* cancel_button;
    ExportProfileDataDialogPrivate private_data;
};

G_DEFINE_TYPE(ExportProfileDataDialog, export_profile_data_dialog, ADW_TYPE_DIALOG)

static void
append_log_line(ExportProfileDataDialog* self, const std::string& line)
{
    GtkTextBuffer* buffer = gtk_text_view_get_buffer(self->log_text_view);
    GtkTextIter end_iter;
    gtk_text_buffer_get_end_iter(buffer, &end_iter);
    gtk_text_buffer_insert(buffer, &end_iter, line.c_str(), -1);
    gtk_text_buffer_insert(buffer, &end_iter, "\n", 1);

    g_idle_add_once([](gpointer data){
        ExportProfileDataDialog* self = APP_EXPORT_PROFILE_DATA_DIALOG(data);
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
export_profile_data_dialog_new()
{
    return ADW_DIALOG(g_object_new(EXPORT_PROFILE_DATA_DIALOG_TYPE, NULL));
}

AdwDialog*
export_profile_data_dialog_new_for_file_name(const char* file_name, orion::ProfileId profile_id)
{
    ExportProfileDataDialog* self = APP_EXPORT_PROFILE_DATA_DIALOG(export_profile_data_dialog_new());
    self->private_data.file_name = file_name ? file_name : "";
    self->private_data.profile_id = profile_id;

    GtkTextBuffer* buffer = gtk_text_view_get_buffer(self->log_text_view);
    gtk_text_buffer_set_text(buffer, "", -1);

    return ADW_DIALOG(self);
}

static void
on_cancel_clicked(GtkButton* button, gpointer user_data)
{
    ExportProfileDataDialog* self = APP_EXPORT_PROFILE_DATA_DIALOG(user_data);
    if (self->private_data.export_running && self->private_data.sync_service) {
        if (!self->private_data.cancel_requested) {
            self->private_data.cancel_requested = true;
            append_log_line(self, "Status: Cancelling export...");
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
export_profile_data_dialog_finalize(GObject* object)
{
    ExportProfileDataDialog* self = APP_EXPORT_PROFILE_DATA_DIALOG(object);

    if (self->private_data.operation_handle) {
        self->private_data.operation_handle->cancel();
    }
    std::destroy_at(&self->private_data);

    G_OBJECT_CLASS(export_profile_data_dialog_parent_class)->finalize(object);
}

static void
export_to(ExportProfileDataDialog* self, orion::ProfileId profile_id, const std::string& directory, orion::SyncService::ProgressHandler progress_handler, orion::SyncService::ExportToCompletionHandler reply)
{
    if (!self->private_data.sync_service) {
        reply(MAKE_ERROR(orion::ErrorCode::invalid_state, "Sync service is not initialized"));
        return;
    }
    append_log_line(self, "Preparing profile export");
    append_log_line(self, std::string("Selected destination: ") + (directory.empty() ? "<unknown>" : directory));
    append_log_line(self, "Status: Waiting for export...");
    self->private_data.operation_handle = self->private_data.sync_service->export_to(profile_id, directory, progress_handler, reply);
    if (!self->private_data.operation_handle) {
        reply(MAKE_ERROR(orion::ErrorCode::invalid_state, "Failed to start export"));
        return;
    }
    if (self->private_data.cancel_requested) {
        self->private_data.operation_handle->cancel();
    }
}

static void
export_to_progress(ExportProfileDataDialog* self, int done, int total, const std::string& description)
{
    std::stringstream ss;
    ss << "[" << done << "/" << total << "] " << description;
    append_log_line(self, ss.str());
}

static detached_task
export_task(ExportProfileDataDialog* self)
{
    self->private_data.sync_service = global_core_api->make_sync_service();
    self->private_data.operation_handle.reset();
    self->private_data.export_running = true;
    self->private_data.cancel_requested = false;

    gtk_widget_set_sensitive(GTK_WIDGET(self->ok_button), false);
    gtk_widget_set_sensitive(GTK_WIDGET(self->cancel_button), true);
    orion::gtk::WeakRefWrapper weak_ref_self(self);
    auto progress_cb = [weak_ref_self](int done, int total, const std::string& description){
        auto ref_self = weak_ref_self.lock();
        if(!ref_self) {
            return;
        }
        ExportProfileDataDialog* self = APP_EXPORT_PROFILE_DATA_DIALOG(ref_self.get());
        export_to_progress(self, done, total, description);
    };
    orion::Error result = co_await async(
        export_to, self, self->private_data.profile_id, self->private_data.file_name, progress_cb
    );
    self->private_data.operation_handle.reset();
    self->private_data.export_running = false;
    if(result == orion::Error::ok()) {
        append_log_line(self, "Status: Export successful");
        gtk_widget_set_sensitive(GTK_WIDGET(self->ok_button), true);
        gtk_widget_set_sensitive(GTK_WIDGET(self->cancel_button), false);
    }
    else if (result.code() == orion::ErrorCode::cancelled) {
        append_log_line(self, "Status: Export cancelled");
        gtk_widget_set_sensitive(GTK_WIDGET(self->ok_button), true);
        gtk_widget_set_sensitive(GTK_WIDGET(self->cancel_button), false);
    }
    else {
        append_log_line(self, "Status: Export failed with: " + result.message());
        gtk_widget_set_sensitive(GTK_WIDGET(self->ok_button), true);
        gtk_widget_set_sensitive(GTK_WIDGET(self->cancel_button), false);
    }
}

static void
export_profile_data_dialog_attached(GtkWidget* widget)
{
    GTK_WIDGET_CLASS(export_profile_data_dialog_parent_class)->root(widget);

    ExportProfileDataDialog* self = APP_EXPORT_PROFILE_DATA_DIALOG(widget);

    export_task(self);
}

static void
export_profile_data_dialog_detached(GtkWidget* widget)
{
    GTK_WIDGET_CLASS(export_profile_data_dialog_parent_class)->unroot(widget);
}

static void
export_profile_data_dialog_init(ExportProfileDataDialog* self)
{
    std::construct_at(&self->private_data);

    gtk_widget_init_template(GTK_WIDGET(self));
}

static void
export_profile_data_dialog_class_init(ExportProfileDataDialogClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);
    object_class->finalize = export_profile_data_dialog_finalize;

    GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);
    widget_class->root = export_profile_data_dialog_attached;
    widget_class->unroot = export_profile_data_dialog_detached;
    gtk_widget_class_set_template_from_resource(widget_class, GRESOURCE_PREFIX "/src/window/export_profile_data_dialog.ui");

    gtk_widget_class_bind_template_child(widget_class, ExportProfileDataDialog, log_text_view);
    gtk_widget_class_bind_template_child(widget_class, ExportProfileDataDialog, ok_button);
    gtk_widget_class_bind_template_child(widget_class, ExportProfileDataDialog, cancel_button);

    gtk_widget_class_bind_template_callback(widget_class, on_cancel_clicked);
    gtk_widget_class_bind_template_callback(widget_class, on_ok_clicked);
}
