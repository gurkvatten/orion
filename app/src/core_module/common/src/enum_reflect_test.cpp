#define ENABLE_VERBOSE_LOGS 0

#include "enum_reflect.h"
#include <iostream>
#include <cassert>
#include <src/log.h>

namespace orion::enum_reflect {

// Define an enum
enum class HttpStatus {
    OK,
    NotFound,
    Unauthorized,
    ServerError
};

const auto& handler = REGISTER_ENUM(HttpStatus,
     ENUM_ENTRY(HttpStatus::OK)
    , ENUM_ENTRY(HttpStatus::NotFound)
    , ENUM_ENTRY(HttpStatus::Unauthorized)
    , ENUM_ENTRY(HttpStatus::ServerError)
);

// ============================================================================
// Usage Examples
// ============================================================================

int
enum_reflect_test()
{
    using namespace orion::enum_reflect;
 
    // 1. runtime enum to string
    
    auto status_ok_str = handler.to_string(HttpStatus::ServerError);
    assert(status_ok_str == "ServerError");
    orion::log_verbose("Runtime ServerError: %s", std::string(status_ok_str.value()).c_str());

    // 2. runtime string to enum

    auto status_from_str = handler.from_string("ServerError");
    if (status_from_str) {
        assert(status_from_str.value() == HttpStatus::ServerError);
        orion::log_verbose("String 'ServerError' -> HttpStatus::ServerError");
    }

    // 3. Invalid string returns std::nullopt
    auto invalid_status = handler.from_string("InvalidStatus");
    if (!invalid_status) {
        orion::log_verbose("String 'InvalidStatus' -> std::nullopt");
    }

    // ========================================================================
    // 4. Loop through all enum values and their names (runtime)
    // ========================================================================
    // Access the reflection table to iterate through all enum values
    
    orion::log_verbose("All registered HTTP status codes:");
    for (const auto& [name, value] : handler.values()) {
        orion::log_verbose("  %s", std::string(name).c_str());
    }

    return 0;
}

} // namespace orion::enum_reflect
