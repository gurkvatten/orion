#pragma once

#include <string>
#include <stdarg.h>

namespace orion {

enum class LogLevel {
    error   = 0,
    info    = 1,
    warning = 2,
    debug   = 3,
    verbose = 4,
};

using LogSink = void(*)(const char* formatted_line);
void set_log_sink(LogSink sink);
void log_message(char level, const char* message);

void log_info(const char* format, ...);
void log_error(const char* format, ...);
void log_warning(const char* format, ...);
void log_debug(const char* format, ...);
void internal_log_verbose(const char* format, ...);
void log(const std::string& level, const char* format, va_list args);

#if ENABLE_VERBOSE_LOGS
#define log_verbose(...) internal_log_verbose(__VA_ARGS__)
#else
inline void log_verbose(const char* format, ...) {}
#endif

inline void log_nop(const char* format, ...) {}

void log_error(const std::string& message);

// Conditionally logs a message based on the specified log level. If ENABLE_VERBOSE_LOGS is not defined or is disabled,
// verbose messages will not be logged. Debug messages will only be logged in non-release builds. The implementation
// cannot be easily moved to .cpp file and it remains in the header. It has to be inline to avoid multiple definition
// errors.
inline void
log_by_level(LogLevel level, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    switch (level) {
    case LogLevel::verbose:
#if ENABLE_VERBOSE_LOGS
        log("V", format, args);
#endif
        break;
    case LogLevel::debug:
#ifndef NDEBUG
        log("D", format, args);
#endif
        break;
    case LogLevel::info:
        log("I", format, args);
        break;
    case LogLevel::warning:
        log("W", format, args);
        break;
    case LogLevel::error:
        log("E", format, args);
        break;
    }
    va_end(args);
}

} // namespace orion