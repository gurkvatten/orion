#include <gtk/gtk.h>
#include <core/settings.h>
#include <common/src/log.h>
#include "main_window/main_window.h"
#include "app.h"
#include <unordered_map>
#include <string>
#include <vector>
#include <memory>

#include "reset_orion_dialog.h"
#include "reset_orion_resetter.h"

#define widget_get_profile_id(widget) main_window_get_current_profile_id(MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(widget))))

/**
 * @brief Helper that can register and instantiate a list of Resetter types.
 *
 * This template is used as a single source of truth for all resetter
 * implementations used by the Reset Orion dialog.
 */
template <typename... Resetters>
struct ResetterRegistry
{
    /**
     * @brief Register GTK template callbacks for every resetter type.
     *
     * @param widget_class The GTK widget class to bind callbacks to.
     */
    static void register_all_callbacks(GtkWidgetClass* widget_class)
    {
        (Resetters::register_callbacks(widget_class), ...);
    }

    /**
     * @brief Instantiate all resetter objects and append them to @p cleaners.
     *
     * @param cleaners Vector that will receive newly created resetter instances.
     */
    static void instantiate_all(std::vector<std::unique_ptr<OptionResetterBase>>& cleaners)
    {
        (cleaners.push_back(std::make_unique<Resetters>()), ...);
    }
};

using AllResetters = ResetterRegistry<
    CleanHistoryResetter,
    BookmarksResetter,
    ReadingListResetter,
    SnapshotResetter,
    DownloadsResetter,
    ICloudAppDataResetter,
    SettingsResetter,
    CookiesResetter,
    BrowserCacheResetter,
    LocalStorageAndDatabaseResetter,
    PasswordsResetter,
    ExtensionsResetter,
    CloseWindowsResetter>;

/**
 * @brief Private instance struct for the Reset Orion dialog GObject.
 */
struct _ResetOrionDialog
{
    AdwDialog parent;
    std::vector<std::unique_ptr<OptionResetterBase>> cleaners;
    int current_older_than_index;
};

G_DEFINE_TYPE(ResetOrionDialog, reset_orion_dialog, ADW_TYPE_DIALOG)

/**
 * @brief Create a new ResetOrionDialog instance.
 *
 * @return New AdwDialog instance.
 */
AdwDialog*
reset_orion_dialog_new()
{
    return ADW_DIALOG(g_object_new(RESET_ORION_DIALOG_TYPE, NULL));
}

/**
 * @brief Handler for the dialog cancel button.
 *
 * Closes the dialog without performing any reset actions.
 */
static void
on_cancel_clicked(GtkButton*button, gpointer user_data)
{
    adw_dialog_close(ADW_DIALOG(RESET_ORION_DIALOG(user_data)));
}

/**
 * @brief Confirmation response handler for the destructive reset alert.
 *
 * If the user confirmed the reset this iterates the resetter instances
 * and calls reset_option for each checked option.
 */
static void
on_reset_orion_confirmation(AdwAlertDialog* self, gchar* response, gpointer user_data)
{
    orion::log_debug("on_reset_orion_confirmation: %s", response);
    if (g_strcmp0(response, "reset") == 0)
    {

        ResetOrionDialog* dialog = RESET_ORION_DIALOG(user_data);
        orion::ProfileId profile_id = widget_get_profile_id(dialog);
        MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(dialog)));
        for (auto& cleaner : dialog->cleaners)
        {
            if (cleaner->is_checked(profile_id))
            {
                cleaner->reset_option(profile_id, dialog->current_older_than_index, win);
            }
        }
        adw_dialog_close(ADW_DIALOG(dialog));
    }
}

/**
 * @brief Show a confirmation alert and present destructive reset responses.
 *
 * The alert will call @c on_reset_orion_confirmation when the user responds.
 */
static void
on_reset_clicked(GtkButton* button, gpointer user_data)
{
    GtkWidget* parent = GTK_WIDGET(user_data);
    AdwDialog* dialog = adw_alert_dialog_new(NULL, NULL);

    adw_alert_dialog_format_body_markup(
        ADW_ALERT_DIALOG(dialog),
        _("<b>Are you sure you want to reset Orion?</b>\n\nYou cannot undo this operation."));

    adw_alert_dialog_add_responses(
        ADW_ALERT_DIALOG(dialog),
        "cancel", _("_Cancel"),
        "reset", _("_Reset"),
        NULL);

    adw_alert_dialog_set_response_appearance(
        ADW_ALERT_DIALOG(dialog),
        "reset",
        ADW_RESPONSE_DESTRUCTIVE);

    adw_alert_dialog_set_default_response(ADW_ALERT_DIALOG(dialog), "reset");
    adw_alert_dialog_set_close_response(ADW_ALERT_DIALOG(dialog), "cancel");

    g_signal_connect(dialog, "response", G_CALLBACK(on_reset_orion_confirmation), parent);
    adw_dialog_present(dialog, parent);
}

/**
 * @brief Instance initializer: construct cleaners container and load template.
 */
static void
reset_orion_dialog_init(ResetOrionDialog* win)
{
    std::construct_at(&win->cleaners);
    gtk_widget_init_template(GTK_WIDGET(win));

    AllResetters::instantiate_all(win->cleaners);
}

/**
 * @brief Finalizer: destroy the cleaners vector then chain up.
 */
static void
reset_orion_dialog_finalize(GObject* object)
{
    ResetOrionDialog* self = RESET_ORION_DIALOG(object);

    std::destroy_at(&self->cleaners);


    G_OBJECT_CLASS(reset_orion_dialog_parent_class)->finalize(object);
}

/**
 * @brief GTK callback when the "Older than" dropdown selection changes.
 *
 * Persists the selection to the core settings and updates the dialog state.
 */
static void
on_older_than_drop_down_notify_selected(GtkDropDown* drop_down, GParamSpec* pspec, gpointer user_data)
{
    orion::ProfileId profile_id = main_window_get_current_profile_id(MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(drop_down))));
    int selected_index = gtk_drop_down_get_selected(drop_down);

    /*
     * GTK dropdown index is 0-based (0=All Time, 1=One Hour, ..., 4=One Month),
     * OlderThan enum starts at 1 (all_time=1, one_hour=2, ..., one_month=5).
     */
    orion::settings::reset_orion::OlderThan older_than_value = static_cast<orion::settings::reset_orion::OlderThan>(selected_index + 1);
    ResetOrionDialog* dialog = RESET_ORION_DIALOG(user_data);
    dialog->current_older_than_index = static_cast<int>(older_than_value);

    global_core_api->set_setting(profile_id, orion::settings::reset_orion::older_than, older_than_value,
                                 [](orion::SetSettingReply<orion::settings::reset_orion::OlderThan> reply)
                                 {
                                     VERIFY_LOG(reply.err);
                                 });
}

/**
 * @brief Map handler for the "Older than" dropdown; synchronises widget
 * state from core settings when the widget is mapped.
 */
static void
on_older_than_drop_down_map(GtkDropDown* drop_down, gpointer user_data) {
    orion::ProfileId profile_id = main_window_get_current_profile_id(MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(drop_down))));
    auto wrapped_ref = std::make_shared<orion::gtk::WeakRefWrapper>(drop_down);
    ResetOrionDialog* dialog = RESET_ORION_DIALOG(user_data);
    global_core_api->get_setting(profile_id, orion::settings::reset_orion::older_than,
                                 [wrapped_ref, profile_id, user_data, dialog](orion::GetSettingReply<orion::settings::reset_orion::OlderThan> reply)
                                 {
                                     VERIFY_LOG(reply.err);

                                     dialog->current_older_than_index = static_cast<int>(reply.value);
                                     GtkDropDown* drop_down = GTK_DROP_DOWN(wrapped_ref->get());
                                     if (drop_down)
                                     {
                                         /* OlderThan enum starts at 1, GTK dropdown index starts at 0 */
                                         guint gtk_index = static_cast<guint>(reply.value) > 0 ? static_cast<guint>(reply.value) - 1 : 0;
                                         gtk_drop_down_set_selected(drop_down, gtk_index);
                                     }
                                 });
}

/**
 * @brief Class initializer: set template, bind callbacks and set finalizer.
 */
static void
reset_orion_dialog_class_init(ResetOrionDialogClass* klass)
{
    GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);
    gtk_widget_class_set_template_from_resource(widget_class, GRESOURCE_PREFIX "/src/window/reset_orion_dialog.ui");

    GObjectClass* object_class = G_OBJECT_CLASS(klass);
    object_class->finalize = reset_orion_dialog_finalize;

    AllResetters::register_all_callbacks(widget_class);

    gtk_widget_class_bind_template_callback(widget_class, on_older_than_drop_down_notify_selected);
    gtk_widget_class_bind_template_callback(widget_class, on_older_than_drop_down_map);
    gtk_widget_class_bind_template_callback(widget_class, on_cancel_clicked);
    gtk_widget_class_bind_template_callback(widget_class, on_reset_clicked);
}