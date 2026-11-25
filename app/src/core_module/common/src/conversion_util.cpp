#include <src/conversion_util.h>
#include <src/csv_util.h>
#include <src/json_util.h>
#include <vector>

/*
base64 check 
https://stackoverflow.com/questions/8571501/how-to-check-whether-a-string-is-base64-encoded-or-not

In base64 encoding, the character set is [A-Z, a-z, 0-9, and + /]. If the rest length is less than 4, the string is padded with '=' characters.
^([A-Za-z0-9+/]{4})*([A-Za-z0-9+/]{3}=|[A-Za-z0-9+/]{2}==)?$ => the whole string is base64 encoded

^([A-Za-z0-9+/]{4})* => the string starts with 0 or more base64 groups.

([A-Za-z0-9+/]{4}|[A-Za-z0-9+/]{3}=|[A-Za-z0-9+/]{2}==)$ => the string ends in one of three forms:
*/
namespace orion {
static const char* base64_chars =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

std::string
base64_encode(const unsigned char* bytes, size_t len)
{
    std::string ret;
    ret.reserve(((len + 2) / 3) * 4);

    size_t i = 0;
    while (i + 2 < len) {
        unsigned int triple = (bytes[i] << 16) | (bytes[i+1] << 8) | bytes[i+2];
        ret.push_back(base64_chars[(triple >> 18) & 0x3F]);
        ret.push_back(base64_chars[(triple >> 12) & 0x3F]);
        ret.push_back(base64_chars[(triple >> 6) & 0x3F]);
        ret.push_back(base64_chars[triple & 0x3F]);
        i += 3;
    }

    if (i < len) {
        unsigned int triple = 0;
        int remaining = 0;
        triple |= bytes[i] << 16; remaining = 1;
        if (i + 1 < len) { triple |= bytes[i+1] << 8; remaining = 2; }

        ret.push_back(base64_chars[(triple >> 18) & 0x3F]);
        ret.push_back(base64_chars[(triple >> 12) & 0x3F]);
        if (remaining == 2) {
            ret.push_back(base64_chars[(triple >> 6) & 0x3F]);
            ret.push_back('=');
        } else {
            ret.push_back('=');
            ret.push_back('=');
        }
    }

    return ret;
}

std::string
base64_encode(const std::string& data)
{
    return base64_encode((const unsigned char*)data.data(), data.size());
}

std::string
base64_decode(const std::string& input)
{
    std::vector<int> T(256, -1);
    for (int i = 0; i < 64; ++i) T[(unsigned char)base64_chars[i]] = i;

    std::string out;
    out.reserve((input.size() * 3) / 4);

    int val = 0;
    int valb = -8;
    for (unsigned char c : input) {
        if (T[c] == -1) {
            if (c == '=') break;
            else continue; // skip invalid/whitespace
        }
        val = (val << 6) + T[c];
        valb += 6;
        if (valb >= 0) {
            out.push_back(char((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return out;
}

std::string
base64url_decode(const std::string& encoded)
{
    // Convert base64url to standard base64
    std::string b64 = encoded;
    
    // Replace base64url characters with standard base64
    for (auto& c : b64) {
        if (c == '-') c = '+';
        else if (c == '_') c = '/';
    }
    
    // Add padding if needed
    size_t padding = (4 - (b64.size() % 4)) % 4;
    for (size_t i = 0; i < padding; ++i) {
        b64.push_back('=');
    }
    
    return base64_decode(b64);
}

std::string
base64url_encode(const std::string& data)
{
    std::string b64 = base64_encode(data);
    
    // Replace standard base64 characters with base64url
    for (auto& c : b64) {
        if (c == '+') c = '-';
        else if (c == '/') c = '_';
    }
    
    // Remove padding
    while (!b64.empty() && b64.back() == '=') {
        b64.pop_back();
    }
    
    return b64;
}

static std::string
html_escape(const std::string& text)
{
    std::string escaped;
    escaped.reserve(text.size());
    for (char c : text) {
        switch (c) {
            case '&':  escaped += "&amp;"; break;
            case '<':  escaped += "&lt;"; break;
            case '>':  escaped += "&gt;"; break;
            case '"':  escaped += "&quot;"; break;
            case '\'': escaped += "&#39;"; break;
            default:   escaped += c;
        }
    }
    return escaped;
}

static std::vector<std::string> 
csv_parse_line(const std::string& line, Error& error)
{
    std::vector<std::string> fields;
    std::vector<std::vector<std::string>> rows;
    error = parse_csv(line, ',', &rows);
    if (error) {
        return {};
    }
    if (rows.size() == 1) {
        fields = std::move(rows[0]);
    } else {
        error = MAKE_ERROR(ErrorCode::invalid_format, "Expected a single line of CSV input");
    }
    return fields;
}

std::string
csv_to_html(const std::string& csv_data, Error& error)
{
    std::string html;
    html += "<table border=\"1\" cellpadding=\"5\" cellspacing=\"0\">\n";

    std::istringstream stream(csv_data);
    std::string line;
    bool is_header = true;

    while (std::getline(stream, line)) {
        // Skip empty lines
        if (line.empty()) continue;

        std::vector<std::string> fields = csv_parse_line(line, error);
        if (error) {
            return {};
        }

        if (is_header) {
            html += "  <thead>\n    <tr>\n";
            for (const auto& field : fields) {
                html += "      <th>";
                html += html_escape(field);
                html += "</th>\n";
            }
            html += "    </tr>\n  </thead>\n";
            html += "  <tbody>\n";
            is_header = false;
        } else {
            html += "    <tr>\n";
            for (const auto& field : fields) {
                html += "      <td>";
                html += html_escape(field);
                html += "</td>\n";
            }
            html += "    </tr>\n";
        }
    }

    html += "  </tbody>\n";
    html += "</table>\n";
    return html;
}

Error
json_array_to_csv(const std::string& json_str, char delimiter, std::function<void(const std::string& row)> row_cb)
{
    RAISE_CHECK(row_cb);
    if (json_str.empty()) {
        return MAKE_ERROR(ErrorCode::argument, "JSON is empty");
    }
    enum State { Normal, InArray, InValue, InQuotes };
    static constexpr char double_quote_mark = '"';
    static constexpr char sepparator_mark = ',';
    static constexpr int expected_dimension = 2;

    int rows_count = 0;
    int array_dimension = 0;
    char expected_char = 0;
    std::string header;
    std::string current_row;

    State state = Normal;
    UnescapeState escape_state = UnescapeState::Normal;
    
    std::string key, value, hex_buffer;
    hex_buffer.reserve(4);
    for (size_t i = 0; i < json_str.size(); i++) {
        char c = json_str[i];
        switch (state) {
            case Normal:
                if (expected_char != 0 && c == expected_char) {
                    expected_char = 0;
                } else if (c == '[' && expected_char == 0) {
                    if (++array_dimension == expected_dimension) {
                        state = InArray;
                    } else if (array_dimension > expected_dimension) {
                        return MAKE_ERROR(ErrorCode::invalid_format, "Unexpected array dimension > %d at index: %zu", expected_dimension, i);
                    }
                } else if (c == ']') {
                    if (--array_dimension == 0) {
                        // End of 2D array
                        SUCCEED();
                    } else {
                        return MAKE_ERROR(ErrorCode::invalid_format, "Unexpected closing array at index: %zu", i);
                    }
                } else if (!isspace(c)) {
                    return MAKE_ERROR(ErrorCode::invalid_format, "Expected whitespace at index: %zu", i);
                }
                break;
            case InArray:
                if (c == ']') {
                    // End of array row
                    if (!current_row.empty()) {
                        row_cb(current_row);
                        current_row.clear();
                        rows_count++;
                    } else if (rows_count == 0) {
                        return MAKE_ERROR(ErrorCode::invalid_format, "Empty row at index: %zu, first row is expected to be the header", i);
                    }
                    state = Normal;
                    value.clear();
                    expected_char = sepparator_mark;
                    --array_dimension; // close the current array
                } else if (c == sepparator_mark) {
                    // End of value
                    current_row += delimiter;
                    value.clear();
                } else if (!isspace(c)) {
                    if (c == double_quote_mark) {
                        escape_state = UnescapeState::Normal;
                        state = InQuotes;
                    } else {
                        //assuming a start of unquoted value
                        state = InValue;
                        value += c;
                    }
                }
                break;
            case InValue:
                if (c == double_quote_mark) {
                    if (value.empty()) {
                        //empty value, we treat it as quoted value, we will end up with empty string as value but that's fine since it's what we expect for empty value.
                        escape_state = UnescapeState::Normal;
                        state = InQuotes;
                    } else {
                        //unexpected quote in unquoted value
                        return MAKE_ERROR(ErrorCode::invalid_format, "Unexpected quote in unquoted value at index: %zu", i);
                    }
                } else if (c == sepparator_mark || c == ']') {
                    state = InArray;
                    --i; // re-process the char in the array state
                    current_row += value;
                } else if (isspace(c)) {
                    return MAKE_ERROR(ErrorCode::invalid_format, "Unexpected whitespace in unquoted value at index: %zu", i);
                } else {
                    //assuming unquoted value => shouldn't need escaping
                    value += c;
                }
                break;
            case InQuotes: {
                if (expected_char != 0) {
                    if (c == expected_char || c == ']') {
                        expected_char = 0;
                        --i; // re-process the char in the InArray state
                        state = InArray;
                    } else if (!isspace(c)){
                        return MAKE_ERROR(ErrorCode::invalid_format, "Expected char '%c' or whitespace at index: %zu", expected_char, i);
                    }
                } else {
                    char out = unescape_json_char(c, escape_state, hex_buffer);
                    if (out != 0) {
                        value += out;
                    } else if (escape_state == UnescapeState::End) {
                        current_row +=  escape_csv_field(value, delimiter);
                        expected_char = sepparator_mark;
                    } else if (escape_state == UnescapeState::InError) {
                        return MAKE_ERROR(ErrorCode::invalid_format, "failed to parse value: %s for key: %s of JSON object at index: %zu", value.c_str(), key.c_str(), i);
                    }
                }
                break;
            }
        }
    }
    return MAKE_ERROR(ErrorCode::invalid_format, "Unexpected end on JSON array input");
}

} // namespace orion
