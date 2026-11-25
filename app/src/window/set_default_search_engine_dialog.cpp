#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "set_default_search_engine_dialog.h"
#include "window/settings_dialog.h"

static void
on_yes(AdwAlertDialog* self, gchar* response, OrionWindow* win)
{
    if (g_strcmp0(response, "yes") == 0) {
        gtk_widget_activate_action(GTK_WIDGET(win), "win.orion-settings-on-page", "i", 7);
    }
}

AdwDialog*
set_default_search_engine_dialog_new(OrionWindow* parent)
{
    AdwDialog* dialog = adw_alert_dialog_new(NULL, NULL);

	adw_alert_dialog_format_body_markup(
		ADW_ALERT_DIALOG(dialog),
		_("<b>No default search engine selected.</b>\n\nWould you like to set one now?")
    );

	adw_alert_dialog_add_responses(
		ADW_ALERT_DIALOG(dialog),
		"no", _("_No"),
		"yes", _("_Yes"),
		NULL
    );

	adw_alert_dialog_set_response_appearance(
		ADW_ALERT_DIALOG(dialog),
		"yes",
		ADW_RESPONSE_SUGGESTED
    );

	adw_alert_dialog_set_default_response(ADW_ALERT_DIALOG(dialog), "yes");
	adw_alert_dialog_set_close_response(ADW_ALERT_DIALOG(dialog), "no");

    // GLib automatically pass NULL user_data to the callback if parent is destroyed/finalized
    // before the callback is called
	g_signal_connect_object(dialog, "response", G_CALLBACK(on_yes), parent, G_CONNECT_DEFAULT);
	return dialog;
}