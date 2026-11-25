#include <gtk/gtk.h>
#include <common/src/log.h>
#include <core/settings.h>

#include "app.h"
#include "quit_orion_dialog.h"
#include "util/text_util.h"
#include "util/profile_manager.h"
#include "window/main_window/main_window.h"

struct _QuitOrionDialog
{
	AdwDialog parent;

    GtkLabel* body_label;
    GtkCheckButton* show_warning_check_button;
};

G_DEFINE_TYPE(QuitOrionDialog, quit_orion_dialog, ADW_TYPE_DIALOG)

AdwDialog*
quit_orion_dialog_new()
{
	return ADW_DIALOG(g_object_new(QUIT_ORION_DIALOG_TYPE, NULL));
}

static void
on_cancel_clicked(GtkButton* button, gpointer user_data)
{
	adw_dialog_close(ADW_DIALOG(QUIT_ORION_DIALOG(user_data)));
}

static void
on_quit_clicked(GtkButton* button, gpointer user_data)
{
    QuitOrionDialog* self = QUIT_ORION_DIALOG(user_data);
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(self)));
    orion::ProfileId profile_id = main_window_get_current_profile_id(win);
    orion::gtk::WeakRefWrapper self_weak(self);

    auto on_close = [self_weak,profile_id]{
        auto self_strong = self_weak.lock();
        if (!self_strong) return;
        QuitOrionDialog* self = QUIT_ORION_DIALOG(self_strong.get());

        adw_dialog_close(ADW_DIALOG(self));
        //orion_app_quit();
        // TODO: quit only opened windows for current profile
        orion::profile::quit(profile_id);
    };

    if (gtk_check_button_get_active(self->show_warning_check_button)) {
        on_close();
    } else {
        global_core_api->set_setting(profile_id, orion::settings::general::show_warning_before_quitting, false,
            [on_close](orion::SetSettingReply<bool> reply){
                VERIFY_LOG(reply.err);
                on_close();
            }
        );
    }
}

static void
quit_orion_dialog_root(GtkWidget* widget)
{
    GTK_WIDGET_CLASS(quit_orion_dialog_parent_class)->root(widget);

    QuitOrionDialog* self = QUIT_ORION_DIALOG(widget);
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(widget));
    orion::ProfileId profile_id = main_window_get_current_profile_id(win);
    orion::gtk::WeakRefWrapper self_weak(self);

    global_core_api->get_windows_by_query(profile_id, [self_weak,profile_id](orion::GetWindowsReply reply) {
        VERIFY_LOG(reply.err);

        auto windows = std::move(reply.windows);
        global_core_api->get_setting(profile_id, orion::settings::general::app_opens_with, [self_weak,windows](orion::GetSettingReply<orion::settings::AppOpensWith> reply) {
            VERIFY_LOG(reply.err);
            

            auto self_strong = self_weak.lock();
            if (!self_strong) return;
            QuitOrionDialog* self = QUIT_ORION_DIALOG(self_strong.get());
            
            int64_t num_tabs = 0;
            for (const orion::WindowDto& window : windows) {
                num_tabs += window.num_tabs;
            }

            const std::string windows_word =
                        orion::text::ngettext_string("tab group", "tab groups", static_cast<int>(windows.size()));
            const std::string tabs_word =
                orion::text::ngettext_string("tab", "tabs", static_cast<int>(num_tabs));

            const char* msgid = nullptr;
            switch (reply.value) {
                case orion::settings::AppOpensWith::all_windows_from_last_session:
                case orion::settings::AppOpensWith::all_non_private_windows_from_last_session:
                    msgid = "You have %1$d open %3$s containing %2$d %4$s that will be restored when Orion reopens.";
                    break;

                case orion::settings::AppOpensWith::new_window:
                case orion::settings::AppOpensWith::new_private_window:
                    msgid = "You have %1$d open %3$s containing %2$d %4$s that will be closed.";
                    break;

                default:
                    orion::log_error("Unexpected app_opens_with value: %d", static_cast<int>(reply.value));
                    msgid = "You have %1$d open %3$s containing %2$d %4$s.";
                    break;
            }

            const std::string body = orion::text::translatef(
                msgid,
                static_cast<int>(windows.size()),
                static_cast<int>(num_tabs),
                windows_word.c_str(),
                tabs_word.c_str()
            );
            gtk_label_set_text(self->body_label, body.c_str());
        });
    });
}

static void
quit_orion_dialog_init(QuitOrionDialog* self)
{
	gtk_widget_init_template(GTK_WIDGET(self));
}

static void
quit_orion_dialog_class_init(QuitOrionDialogClass* klass)
{
    GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);
    widget_class->root = quit_orion_dialog_root;
	gtk_widget_class_set_template_from_resource(widget_class, GRESOURCE_PREFIX "/src/window/quit_orion_dialog.ui");

	gtk_widget_class_bind_template_callback(widget_class, on_cancel_clicked);
    gtk_widget_class_bind_template_callback(widget_class, on_quit_clicked);

    gtk_widget_class_bind_template_child(widget_class, QuitOrionDialog, body_label);
    gtk_widget_class_bind_template_child(widget_class, QuitOrionDialog, show_warning_check_button);
}
