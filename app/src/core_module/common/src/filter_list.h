#pragma once

#include <string>
#include <regex>
#include <map>
#include <unordered_map>
#include <variant>
#include <stop_token>
#include <src/error.h>

namespace orion {

using FilterVariant = std::variant<
    std::string,
    bool,
    std::vector<std::string>
>;

// Each FilterListItem instance can be serialized to JSON using rfl::json::write into for example:
//   {"trigger":[{"url-filter":"some value"}],"action":[{"type":"block"}]}
// We have to support values of different types, so we use std::variant that is handled
// by rfl::json::write correctly.
using FilterListItem = std::map<std::string, std::map<std::string, FilterVariant>>;

// Just a forward declaration
struct FilterList;

FilterList* filter_list_new();
// The category enables list-specific compatibility rules when known (for example, EasyList).
FilterList* filter_list_new(const char* list_category);
void filter_list_free(FilterList* self);

// Slow
Error filter_list_parse(
    FilterList* self, const char* raw_data, std::stop_token stop_token, std::vector<FilterListItem>* out_items,
    int32_t* out_parsed_rule_count, std::string* out_version
);

// Fast. Returns empty string if no version info is found.
Error filter_list_parse_version(FilterList* self, const char* raw_data, std::string* out_version);
// Fast. Returns empty string if no title info is found.
Error filter_list_parse_title(FilterList* self, const char* raw_data, std::string* out_title);
// Fast. Returns empty string if no description info is found.
Error filter_list_parse_description(FilterList* self, const char* raw_data, std::string* out_description);

} // namespace orion
