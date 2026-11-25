#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT

#include <common/src/defer.h>
#include <common/src/log.h>

#include "jsc_util.h"

namespace orion::jsc {

std::string
value_to_string(JSCValue* value)
{
    char* ptr = jsc_value_to_string(value);
    if (!ptr) {
        orion::log_verbose("%s: jsc_value_to_string returned NULL", __func__);
        return "";
    }
    defer([ptr]{ g_free(ptr); });
    std::string s = ptr;
    return s;
}

} // namespace orion::jsc

#endif // defined(ENABLE_WEBKIT) && ENABLE_WEBKIT