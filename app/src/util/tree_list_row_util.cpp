#include <gtk/gtk.h>

#include "tree_list_row_util.h"

namespace orion::gtk {

GObjectPtr
tree_list_row_get_item_strong_ref(gpointer obj)
{
    while(GTK_IS_TREE_LIST_ROW(obj))
    {
        gpointer tmp = gtk_tree_list_row_get_item(static_cast<GtkTreeListRow*>(obj));
        g_object_unref(obj);
        obj = tmp;
    }
    return GObjectPtr(G_OBJECT(obj));
}

} // namespace orion::gtk