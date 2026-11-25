#pragma once

#include <string>
#include <memory>
#include <sstream>
#include <vector>

#include <src/log.h>
#include <src/error_code.h>

// With these it is possible to do varadic macro overloading based on the number of arguments in cross-platform way.
#define PP_EXPAND(x) x
#define PP_GET_MACRO_2(_1,_2,NAME,...) NAME

// Creates a new error with an error code (a source error) in unique_ptr form
#define MAKE_ERROR(error_code, format, ...) \
    orion::make_error(error_code, __FILE__, __LINE__, false, format, ##__VA_ARGS__)

// Creates a new error (a source error) in unique_ptr form
#define MAKE_ERROR_NO_CODE(format, ...) \
    orion::make_error(orion::ErrorCode::not_set, __FILE__, __LINE__, false, format, ##__VA_ARGS__)

// Creates a new error (a source error) in value form
#define MAKE_ERROR_VALUE(error_code, format, ...) \
    orion::make_error_value(error_code, __FILE__, __LINE__, false, format, ##__VA_ARGS__)

// Creates an error from scratch and logs it immediately
#define MAKE_ERROR_NO_CODE_LOG(format, ...) \
    orion::make_error(orion::ErrorCode::not_set, __FILE__, __LINE__, true, format, ##__VA_ARGS__)

// Creates an error with an error code from scratch and logs it immediately
#define MAKE_ERROR_LOG(error_code, format, ...) \
    orion::make_error(error_code, __FILE__, __LINE__, true, format, ##__VA_ARGS__)

// Adds a trace to an existing error - useful when forwarding errors to parent caller function
#define WRAP_ERROR(err) \
    orion::wrap_error(err, __FILE__, __LINE__)

#define RAISE(error_code, format, ...) \
    do { \
        return MAKE_ERROR(error_code, format, ##__VA_ARGS__); \
    } while (0)

#define RAISE_LOG(error_code, format, ...) \
    do { \
        return MAKE_ERROR_LOG(error_code, format, ##__VA_ARGS__); \
    } while (0)

#define RAISE_NO_CODE(format, ...) \
    do { \
        return MAKE_ERROR(format, ##__VA_ARGS__); \
    } while (0)

#define RAISE_NO_CODE_LOG(format, ...) \
    do { \
        return MAKE_ERROR_LOG(format, ##__VA_ARGS__); \
    } while (0)

#define VERIFY(err) \
    do { \
        /* Use local variable to avoid multiple evaluation of the expression.
         * Note that declaration and assignment must be separate statements
         * otherwise this macro will not work in some contexts.
         */ \
        orion::Error verify_err_macro_variable__; \
        verify_err_macro_variable__ = err; \
        if (verify_err_macro_variable__) { \
            return WRAP_ERROR(std::move(verify_err_macro_variable__)); \
        } \
    } while (0)

#define LOG_ERROR(err) \
    orion::log_error("%s", WRAP_ERROR(err).to_string().c_str());

#define VERIFY_LOG(err) \
    do { \
        /* Use local variable to avoid multiple evaluation of the expression.
         * Note that declaration and assignment must be separate statements
         * otherwise this macro will not work in some contexts.
         */ \
        orion::Error verify_err_macro_variable__; \
        verify_err_macro_variable__ = err; \
        if (verify_err_macro_variable__) { \
            LOG_ERROR(std::move(verify_err_macro_variable__)); \
            return; \
        } \
    } while (0)

#define VERIFY_LOG_ERR_DEFER(err, err_defer) \
    do { \
        /* Use local variable to avoid multiple evaluation of the expression.
         * Note that declaration and assignment must be separate statements
         * otherwise this macro will not work in some contexts.
         */ \
        orion::Error verify_err_macro_variable__; \
        verify_err_macro_variable__ = err; \
        if (verify_err_macro_variable__) { \
            LOG_ERROR(std::move(verify_err_macro_variable__)); \
            (err_defer); \
            return; \
        } \
    } while (0)

#define VERIFY_LOG_OR_DONE_CB(err, done_cb) \
    do { \
        /* to avoid multiple evaluation */ \
        const auto& cb_macro_variable__ = (done_cb); \
        if (cb_macro_variable__) { \
            if (err) { \
                cb_macro_variable__(err); \
                return; \
            } \
        } else { \
            VERIFY_LOG(err); \
        } \
    } while (0)

#define CO_VERIFY_LOG(err) \
    do { \
        /* Use local variable to avoid multiple evaluation of the expression.
         * Note that declaration and assignment must be separate statements
         * otherwise this macro will not work in some contexts.
         */ \
        orion::Error verify_err_macro_variable__; \
        verify_err_macro_variable__ = err; \
        if (verify_err_macro_variable__) { \
            LOG_ERROR(std::move(verify_err_macro_variable__)); \
            co_return; \
        } \
    } while (0)

#if !defined(NDEBUG)
#define DEBUG_CHECK(err) \
    do { \
        if (err) { \
            RAISE(orion::ErrorCode::assert, "assert failed: %s", #err); \
        } \
    } while (0)
#else
#define DEBUG_CHECK(condition)
#endif // !defined(NDEBUG)

#define CHECK_LOG_1(condition) \
    do { \
        if (!(condition)) { \
            orion::Error verify_err_macro_variable__; \
            verify_err_macro_variable__ = MAKE_ERROR(orion::ErrorCode::assert, "check failed: %s", #condition); \
            orion::log_error("%s", verify_err_macro_variable__.to_string().c_str()); \
            return; \
        } \
    } while (0)

#define CHECK_LOG_2(condition, message) \
    do { \
        if (!(condition)) { \
            orion::Error verify_err_macro_variable__; \
            verify_err_macro_variable__ = MAKE_ERROR(orion::ErrorCode::assert, "check failed: %s (message: %s)", #condition, message); \
            orion::log_error("%s", verify_err_macro_variable__.to_string().c_str()); \
            return; \
        } \
    } while (0)

#define CHECK_LOG(...) PP_EXPAND(PP_GET_MACRO_2(__VA_ARGS__, CHECK_LOG_2, CHECK_LOG_1)(__VA_ARGS__))

#define RAISE_CHECK_1(condition) \
    do { \
        if (!(condition)) { \
            RAISE(orion::ErrorCode::assert, "assertion failed: %s", #condition); \
        } \
    } while (0)

#define RAISE_CHECK_2(condition, message) \
    do { \
        if (!(condition)) { \
            RAISE(orion::ErrorCode::assert, "assertion failed: %s (message: %s)", #condition, message); \
        } \
    } while (0)

#define RAISE_CHECK(...) PP_EXPAND(PP_GET_MACRO_2(__VA_ARGS__, RAISE_CHECK_2, RAISE_CHECK_1)(__VA_ARGS__))

#define RAISE_IF_1(condition) \
    do { \
        if (condition) { \
            RAISE(orion::ErrorCode::assert, "error detected: %s", #condition); \
        } \
    } while (0)

#define RAISE_IF_2(condition, message) \
    do { \
        if (condition) { \
            RAISE(orion::ErrorCode::assert, "error detected: %s (message: %s)", #condition, message); \
        } \
    } while (0)

#define RAISE_IF(...) PP_EXPAND(PP_GET_MACRO_2(__VA_ARGS__, RAISE_IF_2, RAISE_IF_1)(__VA_ARGS__))

#define SUCCEED() \
    do { \
        return OK; \
    } while (0)

namespace orion {

struct FileLine {
    std::string file;
    int line;

    FileLine() = default;

    FileLine(const char* f, int l) : file(std::string(f)), line(l) {
    }
};

class Error {
public:
    Error() = default; // default constructed: success
    Error(const Error& err) = default;
    Error(Error&& err) = default;
    Error& operator=(const Error& err) = default;

    Error(ErrorCode error_code, const char* file, int line, const char* format, ...);
    Error(ErrorCode error_code, const std::string& message, const char* file, int line);

    ~Error() {
    }

    const std::vector<FileLine>& trace() const { return m_trace; }
    ErrorCode code() const { return m_code; }
    std::string code_name() const;
    std::string message() const { return m_message; }
    void set_message(const std::string& message) { m_message = message; }
    std::string to_string() const; // debug info with stack trace
    explicit operator bool() const { return m_code != ErrorCode::ok; }
    static Error ok();
    bool operator==(const Error& other) const; // ignores stack trace
    bool operator!=(const Error& other) const; // ignores stack trace
    
private:
    ErrorCode m_code = ErrorCode::ok; // can be used to categorize errors
    std::string m_message;
    std::vector<FileLine> m_trace;
    
    friend Error make_error(ErrorCode error_code, const char* file, int line, bool with_log, const char* format, ...);
    friend Error wrap_error(Error err, const char* file, int line);
};

Error make_error(ErrorCode error_code, const char* file, int line, bool with_log, const char* format, ...);
Error wrap_error(Error err, const char* file, int line);

#define OK orion::Error() // default constructed: success

std::string sprintf_to_string(const char* format, ...);

} // namespace orion
