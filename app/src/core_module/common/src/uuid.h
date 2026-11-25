#pragma once

#include <string>

namespace orion {

std::string generate_uuid_v4();
std::string generate_uuid_v4(const std::string& seed);

} // namespace orion
