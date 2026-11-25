#include <src/string_util.h>
#include <algorithm>
#include <sstream>
#include <map>

#include <rfl.hpp>
#include <rfl/json.hpp>

#ifdef ORION_COMMON_USE_GLIB
#include <glib.h>
#endif

namespace orion::str {

std::string
format(const char *fmt, ...)
{
    va_list args_copy;
    va_start(args_copy, fmt);
    int size = vsnprintf(nullptr, 0, fmt, args_copy);
    va_end(args_copy);

    std::string result(size, '\0');
    va_list args;
    va_start(args, fmt);
    vsnprintf(&result[0], size + 1, fmt, args);
    va_end(args);
    return result;
}

// Trims leading and trailing whitespace in-place
void
trim(std::string* s)
{
    // Trim leading
    s->erase(s->begin(), std::find_if(s->begin(), s->end(),
        [](unsigned char ch) { return !std::isspace(ch); }));

    // Trim trailing
    s->erase(std::find_if(s->rbegin(), s->rend(),
        [](unsigned char ch) { return !std::isspace(ch); }).base(), s->end());
}

void
ltrim(std::string* s)
{
    s->erase(s->begin(), std::find_if(s->begin(), s->end(),
        [](unsigned char ch) { return !std::isspace(ch); }));
}

void
rtrim(std::string* s)
{
    s->erase(std::find_if(s->rbegin(), s->rend(),
        [](unsigned char ch) { return !std::isspace(ch); }).base(), s->end());
}

std::string
repeat(const std::string& s, size_t n)
{
    std::string result;
    result.reserve(s.size() * n); // optional, avoids reallocations
    for (size_t i = 0; i < n; ++i) {
        result += s;
    }
    return result;
}

std::string
join_strings(const std::vector<std::string>& parts, const std::string& sep)
{
    std::string result;
    for (size_t i = 0; i < parts.size(); ++i) {
        result += parts[i];
        if (i + 1 < parts.size()) {
            result += sep;
        }
    }
    return result;
}

bool
contains_case_insensitive(const std::string& haystack, const std::string& needle)
{
    auto it = std::search(
        haystack.begin(), haystack.end(),
        needle.begin(), needle.end(),
        [](unsigned char ch1, unsigned char ch2) { return std::toupper(ch1) == std::toupper(ch2); }
    );
    return (it != haystack.end());
}

bool
equals_case_insensitive(const std::string& a, const std::string& b)
{
    return a.size() == b.size() &&
        std::equal(a.begin(), a.end(), b.begin(),
            [](unsigned char ch1, unsigned char ch2) { return std::toupper(ch1) == std::toupper(ch2); }
        );
}

Error
try_parse_int64(const std::string& s, int64_t* value)
{
    if (!value) {
        RAISE(ErrorCode::argument, "value pointer is null");
    }
    char* endptr = nullptr;
    errno = 0; // reset errno before the call

    int64_t v = static_cast<int64_t>(strtoll(s.c_str(), &endptr, 10));

    if ((v == LLONG_MAX || v == LLONG_MIN) && errno == ERANGE) {
        RAISE(ErrorCode::invalid_type, "failed to parse int64 from string (out of range): '%s'", s.c_str());
    }

    if (endptr == s.c_str()) {
        RAISE(ErrorCode::invalid_type, "failed to parse int64 from string (no digits found): '%s'", s.c_str());
    }

    if (*endptr != '\0') {
        RAISE(ErrorCode::invalid_type, "failed to parse int64 from string (extra characters found): '%s'", s.c_str());
    }

    *value = v;
    SUCCEED();
}

std::string
ptr_to_hex(const void* ptr)
{
    std::stringstream ss;
    ss << "0x" << std::hex << reinterpret_cast<uintptr_t>(ptr);
    return ss.str();
}

std::deque<std::string>
split(const std::string& s, char delim)
{
    std::deque<std::string> out;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim))
        out.push_back(item);
    return out;
}

std::string
join(const std::deque<std::string>& parts, char delim)
{
    std::string out = parts.empty() ? "" : parts[0];
    for (size_t i = 1; i < parts.size(); ++i) {
        out.push_back(delim);
        out += parts[i];
    }
    return out;
}

void
to_lower(std::string* s)
{
    std::transform(s->begin(), s->end(), s->begin(), [](unsigned char c){ return std::tolower(c); });
}

void
to_upper(std::string* s)
{
    std::transform(s->begin(), s->end(), s->begin(), [](unsigned char c){ return std::toupper(c); });
}

bool
is_ascii(const std::string& s)
{
    for (char c : s) {
        if (static_cast<unsigned char>(c) > 127) {
            return false;
        }
    }
    return true;
}

void
replace_all(std::string* in_out_str, const std::string& from, const std::string& to)
{
    if (in_out_str == nullptr) {
        return;
    }

    if (from.empty()) {
        return;
    }

    size_t start_pos = 0;
    while ((start_pos = in_out_str->find(from, start_pos)) != std::string::npos) {
        in_out_str->replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
}

unsigned long
count_occurences_of_char(const std::string_view string, char c)
{
    return std::count(string.cbegin(), string.cend(), c);
}

std::string
escape_regex(const std::string& s)
{
    static const std::string special = R"(\.^$|()[]{}*+?)";
    std::string result;
    result.reserve(s.size() * 2);

    for (char c : s) {
        if (special.find(c) != std::string::npos) {
            result.push_back('\\');
        }
        result.push_back(c);
    }
    return result;
}

static void
append_escaped(std::string& destination, std::string_view source)
{
    size_t current = 0;
    while(current < source.size())
    {
        size_t quote_pos = source.find('`', current);

        if(quote_pos == std::string_view::npos)
        {
            destination.append(source.substr(current));
            break;
        }
        else
        {
            destination.append(source.substr(current, quote_pos - current));
            destination.append("\\`");
            current = quote_pos + 1;
        }
    }
}

std::string
remove_all_occurrences_of_substring_in_string(const std::string& original, std::string_view substring)
{
    if (substring.empty()) {
        return original;
    }

    std::string result;
    
    //NOTE: usefulness of this depends on your workload, maybe should add an enum that calculates whether we 
    //result.reserve(original.size());

    std::size_t start = 0;
    std::size_t pos;

    //finds position of next occurrence, appends a chunk from start to pos (exclusive),
    //starts search again in the position after the substring occurence
    while ((pos = original.find(substring, start)) != std::string_view::npos) {
        result += original.substr(start, pos - start);
        
        start = pos + substring.size();
    }

    //appends whatever was left over
    result += original.substr(start);

    return result;
}

std::string
parse_string_prepend_append_and_replace_placeholders(const std::string_view template_str, const std::string_view prefix,
    const std::string_view suffix, const PlaceholderMap& dictionary, bool escape_backticks)
{
    size_t max_len = 0;
    for(auto& pair : dictionary)
    {
        max_len = pair.first.length()>max_len ? pair.first.length() : max_len;
    }

    std::string result;

    result.reserve(template_str.size()*1.05 + prefix.size() + suffix.size()); //magic constant to reduce reallocations

    result.append(prefix);

    size_t current = 0;
    while(current < template_str.size())
    {
        size_t start_pos = template_str.find("%", current);
        
        if(start_pos == std::string_view::npos)
        {
            if(escape_backticks) append_escaped(result, template_str.substr(current));
            else result.append(template_str.substr(current));
            break;
        }

        if(escape_backticks) append_escaped(result, template_str.substr(current, start_pos - current));
        else result.append(template_str.substr(current, start_pos-current));

        size_t end_pos = template_str.find("%", start_pos + 1);

        if(end_pos == std::string_view::npos)
        {
            if(escape_backticks) append_escaped(result, template_str.substr(start_pos));
            else result.append(template_str.substr(start_pos));
            break;
        }

        size_t key_len = (end_pos-1) - (start_pos);
        if(key_len > max_len)
        {
            if(escape_backticks) append_escaped(result, template_str.substr(start_pos, end_pos - start_pos));
            else result.append(template_str.substr(start_pos, end_pos - start_pos));
            current = end_pos;
            continue;
        }

        std::string_view key = template_str.substr(start_pos + 1, key_len);
        auto it = dictionary.find(key);

        if(it != dictionary.end())
        {
            if(escape_backticks) append_escaped(result, it->second);
            else result.append(it->second);
            current = end_pos + 1;
        }
        else
        {
            if(escape_backticks) append_escaped(result, template_str.substr(start_pos, end_pos - start_pos));
            else result.append(template_str.substr(start_pos, end_pos - start_pos));
            current = end_pos;
        }
    }
    result.append(suffix);
    return result;
}

std::string
escape_string_for_js(std::string_view str)
{
    return rfl::json::write(str);
}

std::string
strip_diacritics(const std::string& input)
{
#ifdef ORION_COMMON_USE_GLIB
    gchar* normalized = g_utf8_normalize(input.c_str(), -1, G_NORMALIZE_NFD);
    GString* result = g_string_new(NULL);

    for (const gchar* p = normalized; *p; p = g_utf8_next_char(p)) {
        gunichar c = g_utf8_get_char(p);

        if (!g_unichar_combining_class(c)) {
            g_string_append_unichar(result, c);
        }
    }

    g_free(normalized);
    return g_string_free(result, FALSE);
#else
    return input;
#endif
}

size_t strnlen0(const char* str, size_t max_len)
{
    if(str)
    {
        return strnlen(str, max_len);
    }
    else
    {
        return 0;
    }
}

} // namespace orion::str