// Note: This definition is needed before the includes below.
#define ORION_SETTINGS_CPP

#include <core/settings_impl.h>

// Note: This include must be present to ensure that all settings are registered in compile time
// to their individual realms. Failing to include this will result in empty realms when trying
// to enumerate all settings in runtime.
#include <core/settings.h>

namespace orion::settings {

Error
parse_int64(const std::string& str, int64_t* value)
{
    assert(value != nullptr);
    const char* begin = str.data();
    const char* end = begin + str.size();

    int64_t output = 0;
    auto result = std::from_chars(begin, end, output);
    if (result.ec != std::errc()) {
        // parsing error
        RAISE(ErrorCode::argument, "failed to parse int64 from string: %s", str.c_str());
    }
    if (result.ptr != end) {
        // trailing characters
        RAISE(ErrorCode::argument, "failed to parse int64 from string (trailing characters): %s", str.c_str());
    }
    *value = output;
    SUCCEED();
}

Error
parse_double(const std::string& str, double* value)
{
    assert(value != nullptr);
    const char* begin = str.data();
    const char* end = begin + str.size();

    double output = 0;
    auto result = std::from_chars(begin, end, output);
    if (result.ec != std::errc()) {
        // parsing error
        RAISE(ErrorCode::argument, "failed to parse double from string: %s", str.c_str());
    }
    if (result.ptr != end) {
        // trailing characters
        RAISE(ErrorCode::argument, "failed to parse double from string (trailing characters): %s", str.c_str());
    }
    *value = output;
    SUCCEED();
}

} // namespace orion::settings