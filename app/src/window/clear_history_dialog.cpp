#include <gdk/gdk.h>

#include "../app.h"
#include "clear_history_dialog.h"
#include "window/main_window/main_window.h"

struct _ClearHistoryDialog
{
	AdwDialog parent;
};

G_DEFINE_TYPE(ClearHistoryDialog, clear_history_dialog, ADW_TYPE_DIALOG)

AdwDialog*
clear_history_dialog_new()
{
	return ADW_DIALOG(g_object_new(CLEAR_HISTORY_DIALOG_TYPE, NULL));
}

static void
on_clear_clicked(GtkButton* button, gpointer user_data)
{

    const orion::ProfileHistoryQuery query{
        .profile_id = main_window_get_current_profile_id(MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(button)))),
    };
    adw_dialog_close(ADW_DIALOG(user_data));
    
    global_core_api->clear_history_entries(query, [](orion::Error err){
        VERIFY_LOG(err);
    });

}

static void
on_cancel_clicked(GtkButton* button, gpointer user_data)
{
    adw_dialog_close(ADW_DIALOG(user_data));
}

static void
clear_history_dialog_init(ClearHistoryDialog* win)
{
	gtk_widget_init_template(GTK_WIDGET(win));
}

static void
clear_history_dialog_class_init(ClearHistoryDialogClass* klass)
{
    GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);
	gtk_widget_class_set_template_from_resource(widget_class, GRESOURCE_PREFIX "/src/window/clear_history_dialog.ui");
    gtk_widget_class_bind_template_callback(widget_class, on_clear_clicked);
    gtk_widget_class_bind_template_callback(widget_class, on_cancel_clicked);
}
