#pragma once

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT

#include <string>
#include <jsc/jsc.h>

namespace orion::jsc {

// Returns empty string if jsc_value_to_string call returns NULL
std::string value_to_string(JSCValue* value);

} // namespace orion::jsc

#endif // defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
