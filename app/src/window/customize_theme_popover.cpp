#include <gtk/gtk.h>
#include <common/src/log.h>

#include "app.h"
#include "customize_theme_popover.h"

struct _CustomizeThemePopover
{
	GtkPopover parent;
};

G_DEFINE_TYPE(CustomizeThemePopover, customize_theme_popover, ADW_TYPE_DIALOG)

AdwDialog*
customize_theme_popover_new()
{
	return ADW_DIALOG(g_object_new(CUSTOMIZE_THEME_POPOVER_TYPE, NULL));
}

static void
on_custom_color_button_toggled(GtkCheckButton* button, gpointer user_data)
{
    if(gtk_check_button_get_active(GTK_CHECK_BUTTON(button)))
    {
        //TODO: find a way to use autohide true but not have this popover disappear on opening color dialog
        //GtkPopover* popover = GTK_POPOVER(gtk_widget_get_native(GTK_WIDGET(button)));
        //gtk_popover_set_autohide(popover,false);
        GtkColorDialog* color_dialog = gtk_color_dialog_new();
        gtk_color_dialog_set_with_alpha(color_dialog,false);
        (void)gtk_color_dialog_choose_rgba(
            color_dialog,
            GTK_WINDOW(gtk_widget_get_root(GTK_WIDGET(button))),
            NULL,
            NULL,
            NULL,
            NULL
        );
        g_object_unref(color_dialog);
    }
}

static void
on_custom_color_button_activate(GtkCheckButton* button, gpointer user_data)
{
}

static void
customize_theme_popover_init(CustomizeThemePopover* win)
{
	gtk_widget_init_template(GTK_WIDGET(win));
}

static void
customize_theme_popover_class_init(CustomizeThemePopoverClass* klass)
{
    GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);
	gtk_widget_class_set_template_from_resource(widget_class, GRESOURCE_PREFIX "/src/window/customize_theme_popover.ui");

    gtk_widget_class_bind_template_callback(widget_class, on_custom_color_button_toggled);
    gtk_widget_class_bind_template_callback(widget_class, on_custom_color_button_activate);
}
