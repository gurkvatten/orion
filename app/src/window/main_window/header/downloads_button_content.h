#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>

#define APP_DOWNLOADS_BUTTON_CONTENT_TYPE (downloads_button_content_get_type ())

G_DECLARE_FINAL_TYPE(DownloadsButtonContent, downloads_button_content, APP, DOWNLOADS_BUTTON_CONTENT, GtkWidget)

void downloads_button_content_notify_click(DownloadsButtonContent* self);