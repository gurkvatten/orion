#include <gdk/gdk.h>
#include <adwaita.h>

#include "../app.h"
#include "add_web_extension_dialog.h"
#include "core/webkit_bridge.h"
#include "util/text_util.h"

struct _AddWebExtensionDialog
{
	AdwDialog parent;
    WebExtensions::ExtensionInfo extension;
    std::shared_ptr<WebExtensions::WebExtensionInstallSource> source;
    GtkLabel* add_label;
    int response;
};

G_DEFINE_TYPE(AddWebExtensionDialog, add_web_extension_dialog, ADW_TYPE_DIALOG)

AdwDialog*
add_web_extension_dialog_new(const WebExtensions::ExtensionInfo& extension, std::shared_ptr<WebExtensions::WebExtensionInstallSource> source)
{
    AddWebExtensionDialog* win = ADD_WEB_EXTENSION_DIALOG(g_object_new(ADD_WEB_EXTENSION_DIALOG_TYPE, NULL));
    win->extension = extension;
    win->source = source;
    win->response = GTK_RESPONSE_NONE;
    std::string name = extension.displayName.value_or("");
    std::string add_label_text = orion::text::translatef(_("Would you like to add the web extension '%1$s'?"), name.c_str());
    gtk_label_set_text(win->add_label, add_label_text.c_str());
    return ADW_DIALOG(win);
}

WebExtensions::ExtensionInfo
add_web_extension_dialog_extension(AddWebExtensionDialog* win)
{
    return win->extension;
}

std::shared_ptr<WebExtensions::WebExtensionInstallSource>
add_web_extension_dialog_source(AddWebExtensionDialog* win)
{
    return win->source;
}

int
add_web_extension_dialog_response(AddWebExtensionDialog* win)
{
    return win->response;
}

static void
on_add_clicked(GtkButton* button, AddWebExtensionDialog* win)
{
    if(win->response == GTK_RESPONSE_NONE) {
        win->response = GTK_RESPONSE_OK;
    }
    adw_dialog_close(ADW_DIALOG(win));
}

static void
on_cancel_clicked(GtkButton* button, AddWebExtensionDialog* win)
{
    win->response = GTK_RESPONSE_CANCEL;
    adw_dialog_close(ADW_DIALOG(win));
}

static void
add_web_extension_dialog_init(AddWebExtensionDialog* win)
{
    std::construct_at(&win->extension);
    std::construct_at(&win->source);
	gtk_widget_init_template(GTK_WIDGET(win));
}

static void
add_web_extension_dialog_finalize(GObject* gobject)
{
    AddWebExtensionDialog* win = ADD_WEB_EXTENSION_DIALOG(gobject);
    std::destroy_at(&win->extension);
    std::destroy_at(&win->source);
    G_OBJECT_CLASS(add_web_extension_dialog_parent_class)->finalize(gobject);
}

static void
add_web_extension_dialog_class_init(AddWebExtensionDialogClass* klass)
{
    G_OBJECT_CLASS(klass)->finalize = add_web_extension_dialog_finalize;
    GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);
	gtk_widget_class_set_template_from_resource(widget_class, GRESOURCE_PREFIX "/src/window/add_web_extension_dialog.ui");
    gtk_widget_class_bind_template_callback(widget_class, on_add_clicked);
    gtk_widget_class_bind_template_callback(widget_class, on_cancel_clicked);
    gtk_widget_class_bind_template_child(widget_class, AddWebExtensionDialog, add_label);
}
