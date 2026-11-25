#pragma once

#include <regex>
#include <string>
#include <map>

namespace orion {

class Uri {
public:
    enum class ParseType { strict, user_input };

    // For external, add them here (mailto, geo, sip, tel, sms, etc.) as needed.
    enum class Scheme {
        empty, http, https, ftp, mailto, file, orion,
        chrome_extension, moz_extension, unsupported_extension,
        extension_proxy, unknown, gresource_font,
    };
    
    static Uri parse_strict(const char* uri);
    static Uri parse_user_input(const char* uri);
    static Uri parse(const char* uri, ParseType parseType = ParseType::strict);
    Scheme known_scheme() const;
    const std::string& uri() const;       // for example: https://www.example.com/path?query#fragment
    const std::string& scheme() const;    // for example: https
    const std::string& authority() const; // for example: www.example.com
    const std::string& path() const;      // for example: /path/to/resource
    const std::string& query() const;     // for example: key=value&otherkey=value
    const std::string& fragment() const;  // for example: section
    Uri copy_with_path(const std::string& path) const;
    std::string to_string() const;

public:
    static std::string encode(const std::string& value);

private:
    std::string m_uri = "";
    std::map<int, std::string> m_uri_parts;
};

} // namespace orion