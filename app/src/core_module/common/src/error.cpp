#include <src/error.h>
#include <cstdarg>

namespace orion {

std::string
format_string(const char* format, va_list args)
{
    va_list args_copy;
    va_copy(args_copy, args);

    // Get required size
    int size = vsnprintf(nullptr, 0, format, args_copy);
    va_end(args_copy);

    if (size < 0) {
        // Formatting error
        return "<formatting-error>";
    }

    std::vector<char> buffer(size + 1); // +1 for null terminator
    vsnprintf(buffer.data(), buffer.size(), format, args);

    return std::string(buffer.data(), size); // exclude null terminator
}

std::string
format_string(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    std::string result = format_string(format, args);
    va_end(args);
    return result;
}

Error::Error(ErrorCode error_code, const char* file, int line, const char* format, ...)
    : m_code(error_code)
{
    va_list args;
    va_start(args, format);
    m_message = format_string(format, args);
    va_end(args);

    m_trace.push_back(FileLine(file, line));
}

Error::Error(ErrorCode error_code, const std::string& message, const char* file, int line)
    : m_code(error_code), m_message(message)
{
    m_trace.push_back(FileLine(file, line));
}

std::string
Error::to_string() const
{
    if (m_code == ErrorCode::ok) {
        return this->code_name();
    }

    std::stringstream ss;
    // Strip /home/user/workspaces/orion-linux/ prefix from file path if needed
    ss << m_message;
#ifdef _WIN32
    // On Windows, to make the file paths clickable in debug output, we can't have any text before the filename.
    // We also need file(line)
    // I kept the "  at " on Linux because it is more clear.
    if (!m_trace.empty()) {
        ss << " at:";
    }
    for (const auto& fl : m_trace) {
        ss << "\n" << fl.file << "(" << fl.line << ")";
    }
#else
    for (const auto& fl : m_trace) {
        ss << "\n  at " << fl.file << ":" << fl.line;
    }
#endif
    return ss.str();
}

static Error g_ok = Error();

Error
Error::ok()
{
    return g_ok;
}

std::string
Error::code_name() const
{
    auto it = g_error_code_names.find(m_code);
    if (it == g_error_code_names.end()) {
        return std::to_string((int)m_code);
    }
    return std::string("ErrorCode::") + it->second;
}

bool
Error::operator==(const Error& other) const
{
    return m_code == other.m_code && m_message == other.m_message;
}

bool
Error::operator!=(const Error& other) const
{
    return !(*this == other);
}

Error
make_error(ErrorCode error_code, const char* file, int line, bool with_log, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    std::string message = format_string(format, args);
    va_end(args);

    auto err = Error(error_code, message, file, line);
    if (with_log) {
        orion::log_error("%s", err.to_string().c_str());
    }
    return err;
}

Error
wrap_error(Error err, const char* file, int line)
{
    Error copy(err);
    copy.m_trace.push_back(FileLine(file, line));
    return copy; // copy ellision, do not call std::move which would prevent it
}

std::string
sprintf_to_string(const char* format, ...)
{
    va_list args;
    va_start(args, format);

    // Determine required size
    va_list args_copy;
    va_copy(args_copy, args);
    int size = std::vsnprintf(nullptr, 0, format, args_copy);
    va_end(args_copy);

    if (size < 0) {
        va_end(args);
        return "<error:sprintf_to_string>";
    }

    // Allocate string of required size
    std::string result(size, '\0');
    std::vsnprintf(result.data(), result.size() + 1, format, args);

    va_end(args);
    return result;
}

} // namespace orion
