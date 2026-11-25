#include <gtk/gtk.h>
#include <iostream>
#include <sstream>
#include <common/src/log.h>

#include "app.h"
#include "check_for_updates_dialog.h"
#include "software_update_dialog.h"
#include "util/dialog_util.h"

struct _CheckForUpdatesDialog
{
	AdwDialog parent;

	GtkProgressBar* progress_bar;
	guint progress_timer_id;
};

G_DEFINE_TYPE(CheckForUpdatesDialog, check_for_updates_dialog, ADW_TYPE_DIALOG)

AdwDialog*
check_for_updates_dialog_new()
{
	AdwDialog* dialog = ADW_DIALOG(g_object_new(CHECK_FOR_UPDATES_DIALOG_TYPE, NULL));
	orion::log_debug("check_for_updates: %p", dialog);
	return dialog;
}

static gboolean
on_progress_bar_timer(CheckForUpdatesDialog* win)
{
	gtk_progress_bar_pulse(win->progress_bar);

	return true;
}

static void
on_destroy(CheckForUpdatesDialog* win)
{
	orion::log_info("check_for_updates: on_destroy()");

	// Stop the periodic timer
	g_source_remove(win->progress_timer_id);
}

static void
check_for_updates_dialog_init(CheckForUpdatesDialog* win)
{
	gtk_widget_init_template(GTK_WIDGET(win));

	g_signal_connect(win, "destroy", G_CALLBACK(on_destroy), nullptr);

	// Start the periodic timer
	win->progress_timer_id = g_timeout_add(100, G_SOURCE_FUNC(on_progress_bar_timer), win);
}

static void
on_cancel_clicked(GtkButton* button, gpointer user_data)
{
	orion::log_info("check_for_updates: on_cancel_clicked()");

	adw_dialog_close(ADW_DIALOG(user_data));

	adw_dialog_present(software_update_dialog_new(), GTK_WIDGET(orion::gtk::get_parent_window(GTK_WIDGET(user_data))));
}

static void
check_for_updates_dialog_class_init(CheckForUpdatesDialogClass* klass)
{
	gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass), GRESOURCE_PREFIX "/src/window/check_for_updates_dialog.ui");

	gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), CheckForUpdatesDialog, progress_bar);

	gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), on_cancel_clicked);
}
