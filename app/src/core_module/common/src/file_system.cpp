#ifdef ORION_COMMON_USE_GLIB
#include <glib-2.0/glib.h>
#endif

#include <src/file_system.h>
#include <filesystem>

namespace orion {

Error
ensure_parent_directories(const std::filesystem::path& file_path)
{
    std::filesystem::path parent_dir = file_path.parent_path();
    std::error_code ec;
    bool exists = std::filesystem::exists(parent_dir, ec);
    if (ec) {
        RAISE(ErrorCode::io, "error checking existence of parent directory: %s", ec.message().c_str());
    }
    if (!parent_dir.empty() && !exists) {
        std::filesystem::create_directories(parent_dir, ec);
        if (ec) {
            RAISE(ErrorCode::io, "error creating directories: %s", ec.message().c_str());
        }
    }
    SUCCEED();
}



#ifndef ORION_COMMON_USE_GLIB

#ifdef _WIN32
static std::string
app_directory_path(const char* app_id, const std::vector<std::string>& nodes = {}, bool create = true)
{
    std::filesystem::path appdir = std::filesystem::path(std::getenv("LOCALAPPDATA")) / app_id;
    
    if(create) std::filesystem::create_directories(appdir);
    for (auto node : nodes) {
        if(create) std::filesystem::create_directories(appdir);
        appdir /= node;
    }
    return (const char*)appdir.u8string().c_str();
}

std::string
data_directory_path(const char* app_id, const std::vector<std::string>& nodes)
{
    std::filesystem::path datadir = std::filesystem::path(app_directory_path(app_id)) / "data";
    std::filesystem::create_directories(datadir);
    for (auto node : nodes) {
        std::filesystem::create_directories(datadir);
        datadir /= node;
    }
    return (const char*)datadir.u8string().c_str();
}

std::string
get_data_directory_path(const char* app_id, const std::vector<std::string>& nodes)
{
    std::filesystem::path datadir = std::filesystem::path(app_directory_path(app_id, {}, false)) / "data";
    for (auto node : nodes) {
        datadir /= node;
    }
    return (const char*)datadir.u8string().c_str();
}

std::string
download_directory_path(const char* app_id, const std::vector<std::string>& nodes)
{
    std::filesystem::path datadir = std::filesystem::path(app_directory_path(app_id)) / "download";
    std::filesystem::create_directories(datadir);
    for (auto node : nodes) {
        std::filesystem::create_directories(datadir);
        datadir /= node;
    }
    return (const char*)datadir.u8string().c_str();
}

std::string
get_download_directory_path(const char* app_id, const std::vector<std::string>& nodes)
{
    std::filesystem::path datadir = std::filesystem::path(app_directory_path(app_id, {}, false)) / "download";
    for (auto node : nodes) {
        datadir /= node;
    }
    return (const char*)datadir.u8string().c_str();
}

std::string
cache_directory_path(const char* app_id, const std::vector<std::string>& nodes)
{
    std::filesystem::path datadir = std::filesystem::path(app_directory_path(app_id)) / "data";
    std::filesystem::create_directories(datadir);
    for (auto node : nodes) {
        std::filesystem::create_directories(datadir);
        datadir /= node;
    }
    return (const char*)datadir.u8string().c_str();
}

std::string
get_cache_directory_path(const char* app_id, const std::vector<std::string>& nodes)
{
    std::filesystem::path datadir = std::filesystem::path(app_directory_path(app_id, {}, false)) / "data";
    for (auto node : nodes) {
        datadir /= node;
    }
    return (const char*)datadir.u8string().c_str();
}

std::string
extension_fetch_directory_path(const char* app_id, const std::vector<std::string>& nodes)
{
    std::filesystem::path datadir = std::filesystem::path(app_directory_path(app_id)) / "webextensions_fetch";
    std::filesystem::create_directories(datadir);
    for (auto node : nodes) {
        std::filesystem::create_directories(datadir);
        datadir /= node;
    }
    return (const char*)datadir.u8string().c_str();
}

std::string
get_extension_fetch_directory_path(const char* app_id, const std::vector<std::string>& nodes)
{
    std::filesystem::path datadir = std::filesystem::path(app_directory_path(app_id, {}, false)) / "webextensions_fetch";
    for (auto node : nodes) {
        datadir /= node;
    }
    return (const char*)datadir.u8string().c_str();
}
#endif //WIN32


#else // ORION_COMMON_USE_GLIB



std::string
get_data_directory_path(const char* app_id, const std::vector<std::string>& nodes)
{
    std::filesystem::path datadir = std::filesystem::path(g_get_user_data_dir()) / app_id;
    for (auto node : nodes) {
        datadir /= node;
    }
    return (const char*)datadir.u8string().c_str();
}

std::string
get_download_directory_path(const char* app_id, const std::vector<std::string>& nodes)
{
    std::filesystem::path datadir = std::filesystem::path(get_cache_directory_path(app_id)) / "download";
    for (auto node : nodes) {
        datadir /= node;
    }
    return (const char*)datadir.u8string().c_str();
}

std::string
get_cache_directory_path(const char* app_id, const std::vector<std::string>& nodes)
{
    std::filesystem::path datadir = std::filesystem::path(g_get_user_cache_dir()) / app_id;
    for (auto node : nodes) {
        datadir /= node;
    }
    return (const char*)datadir.u8string().c_str();
}

std::string
get_extension_fetch_directory_path(const char* app_id, const std::vector<std::string>& nodes)
{
    std::filesystem::path datadir = get_data_directory_path(app_id, { "webextensions_fetch" });
    for (auto node : nodes) {
        datadir /= node;
    }
    return (const char*)datadir.u8string().c_str();
}



#endif // ORION_COMMON_USE_GLIB

} // namespace orion
