#pragma once

#include <common/src/dto/dto_member.h>
#include <core/dto/profile_dto.h>

namespace orion {

DECLARE_STRONG_TYPEDEF(SearchEngineId, int64_t);

struct SearchEngineDto {
    DTO_MEMBER(id, SearchEngineId, SearchEngineId(0));
    DTO_MEMBER(profile_id, ProfileId, ProfileId(0));
    DTO_MEMBER(name, std::string);
    DTO_MEMBER(search_url_template, std::string);
    DTO_MEMBER(suggestions_url_template, std::string);
    DTO_MEMBER(keyword, std::string);
};

struct CreateSearchEngineQuery {
    DTO_MEMBER(profile_id, ProfileId, ProfileId(0));
    DTO_MEMBER(name, std::string);
    DTO_MEMBER(search_url_template, std::string);
    DTO_MEMBER(suggestions_url_template, std::string);
    DTO_MEMBER(keyword, std::string);
};

struct CreateSearchEngineReply {
    DTO_MEMBER(id, SearchEngineId);
    DTO_MEMBER(err, Error);
};

struct UpdateSearchEngineQuery {
    DTO_MEMBER(id, SearchEngineId, SearchEngineId(0));

    DTO_MEMBER(name, std::string);
    DTO_MEMBER(search_url_template, std::string);
    DTO_MEMBER(suggestions_url_template, std::string);
    DTO_MEMBER(keyword, std::string);

    DTO_MEMBER(update_name, bool, false);
    DTO_MEMBER(update_search_url_template, bool, false);
    DTO_MEMBER(update_suggestions_url_template, bool, false);
    DTO_MEMBER(update_keyword, bool, false);
};

struct GetSearchEngineIdsQuery {
    DTO_MEMBER(profile_id, ProfileId, ProfileId(0));
};

struct GetSearchEngineIdsReply {
    DTO_MEMBER(ids, std::vector<SearchEngineId>);
    DTO_MEMBER(err, Error);
};

struct GetSearchEnginesReply {
    DTO_MEMBER(search_engines, std::vector<SearchEngineDto>);
    DTO_MEMBER(err, Error);
};

struct SearchEnginesChange {
    DTO_MEMBER(created, std::vector<SearchEngineDto>);
    DTO_MEMBER(updated, std::vector<SearchEngineDto>);
    DTO_MEMBER(deleted, std::vector<SearchEngineId>);
};

struct SearchEngineMatch {
    DTO_MEMBER(search_url_template, std::string);
    DTO_MEMBER(search_query, std::string);
};

struct MatchSearchEnginesQuery {
    DTO_MEMBER(query_string, std::string);
    DTO_MEMBER(profile_id, ProfileId, ProfileId(0));
};

struct MatchSearchEnginesReply {
    DTO_MEMBER(err, Error);
    DTO_MEMBER(matches, std::vector<SearchEngineMatch>);
};

} // namespace orion