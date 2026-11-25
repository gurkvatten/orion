#include "search_engines.h"

#include <common/src/defer.h>
#include <gtk/gtk.h>

namespace orion {

static SearchEngineInfo
get_search_engine_info_internal(settings::SearchEngine engine)
{
    switch (engine) {
        case settings::SearchEngine::kagi:
            return {
                "https://kagi.com/search?q=%s",
                "https://kagi.com/suggest?q=%s",
            };
        case settings::SearchEngine::google:
            return {
                "https://www.google.com/search?q=%s",
                "https://suggestqueries.google.com/complete/search?client=firefox&q=%s",
            };
        case settings::SearchEngine::bing:
            return {
                "https://www.bing.com/search?q=%s",
                "https://api.bing.com/osjson.aspx?query=%s",
            };
        case settings::SearchEngine::yandex:
            return {
                "https://yandex.com/search/?text=%s",
                "https://suggest.yandex.com/suggest-ya.cgi?part=%s",
            };
        case settings::SearchEngine::duckduckgo:
            return {
                "https://duckduckgo.com/?q=%s",
                "https://duckduckgo.com/ac/?q=%s&type=list",
            };
        case settings::SearchEngine::startpage:
            return {
                "https://www.startpage.com/sp/search?q=%s",
                "https://ac.startpage.com/ac/sp_suggest.php?query=%s",
            };
        case settings::SearchEngine::brave:
            return {
                "https://search.brave.com/search?q=%s",
                "https://search.brave.com/api/suggest?q=%s",
            };
        case settings::SearchEngine::ecosia:
            return {
                "https://www.ecosia.org/search?q=%s",
                "https://ac.ecosia.org/autocomplete?q=%s",
            };
        case settings::SearchEngine::yahoo:
            return {
                "https://search.yahoo.com/search?p=%s",
                "https://search.yahoo.com/sugg/gossip/gossip-us-ura/?output=fxjson&command=%s",
            };
        case settings::SearchEngine::baidu:
            return {
                "https://www.baidu.com/s?wd=%s",
                "http://suggestion.baidu.com/su?wd=%s&json=1",
            };
        case settings::SearchEngine::qwant:
            return {
                "https://www.qwant.com/?q=%s",
                "https://api.qwant.com/api/suggest/v1/web?count=10&q=%s",
            };
        case settings::SearchEngine::mojeek:
            return {
                "https://www.mojeek.com/search?q=%s",
                "https://www.mojeek.com/suggest?q=%s",
            };
        case settings::SearchEngine::you_com:
            return {
                "https://you.com/search?q=%s",
                "https://you.com/api/suggest?q=%s",
            };
        default:
            return {};
    }
}

Error
get_search_engine_info(settings::SearchEngine engine, SearchEngineInfo* out_info)
{
    if (!out_info) {
        RAISE(ErrorCode::argument, "out_info must not be null");
    }
    *out_info = get_search_engine_info_internal(engine);
    if (out_info->search_uri_template.empty() && out_info->suggestion_uri_template.empty()) {
        RAISE(ErrorCode::not_found, "unknown search engine: %d", static_cast<int>(engine));
    }
    SUCCEED();
}

// Replaces the first occurrence of "%s" in text by value, returns the resulting string
static std::string
replace_first_percent_s(std::string text, const std::string& value)
{
    const std::string token = "%s";
    size_t pos = text.find(token);
    if (pos != std::string::npos) {
        text.replace(pos, token.length(), value);
    }
    return text;
}

Uri
get_search_uri(const std::string& search_uri_template, const std::string& query)
{
    char* escaped = g_uri_escape_string(query.c_str(), NULL, TRUE);
    defer([escaped]{ g_free(escaped); });
    std::string search_uri = replace_first_percent_s(search_uri_template, std::string(escaped));
    orion::Uri uri = orion::Uri::parse_user_input(search_uri.c_str());
    return uri;
}

} // namespace orion