#include "url_or_search_detector.h"

#include <arpa/inet.h>

namespace orion {

static bool
has_space(const std::string& s)
{
    return s.find(' ') != std::string::npos;
}

static bool
is_ip_address(const std::string& s)
{
    static const std::regex ip_regex(
        R"(^(\d{1,3}\.){3}\d{1,3}(:\d+)?$)"
    );
    return std::regex_match(s, ip_regex);
}

static std::string
strip_ipv6_brackets(const std::string& s)
{
    if (s.size() >= 2 && s.front() == '[' && s.back() == ']')
        return s.substr(1, s.size() - 2);
    return s;
}

static bool
looks_like_ipv6(const std::string& input)
{
    std::string s = strip_ipv6_brackets(input);

    // IPv6 must contain ':'
    if (s.find(':') == std::string::npos)
        return false;

    sockaddr_in6 sa{};
    return inet_pton(AF_INET6, s.c_str(), &(sa.sin6_addr)) == 1;
}

// The scheme itself does not have to be known
static bool
has_scheme(const std::string& s)
{
    return s.find("://") != std::string::npos;
}

static bool
looks_like_hostname(const std::string& s)
{
    static const std::regex host_regex(
        R"(^([a-zA-Z0-9-]+\.)+[a-zA-Z]{2,}(:\d+)?([/?#].*)?$)"
    );
    return std::regex_match(s, host_regex);
}

static bool
is_localhost(const std::string& s)
{
    return s.rfind("localhost", 0) == 0;
}

bool
is_probably_url(const std::string& input)
{
    if (has_space(input)) {
        return false;
    }

    if (has_scheme(input)) {
        return true;
    }

    if (is_ip_address(input)) {
        return true;
    }

    if (looks_like_ipv6(input)) {
        return true;
    }

    if (is_localhost(input)) {
        return true;
    }

    if (looks_like_hostname(input)) {
        return true;
    }
    return false;
}

} // namespace orion