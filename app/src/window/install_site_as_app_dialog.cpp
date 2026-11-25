#include <gdk/gdk.h>

#include "../app.h"
#include "install_site_as_app_dialog.h"
#include <glib/gi18n.h>
#include "util/dialog_util.h"

struct _InstallSiteAsAppDialog
{
	AdwDialog parent;
    GtkGrid* grid;
};

G_DEFINE_TYPE(InstallSiteAsAppDialog, install_site_as_app_dialog, ADW_TYPE_DIALOG)

AdwDialog*
install_site_as_app_dialog_new()
{
	return ADW_DIALOG(g_object_new(INSTALL_SITE_AS_APP_DIALOG_TYPE, NULL));
}

static void
on_cancel_clicked(GtkButton* button, gpointer user_data)
{
    adw_dialog_close(ADW_DIALOG(user_data));
}

static void
on_create_clicked(GtkButton* button, gpointer user_data)
{
    adw_dialog_close(ADW_DIALOG(user_data));
}

static void
on_app_icon_browse_clicked(GtkButton* button, gpointer user_data)
{
    GtkFileDialog* file_dialog = gtk_file_dialog_new(); // file chooser
	gtk_file_dialog_open(file_dialog, orion::gtk::get_parent_window(GTK_WIDGET(user_data)), NULL, NULL, NULL);
	g_object_unref(file_dialog);
}

static void
on_location_browse_clicked(GtkButton* button, gpointer user_data)
{
    // TODO: browse to location in default File Manager
}

static void
install_site_as_app_dialog_init(InstallSiteAsAppDialog* self)
{
	gtk_widget_init_template(GTK_WIDGET(self));

#ifdef ENABLE_ADW_17_API
    GtkWidget* toggle_group = adw_toggle_group_new();

    AdwToggle* toggle0 = adw_toggle_new();
    adw_toggle_set_label(toggle0, _("Desktop"));
    adw_toggle_group_add(ADW_TOGGLE_GROUP(toggle_group), toggle0);

    AdwToggle* toggle1 = adw_toggle_new();
    adw_toggle_set_label(toggle1, _("Mobile"));
    adw_toggle_group_add(ADW_TOGGLE_GROUP(toggle_group), toggle1);
    
    gtk_grid_attach(self->grid, toggle_group, 0, 1, 1, 1);
#else // ENABLE_ADW_17_API
    GtkWidget* label = gtk_label_new(_("Here belongs an AdwToggleGroup, removed for compatibility reasons because your libadwaita is older than 1.7"));
    gtk_label_set_wrap(GTK_LABEL(label), TRUE);
    gtk_label_set_max_width_chars(GTK_LABEL(label), 20);
    gtk_grid_attach(self->grid, label, 0, 1, 1, 1);
#endif // ENABLE_ADW_17_API
}

static void
install_site_as_app_dialog_class_init(InstallSiteAsAppDialogClass* klass)
{
    GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);
	gtk_widget_class_set_template_from_resource(widget_class, GRESOURCE_PREFIX "/src/window/install_site_as_app_dialog.ui");
    gtk_widget_class_bind_template_child(widget_class, InstallSiteAsAppDialog, grid);
    gtk_widget_class_bind_template_callback(widget_class, on_cancel_clicked);
    gtk_widget_class_bind_template_callback(widget_class, on_create_clicked);
    gtk_widget_class_bind_template_callback(widget_class, on_app_icon_browse_clicked);
    gtk_widget_class_bind_template_callback(widget_class, on_location_browse_clicked);
}
