#include "back_forward_list.h"

namespace orion {

G_DEFINE_BOXED_TYPE(BackForwardList, back_forward_list, back_forward_list_copy, back_forward_list_free)

BackForwardList* back_forward_list_copy(BackForwardList* self)
{
    auto copy_func = [](gconstpointer src, gpointer user_data){
        return (gpointer)new BackForwardListEntry(*(static_cast<const BackForwardListEntry*>(src)));
    };

    return g_list_copy_deep(self, copy_func, NULL);
}

void back_forward_list_free(BackForwardList* self)
{
    auto destroy_notify = [](gpointer data){
        delete static_cast<BackForwardListEntry*>(data);
    };

    g_list_free_full(self, destroy_notify);
}


}