#pragma once
#include <vector>
#include <string>
#include <array>

#include <common/src/uri.h>

namespace orion {

class DomainFilter {
public: 
    DomainFilter(const std::vector<std::string>& domains);
    DomainFilter();

    bool insert(std::string domain);
    bool remove(std::string domain);

    bool has_contents() const;

    bool is_match(std::string domain) const;
    bool is_match_or_subdomain(std::string domain) const;
    bool is_match_or_subdomain_from_reversed(const std::string& reversed_domain) const;

    std::vector<char*> get_webkit_allow_list(const DomainFilter& disabled_filter) const;

private:
    std::vector<std::string> m_reversed_domains;
};

constexpr inline std::array<bool, 256> build_valid_char_map() {
    std::array<bool, 256> map{};
    //RFC 3986 unreserved characters
    for(int c = 'a'; c <= 'z'; ++c) map[c] = true;
    for(int c = 'A'; c <= 'Z'; ++c) map[c] = true;
    for(int c = '0'; c <= '9'; ++c) map[c] = true;
    map['-'] = map['.'] = map['_'] = map['~'] = true;
    
    //RFC 3986 sub-delimiters and other valid query characters
    //NOTE: '=' and '&' are excluded, this is for validating param key/value components
    map['!'] = map['$'] = map['\''] = map['('] = map[')'] = true;
    map['*'] = map['+'] = map[','] = map[';'] = true;
    map[':'] = map['@'] = map['/'] = map['?'] = true;
    
    return map;
}

constexpr inline std::array<bool, 256> build_hex_map() {
    std::array<bool, 256> map{};
    for(int c = '0'; c <= '9'; ++c) map[c] = true;
    for(int c = 'a'; c <= 'f'; ++c) map[c] = true;
    for(int c = 'A'; c <= 'F'; ++c) map[c] = true;
    return map;
}

constexpr inline auto VALID_CHARS = build_valid_char_map();
constexpr inline auto HEX_CHARS = build_hex_map();

static_assert(VALID_CHARS.size() == (1u << ( 8 * sizeof(unsigned char))),
    "VALID_CHARS isn't perfectly indexable by unsigned char");
static_assert(HEX_CHARS.size() == (1u << ( 8 * sizeof(unsigned char))),
    "HEX_CHARS isn't perfectly indexable by unsigned char");

//doesn't properly validate the url, only validates params
//constexpr to enable a static assert for a basic behaviour test
std::string strip_tracking_data(std::string_view url);

// Returns the registrable domain (for example, "example.co.uk") for a URI.
// Hosts without a public suffix, such as localhost and IP addresses, are
// returned normalized instead. An empty string means the URI has no host.
std::string registrable_domain_from_uri(std::string_view uri);

}
