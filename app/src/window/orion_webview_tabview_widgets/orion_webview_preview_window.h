#pragma once
#include <gtk/gtk.h>

#include "window/main_window/main_window.h"

#define ORION_WEBVIEW_PREVIEW_WINDOW_TYPE (orion_webview_preview_window_get_type ())

G_DECLARE_FINAL_TYPE(OrionWebviewPreviewWindow, orion_webview_preview_window, ORION, WEBVIEW_PREVIEW_WINDOW, AdwApplicationWindow)

OrionWebviewPreviewWindow* orion_webview_preview_window_new(const std::string& uri, const GdkRectangle* pointing_to, MainWindow* win);