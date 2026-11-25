#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <core/dto/filter_list_dto.h>

namespace orion::filter_lists {

struct Metadata {
    std::string name{};
    std::string display_name{};
    std::string resource_path{}; // if provided, raw_data will be ignored
    std::string raw_data{}; // ignored if resource_path or json_data is provided
    std::string url{};
    std::string version{}; // fallback when the list data does not declare a version
    bool is_data_embedded{};
    // JSON representation of the filter list in the format that WebKit expects. If this is non-empty,
    // it will be used instead of raw_data or resource_path internally.
    std::string json_data{};
    bool is_website_specific{};
    std::string description{};
    bool is_enabled{};
    int64_t position{};
    FilterListType filter_type{};
};

const std::vector<Metadata>& get_metadata_list();

} // namespace orion::filter_lists
