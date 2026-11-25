#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <string>
#include <sstream>

#include "app.h"
#include "about_dialog.h"
#include "util/webkit_util.h"

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
#include <webkit/webkit.h>
#endif

struct _AboutDialog
{
	AdwDialog parent;
};

G_DEFINE_TYPE(AboutDialog, about_dialog, ADW_TYPE_DIALOG)

AdwDialog*
about_dialog_new()
{
	return ADW_DIALOG(g_object_new(ABOUT_DIALOG_TYPE, NULL));
}

static void
about_dialog_init(AboutDialog* win)
{
	gtk_widget_init_template(GTK_WIDGET(win));
}

static void
on_button_clicked(GtkButton* button, gpointer user_data)
{
	g_print("Button clicked!\n");
}

static void
about_dialog_class_init(AboutDialogClass* klass)
{
	gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass), GRESOURCE_PREFIX "/src/window/about_window.ui");

	gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), on_button_clicked);
}

AdwDialog*
about_dialog_new_adw()
{
    // TODO: adapt fields according to requirements

    const char* developers[] = {
        "Developer 1",
        "Developer 2",
        "Developer 3",
        NULL,
    };

    const char* funders[] = {
        "Funder 1",
        "Funder 2",
        "Funder 3",
        NULL,
    };

    std::string version = APPLICATION_VERSION " (WebKit " + orion::webkit::get_webkit_version() + ")";
    orion::log_debug("WebKit version: %s", orion::webkit::get_webkit_version().c_str());

    AdwDialog* dialog = adw_about_dialog_new_from_appdata(
        // This loads the release notes from the app metainfo file into "What's New" section
        GRESOURCE_PREFIX "/app.metainfo.xml", // path to the app metainfo file
        APPLICATION_VERSION // release notes version
    );

    g_object_set(G_OBJECT(dialog),
        "application-name", "Orion",
        "application-icon", "orion-logo",
        "version", version.c_str(),
        "website", "https://kagi.com/orion/",
        "comments", _("A fast, private browser."),
        "copyright", "© 2025-2026 Kagi",
        "issue-url", "https://github.com/kagisearch/orion-linux/issues/new",
        "support-url", "https://orionfeedback.org/",
        "license-type", GTK_LICENSE_CUSTOM,
        "license", "", // TODO: fill actual license text here
        //"release-notes", release_notes.c_str(),
        "debug-info", _("Debug info example: Lorem Ipsum is simply dummy text of the printing and typesetting industry. Lorem Ipsum has been the industry's standard dummy text ever since the 1500s, when an unknown printer took a galley of type and scrambled it to make a type specimen book. It has survived not only five centuries, but also the leap into electronic typesetting, remaining essentially unchanged. It was popularised in the 1960s with the release of Letraset sheets containing Lorem Ipsum passages, and more recently with desktop publishing software like Aldus PageMaker including versions of Lorem Ipsum."),
        "developers", developers,
        NULL);

    adw_about_dialog_add_acknowledgement_section(ADW_ABOUT_DIALOG(dialog), _("Funded by"), funders);

    return ADW_DIALOG(dialog);
}