#include <gtk/gtk.h>
#include <adwaita.h>
#include <iostream>
#include <common/src/log.h>

#include "app.h"
#include "start_page_settings_popover.h"
#include "util/action_util.h"

struct _StartPageSettingsPopover
{
    GtkPopover parent;    
    GtkPicture* picture0; 
    GtkPicture* picture1; 
    GtkPicture* picture2; 
};

G_DEFINE_TYPE(StartPageSettingsPopover, start_page_settings_popover, GTK_TYPE_POPOVER)

GtkPopover*
start_page_settings_popover_new()
{
    return GTK_POPOVER(g_object_new(START_PAGE_SETTINGS_POPOVER_TYPE, NULL));
}

static GdkTexture*
load_texture_from_resource(const char* resource_path)
{
    GError* error = NULL;
    GBytes* bytes = g_resources_lookup_data(resource_path, G_RESOURCE_LOOKUP_FLAGS_NONE, &error);
    if (!bytes) {
        g_warning("Failed to load resource: %s", error->message);
        g_clear_error(&error);
        return NULL;
    }

    UNIQUE_PTR(GdkTexture, g_object_unref) tex(gdk_texture_new_from_bytes(bytes, &error));
    g_bytes_unref(bytes);

    if (!tex) {
        g_warning("Failed to create texture: %s", error->message);
        g_clear_error(&error);
        return NULL;
    }

    return tex.release();
}


static void
start_page_settings_popover_init(StartPageSettingsPopover *self)
{
    gtk_widget_init_template(GTK_WIDGET(self));


    // Load and set image 0
    GdkTexture *texture0 = load_texture_from_resource(GRESOURCE_PREFIX  "/mockImages/mi1.png");
    if (texture0) {
        gtk_picture_set_paintable(self->picture0, GDK_PAINTABLE(texture0));
        g_object_unref(texture0);
    }

    // Load and set image 1
    GdkTexture *texture1 = load_texture_from_resource(GRESOURCE_PREFIX "/mockImages/mi2.png");
    if (texture1) {
        gtk_picture_set_paintable(self->picture1, GDK_PAINTABLE(texture1));
        g_object_unref(texture1);
    }

    // Load and set image 2
    GdkTexture *texture2 = load_texture_from_resource(GRESOURCE_PREFIX "/mockImages/mi3.png");
    if (texture2) {
        gtk_picture_set_paintable(self->picture2, GDK_PAINTABLE(texture2));
        g_object_unref(texture2);
    }
}

static void
on_load_image_button_clocked(GtkButton* button, gpointer user_data)
{
    auto file_dialog = gtk_file_dialog_new();
	gtk_file_dialog_open(file_dialog, NULL, NULL, NULL, NULL);
    
}
static void
start_page_settings_popover_class_init(StartPageSettingsPopoverClass* klass)
{
	gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass), GRESOURCE_PREFIX "/src/window/start_page_settings_popover.ui");
    
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), StartPageSettingsPopover, picture0);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), StartPageSettingsPopover, picture1);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), StartPageSettingsPopover, picture2);
    gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), on_load_image_button_clocked);
}
