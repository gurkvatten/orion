#include <src/uri.h>
#include <sstream>
#include <iomanip>

namespace orion {

enum UriPart {
    scheme_ex    = 1,
    scheme       = 2,
    authority_ex = 3,
    authority    = 4,
    path         = 5,
    query_ex     = 6,
    query        = 7,
    fragment_ex  = 8,
    fragment     = 9,
};

static const std::string&
empty_string()
{
    static std::string empty = "";
    return empty;
}

static const std::string&
get_part(const std::map<int, std::string>& uri_parts, UriPart part)
{
    auto it = uri_parts.find(part);
    return it != uri_parts.end() ? it->second : empty_string();
}

Uri
Uri::parse_strict(const char* uri)
{
    Uri result;
    result.m_uri = uri ? uri : "";
    unsigned counter = 0;
    //https://datatracker.ietf.org/doc/html/rfc3986#appendix-B
    std::regex uri_regex(R"(^(([^:\/?#]+):)?(//([^\/?#]*))?([^?#]*)(\?([^#]*))?(#(.*))?)", std::regex::extended);
    std::smatch url_match_result;
    if (std::regex_match(result.m_uri, url_match_result, uri_regex)) {
        for (const auto& res : url_match_result) {
            std::string x = !res.matched || res.length() == 0 ? std::string("") : res;
            result.m_uri_parts[(UriPart)counter++] = x;
        }
    }
    return result;
}

Uri
Uri::parse_user_input(const char* uri)
{
    std::string uri_string = std::string(uri ? uri : "");
    const Uri default_uri = parse_strict(uri);
    switch(default_uri.known_scheme()) {
        case Scheme::ftp:
        case Scheme::file:
        case Scheme::http:
        case Scheme::https:
        case Scheme::chrome_extension:
        case Scheme::moz_extension:
        case Scheme::unsupported_extension:
        case Scheme::extension_proxy:
        case Scheme::orion:
            break;
        default:
        uri_string = "https://" + uri_string;
            break;
    }
    const Uri scheme_uri = parse_strict(uri_string.c_str());
    switch(scheme_uri.known_scheme()) {
        case Scheme::ftp:
        case Scheme::file:
        case Scheme::http:
        case Scheme::https:
        case Scheme::chrome_extension:
        case Scheme::moz_extension:
        case Scheme::unsupported_extension:
        case Scheme::extension_proxy:
        case Scheme::orion:
            break;
        default:
            return default_uri;
    }
    if(scheme_uri.authority() == "") {
        return default_uri;
    }
    if(scheme_uri.path() != "") {
        return scheme_uri;
    }
    return scheme_uri.copy_with_path("/");
}

Uri
Uri::parse(const char* uri, ParseType parseType)
{
    switch (parseType)
    {
    case ParseType::strict:
        return parse_strict(uri);
    case ParseType::user_input:
        return parse_user_input(uri);
    }
    return Uri();
}

Uri::Scheme
Uri::known_scheme() const
{
    static std::map<std::string, Scheme> s_scheme_mapping = {
        { "", Scheme::empty },
        { "http", Scheme::http },
        { "https", Scheme::https },
        { "ftp", Scheme::ftp },
        { "mailto", Scheme::mailto },
        { "file", Scheme::file },
        { "orion", Scheme::orion },
        { "chrome-extension", Scheme::chrome_extension },
        { "moz-extension", Scheme::moz_extension },
        { "unsupported-extension", Scheme::unsupported_extension },
        { "extension-proxy", Scheme::extension_proxy },
        { "gresource-font", Scheme::gresource_font },
    };
    auto it = s_scheme_mapping.find(scheme());
    return it != s_scheme_mapping.end() ? it->second: Scheme::unknown;
}

const std::string&
Uri::uri() const
{
    return m_uri;
}

const std::string&
Uri::scheme() const
{
    return get_part(m_uri_parts, UriPart::scheme);
}

const std::string&
Uri::authority() const
{
    return get_part(m_uri_parts, UriPart::authority);
}

const std::string&
Uri::path() const
{
    return get_part(m_uri_parts, UriPart::path);
}

const std::string&
Uri::query() const
{
    return get_part(m_uri_parts, UriPart::query);
}

const std::string&
Uri::fragment() const
{
    return get_part(m_uri_parts, UriPart::fragment);
}

Uri
Uri::copy_with_path(const std::string& path) const
{
    std::string uri = 
        get_part(m_uri_parts, UriPart::scheme_ex) +
        get_part(m_uri_parts, UriPart::authority_ex) +
        path +
        get_part(m_uri_parts, UriPart::query_ex) +
        get_part(m_uri_parts, UriPart::fragment_ex);
    return parse_strict(uri.c_str());
}

std::string
Uri::to_string() const
{
    std::stringstream ss;
    ss << "Uri {";
    ss << "uri=\"" << m_uri << "\"";
    ss << ", scheme=\"" << scheme() << "\"";
    ss << ", authority=\"" << authority() << "\"";
    ss << ", path=\"" << path() << "\"";
    ss << ", query=\"" << query() << "\"";
    ss << ", fragment=\"" << fragment() << "\"";
    ss << " }";
    return ss.str();
}

std::string 
Uri::encode(const std::string& value)
{
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (char c : value) {
        if (std::isalnum(static_cast<unsigned char>(c)) ||
            c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        } else {
            escaped << '%' << std::setw(2) << std::uppercase
                    << int(static_cast<unsigned char>(c));
        }
    }

    return escaped.str();
}

} // namespace orion