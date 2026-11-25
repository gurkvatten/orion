#include "toolbar_item_util.h"

namespace orion {

std::string
toolbar_item_type_to_string(ToolbarItemType type)
{
    auto it = global_toolbar_item_names.find(type);
    if (it == global_toolbar_item_names.end()) {
        return "";
    }
    return it->second;
}

} // namespace orion
