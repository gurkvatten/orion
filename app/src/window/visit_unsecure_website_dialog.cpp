#include <gdk/gdk.h>
#include <common/src/log.h>
#include "app.h"
#include "visit_unsecure_website_dialog.h"
#include "util/text_util.h"

struct _VisitUnsecureWebsiteDialog
{
	AdwDialog parent;
};

G_DEFINE_TYPE(VisitUnsecureWebsiteDialog, visit_unsecure_website_dialog, ADW_TYPE_DIALOG)

enum {
    ALLOW_ONCE,
    ALWAYS_ALLOW_FOR_THIS_SITE,
    CANCEL,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {0};

AdwDialog*
visit_unsecure_website_dialog_new()
{
    VisitUnsecureWebsiteDialog* dialog = VISIT_UNSECURE_WEBSITE_DIALOG(g_object_new(VISIT_UNSECURE_WEBSITE_DIALOG_TYPE, NULL));
	return ADW_DIALOG(dialog);
}

static void
on_permissions_request_dialog_close(AdwDialog* dialog, gpointer user_data)
{
    VisitUnsecureWebsiteDialog* self = VISIT_UNSECURE_WEBSITE_DIALOG(dialog);
}

static void
visit_unsecure_website_dialog_init(VisitUnsecureWebsiteDialog* win)
{
	gtk_widget_init_template(GTK_WIDGET(win));
    g_signal_connect(win, "closed", G_CALLBACK(on_permissions_request_dialog_close), NULL);
}

static void
on_allow_once_button_clicked(GtkButton* button, gpointer user_data)
{
    VisitUnsecureWebsiteDialog* self = VISIT_UNSECURE_WEBSITE_DIALOG(user_data);
    g_signal_emit(self, signals[ALLOW_ONCE], 0);
    adw_dialog_close(ADW_DIALOG(user_data));
}

static void
on_allow_always_button_clicked(GtkButton* button, gpointer user_data)
{
    VisitUnsecureWebsiteDialog* self = VISIT_UNSECURE_WEBSITE_DIALOG(user_data);
    g_signal_emit(self, signals[ALWAYS_ALLOW_FOR_THIS_SITE], 0);
    adw_dialog_close(ADW_DIALOG(user_data));
}

static void
on_cancel_button_clicked(GtkButton* button, gpointer user_data)
{
    VisitUnsecureWebsiteDialog* self = VISIT_UNSECURE_WEBSITE_DIALOG(user_data);
    g_signal_emit(self, signals[CANCEL], 0);
    adw_dialog_close(ADW_DIALOG(user_data));
}

static void 
visit_unsecure_website_dialog_class_init(VisitUnsecureWebsiteDialogClass* klass)
{
    signals[ALLOW_ONCE] = g_signal_new(
        "allow-once",
        G_TYPE_FROM_CLASS(klass),
        G_SIGNAL_RUN_LAST,
        0,
        NULL,
        NULL,
        NULL,
        G_TYPE_NONE,
        0
    );
    signals[ALWAYS_ALLOW_FOR_THIS_SITE] = g_signal_new(
        "always-allow-for-this-site",
        G_TYPE_FROM_CLASS(klass),
        G_SIGNAL_RUN_LAST,
        0,
        NULL,
        NULL,
        NULL,
        G_TYPE_NONE,
        0
    );
    signals[CANCEL] = g_signal_new(
        "cancel",
        G_TYPE_FROM_CLASS(klass),
        G_SIGNAL_RUN_LAST,
        0,
        NULL,
        NULL,
        NULL,
        G_TYPE_NONE,
        0
    );

	gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass), GRESOURCE_PREFIX "/src/window/visit_unsecure_website_dialog.ui");
    gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), on_allow_once_button_clicked);
    gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), on_allow_always_button_clicked);
    gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), on_cancel_button_clicked);
}
