#include <regex>
#include "html_injector.h"
#include "../window/main_window/main_window.h"

namespace orion {

HtmlInjector&
HtmlInjector::set(const std::string& key, const std::string& value)
{
    values[key] = value;
    return *this;
}

std::string
HtmlInjector::process(const std::string& content)
{
    //std::regex placeholder(R"(\{\{(\w+)\}\})"); //Matches just alphanum
    std::regex placeholder(R"(\{\{([^\}]+)\}\})"); // More potent regex - Can match css styles with - etc.

    std::smatch match;

    std::string result;
    std::string::const_iterator search_start(content.cbegin());

    while (std::regex_search(search_start, content.cend(), match, placeholder)) {
        result.append(search_start, match[0].first);

        std::string key = match[1];
        auto it = values.find(key);
        if (it != values.end()) {
            result += it->second; //Match found, replacing
        } else {
            result += match[0]; //Match NOT found, pass through unchanged
        }

        search_start = match[0].second;
    }
    result.append(search_start, content.cend());

    output += result;

    return output;
}

} // namespace orion