#define ENABLE_VERBOSE_LOGS 0

#include "uri_util.h"

#include <gtk/gtk.h>
#include <libsoup/soup.h>

#include <algorithm>
#include <optional>

#include <common/src/log.h>
#include <common/src/defer.h>

namespace orion {

std::string
registrable_domain_from_uri(std::string_view uri)
{
    const std::string uri_string(uri);
    GError* parse_error = nullptr;
    GUri* parsed_uri = g_uri_parse(uri_string.c_str(), G_URI_FLAGS_NONE, &parse_error);
    if (!parsed_uri) {
        if (parse_error)
            g_error_free(parse_error);
        return {};
    }

    const char* host = g_uri_get_host(parsed_uri);
    if (!host || !*host) {
        g_uri_unref(parsed_uri);
        return {};
    }

    char* ascii_host = g_hostname_to_ascii(host);
    std::string normalized_host = ascii_host ? ascii_host : host;
    g_free(ascii_host);
    g_uri_unref(parsed_uri);

    std::ranges::transform(normalized_host, normalized_host.begin(), [](unsigned char ch) {
        return static_cast<char>(g_ascii_tolower(ch));
    });

    GError* tld_error = nullptr;
    const char* base_domain = soup_tld_get_base_domain(normalized_host.c_str(), &tld_error);
    std::string result = base_domain ? base_domain : normalized_host;
    if (tld_error)
        g_error_free(tld_error);
    return result;
}



#pragma region DomainFilter

DomainFilter::DomainFilter(const std::vector<std::string>& domains) {
    m_reversed_domains = domains;
    for(auto& domain : m_reversed_domains)
    {
        std::reverse(domain.begin(), domain.end());
    }

    std::sort(m_reversed_domains.begin(), m_reversed_domains.end());
}

DomainFilter::DomainFilter() {
    //no operation needed
}


bool DomainFilter::insert(std::string domain) {
    std::reverse(domain.begin(), domain.end());

    auto it = std::lower_bound(m_reversed_domains.begin(), m_reversed_domains.end(), domain);
    if(it == m_reversed_domains.end() || *it != domain)
    {
        m_reversed_domains.insert(it, domain);
        return true;
    }
    return false;
}

bool DomainFilter::remove(std::string domain) {
    std::reverse(domain.begin(), domain.end());

    auto it = std::lower_bound(m_reversed_domains.begin(), m_reversed_domains.end(), domain);

    if(it != m_reversed_domains.end() && *it == domain)
    {
        m_reversed_domains.erase(it);
        return true;
    }
    return false;
}


bool DomainFilter::has_contents() const {
    return m_reversed_domains.size();
}


bool DomainFilter::is_match(std::string domain) const {
    std::reverse(domain.begin(), domain.end());
    return std::binary_search(m_reversed_domains.cbegin(), m_reversed_domains.cend(), domain);
}

bool DomainFilter::is_match_or_subdomain(std::string domain) const {
    std::reverse(domain.begin(), domain.end());
    return is_match_or_subdomain_from_reversed(domain);
}

bool DomainFilter::is_match_or_subdomain_from_reversed(const std::string& reversed_domain) const {
    auto it = std::upper_bound(m_reversed_domains.cbegin(), m_reversed_domains.cend(), reversed_domain);

    if(it == m_reversed_domains.cbegin()) return false; //no element smaller or equal to us alphabetically

    auto prev_it = std::prev(it);

    const std::string& potential_match = *prev_it;

    if(reversed_domain.starts_with(potential_match))
    {
        if (reversed_domain.length() == potential_match.length() || 
            reversed_domain[potential_match.length()] == '.') { //this operation is safe - starts with, different length so domain is alphabeticcaly > potential_match, specifically longer
            return true;
        }
    }
    return false;
}

std::vector<char*> DomainFilter::get_webkit_allow_list(const DomainFilter& disabled_filter) const {
    std::vector<char*> actual_filter;
    actual_filter.reserve((this->m_reversed_domains.size()) * 2 + 1);
    orion::log_verbose("creating webkit allow list");
    for(const std::string& reversed_domain : m_reversed_domains)
    {
        if(!disabled_filter.is_match_or_subdomain_from_reversed(reversed_domain))
        {
            std::string domain(reversed_domain.rbegin(), reversed_domain.rend());
            orion::log_verbose("website: %s", domain.c_str());
            actual_filter.push_back(g_strdup_printf("*://%s/*", domain.c_str()));
            actual_filter.push_back(g_strdup_printf("*://*.%s/*", domain.c_str()));
        }
    }
    orion::log_verbose("done");
    actual_filter.push_back(NULL);
    return actual_filter;
}

#pragma endregion DomainFilter

//LICENSE:
//filters sourced from https://firefox.settings.services.mozilla.com/v1/buckets/main/collections/query-stripping/records 15.09.2026
//this is MPLv2 licensed on a file level, but the data itself should be uncopyrightable


constexpr static std::optional<std::string_view>
extract_param_key_and_validate(std::string_view param)
{
    size_t eq_pos = std::string_view::npos; //will be set to the position of the '=' delimiter

    for (size_t i = 0; i < param.size(); ++i) {
        //safety cast to prevent signed indexing
        auto c = static_cast<unsigned char>(param[i]);

        if(VALID_CHARS[c])
        {
            continue;
        }

        if(c == '=')
        {
            if(eq_pos == std::string_view::npos) {
                eq_pos = i;
                continue;
            }
        } 
        else if(c == '%')
        {
            //check there are 2 hex chars after
            if (i + 2 < param.size() && 
                HEX_CHARS[static_cast<unsigned char>(param[i + 1])] && 
                HEX_CHARS[static_cast<unsigned char>(param[i + 2])])
            {
                i += 2;
                continue;
            }
        }

        //invalid char, bad '%' sequence or second '='
        return std::nullopt;
    }

    
    //substr inherently clamps, so passing npos is safe
    return param.substr(0, eq_pos);
}


constexpr bool
domain_should_bypass(std::string_view domain)
{
    const std::vector<std::string_view> filter = {
        "googleadservices.com",
    };
    
    for(std::string_view filter_str : filter)
    {
        if(domain.ends_with(filter_str))
        {
            if (domain.size() == filter_str.size() ||
                domain.at(domain.size() - filter_str.size() - 1) == '.')
            {
                return true;
            }
        }
    }

    return false;
}

constexpr bool
should_strip_parameter(std::string_view key)
{
    const std::vector<std::string> filter = {
        "gclid",
        "dclid",
        "msclkid",
        "_openstat",
        "yclid",
        "wickedid",
        "twclid",
        "_hsenc",
        "__hssc",
        "__hstc",
        "__hsfp",
        "hsctatracking",
        "wbraid",
        "gbraid",
        "ysclid",
        "mc_eid",
        "oly_anon_id",
        "__s",
        "vero_id",
        "_hsenc",
        "mkt_tok",
        "fbclid",

        "utm_source",//custom addition
    };

    bool result = filter.cend() != std::find(filter.cbegin(), filter.cend(), key);

    //orion::log_verbose("checking key %s, result: %s", std::string(key).c_str(), result ? "strip" : "leave");

    return result;
}

constexpr std::string
strip_tracking_data_internal(std::string_view uri_str)
{
    size_t colon_pos = uri_str.find(':');

    
    //authority only exists if scheme is immediately followed by "//"
    if (colon_pos != std::string_view::npos &&
        (colon_pos + 2 < uri_str.size() && uri_str[colon_pos + 1] == '/' && uri_str[colon_pos + 2] == '/'))
    {
        size_t authority_start = colon_pos + 3;
        
        
        size_t authority_end = uri_str.find_first_of("/?#", authority_start);

        std::string_view authority = uri_str.substr(authority_start, authority_end - authority_start);

        size_t at_symbol = authority.find('@');
        if (at_symbol != std::string_view::npos) {
            authority = authority.substr(at_symbol + 1); //userinfo stripping
        }

        size_t port_colon = authority.find(':');
        if (port_colon != std::string_view::npos) {
            authority = authority.substr(0, port_colon); //port stripping
        }


        if(domain_should_bypass(uri_str.substr(authority_start, authority_end - authority_start)))
        {
            return std::string(uri_str);
        }
    }



    

    size_t query_start = uri_str.find('?');
    if(query_start == uri_str.npos)
    {
        return std::string(uri_str);
    }

    size_t frag_start = uri_str.find('#', query_start + 1); //find is safe even if starting from index == string_view::size()

    size_t query_begin = query_start + 1;
    
    std::string result;
    result.reserve(uri_str.size());
    result.append(uri_str.substr(0, query_begin)); // everything up to and including '?

    //will inherently also safely handle frag_start == string_view::npos through clamping
    std::string_view query = uri_str.substr(query_begin, frag_start - query_begin); 


    if(query.size())
    {
        //orion::log_verbose("trip_tracking_data(): analysing query %s", std::string(query).c_str());

        bool first = true;
        size_t start = 0;
        size_t end = 0;
        do
        {
            end = query.find('&', start);
            std::string_view param = std::string_view(query).substr(start, end-start); //substr clamps in the standard library

            std::optional<std::string_view> key = extract_param_key_and_validate(param);

            if(key.has_value() && !should_strip_parameter(key.value()))
            {
                if(!first) result.push_back('&');
                result.append(param);
                first = false;
            }

            //overflow from npos to 0 is safe as it will be caught at the end condition
            //if end is at the end of our string, start will be == query.size()
            //both string::find() and string_view::substr() guarantee that the are safe to call with this value

            start = end + 1;
        }
        while(end != std::string_view::npos);
    }


    if(frag_start != uri_str.npos)
    {
        result.append(uri_str.substr(frag_start));
    }
    return result;
}

std::string strip_tracking_data(std::string_view url) { return strip_tracking_data_internal(url); }

//rough behaviour test
static_assert(
    strip_tracking_data_internal("https://www.google.com/search?client=firefox-b-d&msclkid=dsa&q=dsad&gclid=dnsjkdn&_testgclid=a#xdd") ==
    "https://www.google.com/search?client=firefox-b-d&q=dsad&_testgclid=a#xdd",
        "uri_util.cpp::strip_tracking_data() probably failed to properly strip known ids from an example uri"
);
static_assert(
    strip_tracking_data_internal("https://www.googleadservices.com?msclkid=dsa#xdd") ==
    "https://www.googleadservices.com?msclkid=dsa#xdd",
        "uri_util.cpp::strip_tracking_data() probably failed to ignore filtered domain"
);
static_assert(
    strip_tracking_data_internal("https://www.badgoogleadservices.com?msclkid=dsa#xdd") ==
    "https://www.badgoogleadservices.com?#xdd",
        "uri_util.cpp::strip_tracking_data() probably filtered out non subdomain"
);


}
