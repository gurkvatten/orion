#include <sstream>

#include "webkit_util.h"

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
#include <webkit/webkit.h>
#endif

namespace orion::webkit {

std::string
get_webkit_version()
{
    std::stringstream ss;
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    ss  << webkit_get_major_version() << "."
        << webkit_get_minor_version() << "."
        << webkit_get_micro_version();
#else
    ss << "WebKit not enabled";
#endif
    return ss.str();
}

} // namespace orion::webkit