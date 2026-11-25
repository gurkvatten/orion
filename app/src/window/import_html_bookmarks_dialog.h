#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "app.h"

#define IMPORT_HTML_BOOKMARKS_DIALOG_TYPE (import_html_bookmarks_dialog_get_type())

G_DECLARE_FINAL_TYPE(ImportHtmlBookmarksDialog, import_html_bookmarks_dialog, IMPORT_HTML_BOOKMARKS, DIALOG, AdwDialog)

AdwDialog* import_html_bookmarks_dialog_new();

void import_html_bookmarks_dialog_set_result(ImportHtmlBookmarksDialog* self, int num_imported, orion::Error err);