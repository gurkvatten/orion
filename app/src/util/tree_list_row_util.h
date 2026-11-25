#pragma once

#include "ref_util.h"

namespace orion::gtk {

//takes a strong reference to a GObject
GObjectPtr tree_list_row_get_item_strong_ref(gpointer obj);

} // namespace orion::gtk