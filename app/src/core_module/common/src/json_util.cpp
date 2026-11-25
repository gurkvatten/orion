#include <src/json_util.h>

namespace orion {


// JSON field escaping/unescaping -------------------------------------------------

std::string
escape_json_field(const std::string& value)
{
    std::string out;
    out.reserve(value.size() + 2);
    out.push_back('"');

    for (unsigned char c : value) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\b': out += "\\b"; break;
        case '\f': out += "\\f"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (c < 0x20) {
                char buf[7];
                // control characters are written as \u00XX
                std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                out += buf;
            } else {
                out.push_back(c);
            }
        }
    }

    out.push_back('"');
    return out;
}

static std::string
scan_hex(const std::string& hex)
{
    unsigned int code = 0;
    std::string out;
    if (std::sscanf(hex.c_str(), "%x", &code) == 1) {
        if (code <= 0x7f) {
            out.push_back(static_cast<char>(code));
        } else if (code <= 0x7ff) {
            out.push_back(static_cast<char>(0xc0 | (code >> 6)));
            out.push_back(static_cast<char>(0x80 | (code & 0x3f)));
        } else {
            out.push_back(static_cast<char>(0xe0 | (code >> 12)));
            out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3f)));
            out.push_back(static_cast<char>(0x80 | (code & 0x3f)));
        }
    }
    return out.empty() ? "?" : out;
}

std::string
unescape_json_field(const std::string& value)
{
    // strip surrounding quotes if any
    size_t start = 0;
    size_t end = value.size();
    if (end >= 2 && value.front() == '"' && value.back() == '"') {
        start = 1;
        end -= 1;
    }

    std::string out;
    out.reserve(end - start);

    for (size_t i = start; i < end; ++i) {
        char c = value[i];
        if (c == '\\' && i + 1 < end) {
            char nxt = value[++i];
            switch (nxt) {
            case '"': out.push_back('"'); break;
            case '\\': out.push_back('\\'); break;
            case '/': out.push_back('/'); break;
            case 'b': out.push_back('\b'); break;
            case 'f': out.push_back('\f'); break;
            case 'n': out.push_back('\n'); break;
            case 'r': out.push_back('\r'); break;
            case 't': out.push_back('\t'); break;
            case 'u': {
                // unicode escape; expect 4 hex digits
                if (i + 4 < end) {
                    std::string hex = value.substr(i + 1, 4);
                    out += scan_hex(hex);
                    i += 4;
                } else {
                    // malformed, just append literal 'u'
                    out.push_back('u');
                }
                break;
            }
            default:
                out.push_back(nxt);
            }
        } else {
            out.push_back(c);
        }
    }

    return out;
}

char
unescape_json_char(const char in, UnescapeState& state, std::string& hex_in_out)
{
    if (in == '\\' && state == Normal) {
        state = InEscape;
        return 0; // signal to caller that we are now in escape sequence
    } else if (state == InEscape) {
        switch (in) {
        case '"': return '"';
        case '\\': return '\\';
        case '/': return '/';
        case 'b': return '\b';
        case 'f': return '\f';
        case 'n': return '\n';
        case 'r': return '\r';
        case 't': return '\t';
        case 'u':
            state = InHex;
            hex_in_out.clear();
        default:
            state = Normal; //unklnown escape sequence, Error?
            return in;
        }
    } else if (state == InHex) {
        hex_in_out.push_back(in);
        if (hex_in_out.size() == 4) {
            hex_in_out = scan_hex(hex_in_out);
            state = Normal;
        } else {
            return 0; // signal to caller that we are still in hex sequence
        }
    } else if (state == Normal && in == '"' ) {
        state = End;
        return 0;
    } else {
        state = Normal;
    }
    return in;
}

} // namespace orion