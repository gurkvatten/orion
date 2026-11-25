#include "style_sheet_list.h"

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
#include <webkit/webkit.h>
gpointer
style_sheet_list_copy (gpointer boxed)
{
    GList* list = static_cast<GList*>(boxed);
    GList* new_list = NULL;
    for (GList *l = list; l != NULL; l = l->next) {
        new_list = g_list_append(new_list, webkit_user_style_sheet_ref((WebKitUserStyleSheet*)(l->data)));
    }
    return new_list;
}

namespace orion {
void
webkit_user_style_sheet_unref_wrapper(gpointer data)
{
    webkit_user_style_sheet_unref(static_cast<WebKitUserStyleSheet*>(data));
}
}

void
style_sheet_list_free (gpointer boxed)
{
    GList* list = static_cast<GList*>(boxed);
    g_list_free_full(list, orion::webkit_user_style_sheet_unref_wrapper);
}

GType
style_sheet_list_get_type()
{
    static GType type = 0;
    if(g_once_init_enter(&type))
    {
        GType _type = g_boxed_type_register_static("StyleSheetList", style_sheet_list_copy, style_sheet_list_free);

        g_once_init_leave(&type, _type);
    }
    return type;
}
#endif