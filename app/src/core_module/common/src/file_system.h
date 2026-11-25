#pragma once

#include <filesystem>
#include <src/error.h>

namespace orion {

Error ensure_parent_directories(const std::filesystem::path& file_path);

#ifdef _WIN32
//these functions inherently create parent directories for the obtained path,
//somewhat awkward to propagate error codes out of (currently, they just abort) and difficult to read
//recommend utilising the get_ prefixed ones below, with an explicit call to ensure_parent_directories
std::string data_directory_path(const char* app_id, const std::vector<std::string>& nodes = {});
std::string download_directory_path(const char* app_id, const std::vector<std::string>& nodes = {});
std::string cache_directory_path(const char* app_id, const std::vector<std::string>& nodes = {});
//TODO: implement reference counting and automatic removal for these extension files
std::string extension_fetch_directory_path(const char* app_id, const std::vector<std::string>& nodes = {});
#endif
std::string get_data_directory_path(const char* app_id, const std::vector<std::string>& nodes = {});
std::string get_download_directory_path(const char* app_id, const std::vector<std::string>& nodes = {});
std::string get_cache_directory_path(const char* app_id, const std::vector<std::string>& nodes = {});
//TODO: implement reference counting and automatic removal for these extension files
std::string get_extension_fetch_directory_path(const char* app_id, const std::vector<std::string>& nodes = {});

} // namespace orion