#include "format_util.h"

namespace orion::format {

std::string
format_integer_with_non_breaking_spaces(int64_t value)
{
    std::string formatted = std::to_string(value);
    size_t digit_start = value < 0 ? 1 : 0;
    for (size_t separator = formatted.size(); separator > digit_start + 3; separator -= 3) {
        formatted.insert(separator - 3, "\xC2\xA0");
    }
    return formatted;
}

} // namespace orion::format