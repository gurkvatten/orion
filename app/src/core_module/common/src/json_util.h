#pragma once

#include <string>

namespace orion {

std::string escape_json_field(const std::string& value);
std::string unescape_json_field(const std::string& value);
enum UnescapeState {
    Normal,
    InEscape,
    InHex,
    InError,
    End
};
char unescape_json_char(const char in, UnescapeState& state, std::string& hex_in_out);

} // namespace orion