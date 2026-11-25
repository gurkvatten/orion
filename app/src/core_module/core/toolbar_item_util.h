#pragma once

#include <string>

#include <core/dto/toolbar_item_dto.h>

namespace orion {

// Returns empty string if the type is not found
std::string toolbar_item_type_to_string(ToolbarItemType type);

} // namespace orion