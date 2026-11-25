#pragma once
#include <gtk/gtk.h>

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
GType style_sheet_list_get_type();

namespace orion {
void webkit_user_style_sheet_unref_wrapper(gpointer data);
}

gpointer style_sheet_list_copy (gpointer boxed);
void style_sheet_list_free (gpointer boxed);
#endif