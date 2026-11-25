#pragma once

#include <gtk/gtk.h>

#include <string>

#define APP_TYPE_BACK_FORWARD_LIST (orion::back_forward_list_get_type())

namespace orion {
    
GType back_forward_list_get_type(void);

using BackForwardList = GList;

struct BackForwardListEntry {
    std::string uri;
    std::string title;
};


BackForwardList* back_forward_list_copy(BackForwardList* self);
void back_forward_list_free(BackForwardList* self);

}