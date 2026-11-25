#pragma once

#include <stop_token>
#include <common/src/dto/dto_member.h>
#include <core/dto/profile_dto.h>
#include <common/src/strong_types.h>
#include <common/src/filter_list.h>

namespace orion {

DECLARE_STRONG_TYPEDEF(FilterListId, int64_t);

enum class FilterListType : int64_t {
    not_set         = 0,
    builtin         = 1, // builtin in the app in build time
    user            = 2, // created by the user
    custom          = 3, // custom filter list with in-place rules filled by the user
    block_web_fonts = 4, // filter list specifically for blocking web fonts
};

// URL cannot be updated
struct CreateFilterListQuery {
    DTO_MEMBER(profile_id, ProfileId);
    DTO_MEMBER(name, std::string);
    DTO_MEMBER(version, std::string);
    DTO_MEMBER(position, int64_t);
    DTO_MEMBER(url, std::string);
    DTO_MEMBER(display_name, std::string);
    DTO_MEMBER(description, std::string);
    DTO_MEMBER(is_enabled, bool);
    DTO_MEMBER(updated_at, UnixTimeUtc64);
    DTO_MEMBER(active_rules_count, int64_t);
    DTO_MEMBER(filter_type, FilterListType);
    DTO_MEMBER(custom_filter_data, std::string);
    DTO_MEMBER(content_hash, std::string);

    DTO_MEMBER(update_is_enabled, bool); // if false, is_enabled field of existing filter list will not be updated
    DTO_MEMBER(update_updated_at, bool); // if false, updated_at field of existing filter list will not be updated
    DTO_MEMBER(update_active_rules_count, bool); // if false, active_rules_count field of existing filter list will not be updated
    DTO_MEMBER(update_version, bool); // if false, version field of existing filter list will not be updated
    DTO_MEMBER(update_content_hash, bool); // if false, content_hash field of existing filter list will not be updated
};

struct CreateFilterListReply {
    DTO_MEMBER(id, FilterListId);
    DTO_MEMBER(err, Error);
};

struct FilterListDto {
    DTO_MEMBER(id, FilterListId);
    DTO_MEMBER(profile_id, ProfileId);
    DTO_MEMBER(name, std::string);
    DTO_MEMBER(version, std::string);
    DTO_MEMBER(position, int64_t);
    DTO_MEMBER(url, std::string);
    DTO_MEMBER(description, std::string);
    DTO_MEMBER(display_name, std::string);
    DTO_MEMBER(is_enabled, bool);
    DTO_MEMBER(updated_at, UnixTimeUtc64);
    DTO_MEMBER(active_rules_count, int64_t);
    DTO_MEMBER(filter_type, FilterListType);
    DTO_MEMBER(custom_filter_data, std::string);
    DTO_MEMBER(content_hash, std::string);
};

struct GetFilterListsQuery {
    DTO_MEMBER(profile_id, ProfileId);
    DTO_MEMBER(ignore_block_web_fonts, bool);
};

struct GetFilterListsReply {
    DTO_MEMBER(filter_lists, std::vector<FilterListDto>);
    DTO_MEMBER(err, Error);
};

struct LoadFilterListQuery {
    DTO_MEMBER(filter_list, std::shared_ptr<FilterList>);
    DTO_MEMBER(raw_data, std::string);
    DTO_MEMBER(stop_token, std::stop_token);
};

struct LoadFilterListReply {
    DTO_MEMBER(err, Error);
    DTO_MEMBER(items, std::vector<FilterListItem>);
    DTO_MEMBER(parsed_rule_count, int32_t);
    DTO_MEMBER(version, std::string);
};

struct FilterListsChange {
    DTO_MEMBER(created, std::vector<FilterListDto>);
    DTO_MEMBER(updated, std::vector<FilterListDto>);
    DTO_MEMBER(deleted, std::vector<FilterListId>);
};

} // namespace orion
