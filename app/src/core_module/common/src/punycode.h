#pragma once

#include <string>

namespace orion::punycode {

std::string encode(const std::string& unicode_domain);
std::string decode(const std::string& domain);

} // namespace orion::punycode