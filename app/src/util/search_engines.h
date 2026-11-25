#pragma once

#include <string>
#include <core/settings.h>
#include <common/src/uri.h>

namespace orion {

struct SearchEngineInfo {
    // Use %s in the URI templates to indicate where the search query should be inserted.
    std::string search_uri_template;

    // Use %s in the URI templates to indicate where the search query should be inserted.
    std::string suggestion_uri_template;
};

// Returns ErrorCode::not_found if the search engine info is unknown
Error get_search_engine_info(settings::SearchEngine engine, SearchEngineInfo* out_info);

// Expects %s in search_uri_template where the search query should be inserted
Uri get_search_uri(const std::string& search_uri_template, const std::string& query);

} // namespace orion