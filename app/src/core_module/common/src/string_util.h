#pragma once

#include <string>
#include <vector>
#include <deque>
#include <src/error.h>

namespace orion::str {

std::string format(const char *fmt, ...);

// Trims leading and trailing whitespace in-place
void trim(std::string* s);

// Trims leading whitespace in-place
void ltrim(std::string* s);

// Trims trailing whitespace in-place
void rtrim(std::string* s);

// Converts for example s="abc", n=3 to "abcabcabc"
std::string repeat(const std::string& s, size_t n);

std::string join_strings(const std::vector<std::string>& parts, const std::string& sep);

bool contains_case_insensitive(const std::string& haystack, const std::string& needle);

bool equals_case_insensitive(const std::string& a, const std::string& b);

Error try_parse_int64(const std::string& s, int64_t* value);

std::string ptr_to_hex(const void* ptr);

std::deque<std::string> split(const std::string& s, char delim);

std::string join(const std::deque<std::string>& parts, char delim);

void to_lower(std::string* s);

void to_upper(std::string* s);

bool is_ascii(const std::string& s);

void replace_all(std::string* in_out_str, const std::string& from, const std::string& to);

//return type is based on ngettext requirement
unsigned long count_occurences_of_char(const std::string_view string, char c);

std::string escape_regex(const std::string& s);

std::string remove_all_occurrences_of_substring_in_string(const std::string& original, std::string_view substring);
struct StringHash {
    using is_transparent = void;

    size_t operator()(std::string_view view) const {
        return std::hash<std::string_view>{}(view);
    }
};

using PlaceholderMap = std::unordered_map<std::string, std::string, StringHash, std::equal_to<>>;

std::string
parse_string_prepend_append_and_replace_placeholders(const std::string_view string, const std::string_view prefix,
    const std::string_view suffix, const PlaceholderMap& dictionary, bool escape_backticks);

//includes quotes before and after
std::string escape_string_for_js(std::string_view str);

//NULL safe
size_t strnlen0(const char* str, size_t max_len);

//replacement is also prepended before the entire string
constexpr std::string copy_and_replace_newlines(const std::string_view view, char replacement)
{
    if(!view.length()) return "";

    std::string result;
    result.reserve(view.length() + 1);

    result.push_back(replacement);

    for(char c : view)
    {
        if(c == '\r') continue;
        result.push_back(c == '\n' ? replacement : c);
    }

    return result;
}

consteval std::size_t static_format_length(const std::string_view fmt)
{
    //counts all non placeholder characters, but counts a double brace as 1 character
    std::size_t length = 0;
    int brace_depth = 0;
    
    for (std::size_t i = 0; i < fmt.size(); ++i) {
        
        if (fmt[i] == '{') {
            if (brace_depth == 0 && i + 1 < fmt.size() && fmt[i + 1] == '{') {
                length++;
                i++; //double brace outside of placeholder, skip second
            } else {
                brace_depth++; //start of placeholder
            }
        } else if (fmt[i] == '}') {
            if (brace_depth == 0 && i + 1 < fmt.size() && fmt[i + 1] == '}') {
                length++; 
                i++; //double brace outside of placeholder, skip second
            }
            else
            {
                brace_depth--;
            }
        } else {
            if (brace_depth == 0) {
                length++; //normal character outside of placeholder
            } 
        }
    }
    return length;
}

std::string
strip_diacritics(const std::string& input);
} // namespace orion::str