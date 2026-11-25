#include <gdk/gdk.h>
#include <common/src/log.h>

#include "app.h"
#include "low_power_mode_dialog.h"
#include "window/main_window/main_window.h"
#include <core/settings.h>

struct _LowPowerModeDialog
{
	AdwDialog parent;
};

G_DEFINE_TYPE(LowPowerModeDialog, low_power_mode_dialog, ADW_TYPE_DIALOG)

AdwDialog*
low_power_mode_dialog_new()
{
	return ADW_DIALOG(g_object_new(LOW_POWER_MODE_DIALOG_TYPE, NULL));
}

static void
low_power_mode_dialog_init(LowPowerModeDialog* win)
{
	gtk_widget_init_template(GTK_WIDGET(win));
}

static void
on_enable_button_clicked(GtkButton* button, gpointer user_data)
{
	orion::log_debug("low_power_mode: on_enable_button_clicked");

    GtkRoot* root = gtk_widget_get_root(GTK_WIDGET(button));
    if (!root || !MAIN_IS_WINDOW(root)) {
        orion::log_error("low_power_mode: on_enable_button_clicked: failed to get MainWindow");
        return;
    }

    MainWindow* win = MAIN_WINDOW(root);
    orion::ProfileId profile_id = main_window_get_current_profile_id(win);
    LowPowerModeDialog* dialog = LOW_POWER_MODE_DIALOG(user_data);
    orion::gtk::WeakRefWrapper dialog_weak(GTK_WIDGET(dialog));

    global_core_api->set_setting(profile_id, orion::settings::general::low_power_mode, true,
    [dialog_weak](orion::SetSettingReply<bool> reply){
        VERIFY_LOG(reply.err);
        orion::log_debug("low_power_mode: new_value=%d", reply.new_value);

        auto dialog_strong = dialog_weak.lock();
        if (!dialog_strong) return;
        LowPowerModeDialog* dialog = LOW_POWER_MODE_DIALOG(dialog_strong.get());

        adw_dialog_close(ADW_DIALOG(dialog));
    });
}

static void
on_close_button_clicked(GtkButton* button, gpointer user_data)
{
	orion::log_debug("low_power_mode: on_close_button_clicked");

	adw_dialog_close(ADW_DIALOG(user_data));
}

static void 
low_power_mode_dialog_class_init(LowPowerModeDialogClass* klass)
{
	gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass), GRESOURCE_PREFIX "/src/window/low_power_mode_dialog.ui");
    gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), on_close_button_clicked);
    gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), on_enable_button_clicked);
}
