#include <gtk/gtk.h>

#include "app.h"
#include "move_downloads_to_trash_dialog.h"

struct _MoveDownloadsToTrashDialog
{
	AdwDialog parent;
};

G_DEFINE_TYPE(MoveDownloadsToTrashDialog, move_downloads_to_trash_dialog, ADW_TYPE_DIALOG)

enum {
    SIGNAL_FINISHED,
    N_SIGNALS,
};

guint obj_signals[N_SIGNALS];

static void
on_cancel_button_clicked(GtkButton* button, MoveDownloadsToTrashDialog* win)
{
    g_signal_emit(win, obj_signals[SIGNAL_FINISHED], 0, MoveDownloadsToTrashDialogResult::cancel);
	adw_dialog_close(ADW_DIALOG(win));
}

static void
on_confirm_button_clicked(GtkButton* button, MoveDownloadsToTrashDialog* win)
{
    g_signal_emit(win, obj_signals[SIGNAL_FINISHED], 0, MoveDownloadsToTrashDialogResult::confirm);
	adw_dialog_close(ADW_DIALOG(win));
}

AdwDialog*
move_downloads_to_trash_dialog_new()
{
	return ADW_DIALOG(g_object_new(MOVE_DOWNLOADS_TO_TRASH_DIALOG_TYPE, NULL));
}

static void
move_downloads_to_trash_dialog_init(MoveDownloadsToTrashDialog* win)
{
	gtk_widget_init_template(GTK_WIDGET(win));
}

static void
move_downloads_to_trash_dialog_class_init(MoveDownloadsToTrashDialogClass* klass)
{
    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass), GRESOURCE_PREFIX "/src/window/move_downloads_to_trash_dialog.ui");

    obj_signals[SIGNAL_FINISHED] = g_signal_new("finished", MOVE_DOWNLOADS_TO_TRASH_DIALOG_TYPE, GSignalFlags(0), 0, NULL, NULL,
        g_cclosure_marshal_generic, G_TYPE_NONE, 1, G_TYPE_INT);

	gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), on_cancel_button_clicked);
	gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), on_confirm_button_clicked);
}
