#include <gtk/gtk.h>
#include <iostream>
#include <sstream>
#include <common/src/log.h>

#include "app.h"
#include "updating_orion_dialog.h"
#include "up_to_date_dialog.h"
#include "util/dialog_util.h"

struct _UpdatingOrionDialog
{
	AdwDialog parent;

	GtkProgressBar* progress_bar;
	guint progress_timer_id;
	double fraction_downloaded;
};

G_DEFINE_TYPE(UpdatingOrionDialog, updating_orion_dialog, ADW_TYPE_DIALOG)

AdwDialog*
updating_orion_dialog_new()
{
	return ADW_DIALOG(g_object_new(UPDATING_ORION_DIALOG_TYPE, NULL));
}

static gboolean
on_progress_bar_timer(UpdatingOrionDialog* win)
{
	gtk_progress_bar_set_fraction(win->progress_bar, win->fraction_downloaded);

	if (win->fraction_downloaded < 1.0) {
		win->fraction_downloaded += 0.01;
	}

	return true;
}

static void
on_destroy(UpdatingOrionDialog* win)
{
	orion::log_info("updating_orion: on_destroy()");

	// Stop the periodic timer
	g_source_remove(win->progress_timer_id);
}

static void
updating_orion_dialog_init(UpdatingOrionDialog* win)
{
	gtk_widget_init_template(GTK_WIDGET(win));

	g_signal_connect(win, "destroy", G_CALLBACK(on_destroy), nullptr);

	// Start the periodic timer
	win->progress_timer_id = g_timeout_add(100, G_SOURCE_FUNC(on_progress_bar_timer), win);
	win->fraction_downloaded = 0.0;
}

static void
on_cancel_clicked(GtkButton* button, gpointer user_data)
{
	orion::log_info("updating_orion: on_cancel_clicked()");

	adw_dialog_close(ADW_DIALOG(user_data));

	adw_dialog_present(up_to_date_dialog_new(), GTK_WIDGET(orion::gtk::get_parent_window(GTK_WIDGET(user_data))));
}

static void
updating_orion_dialog_class_init(UpdatingOrionDialogClass* klass)
{
	gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass), GRESOURCE_PREFIX "/src/window/updating_orion_dialog.ui");

	gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), UpdatingOrionDialog, progress_bar);

	gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), on_cancel_clicked);
}
