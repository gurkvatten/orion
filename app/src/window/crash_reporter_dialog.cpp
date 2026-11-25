#include "crash_reporter_dialog.h"

struct _CrashReporterDialog
{
	AdwDialog parent;
    GtkTextView* details_text_view;
};

G_DEFINE_TYPE(CrashReporterDialog, crash_reporter_dialog, ADW_TYPE_DIALOG)

AdwDialog*
crash_reporter_dialog_new()
{
	return ADW_DIALOG(g_object_new(CRASH_REPORTER_DIALOG_TYPE, NULL));
}

static void
on_dont_send_clicked(GtkButton* button, gpointer user_data)
{
    adw_dialog_close(ADW_DIALOG(user_data));
}

static void
on_send_crash_log_clicked(GtkButton* button, gpointer user_data)
{
    adw_dialog_close(ADW_DIALOG(user_data));
}

static void
crash_reporter_dialog_init(CrashReporterDialog* self)
{
	gtk_widget_init_template(GTK_WIDGET(self));

    GtkTextBuffer* buffer = gtk_text_view_get_buffer(self->details_text_view);
    gtk_text_buffer_set_text(buffer, "{\"app_name\":\"Orion\",\"timestamp\":\"2025-04-04 16:44:39.00+02:00\"}", -1);
}

static void
crash_reporter_dialog_class_init(CrashReporterDialogClass* klass)
{
    GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);
	gtk_widget_class_set_template_from_resource(widget_class, GRESOURCE_PREFIX "/src/window/crash_reporter_dialog.ui");
    gtk_widget_class_bind_template_child(widget_class, CrashReporterDialog, details_text_view);
    gtk_widget_class_bind_template_callback(widget_class, on_dont_send_clicked);
    gtk_widget_class_bind_template_callback(widget_class, on_send_crash_log_clicked);
}
