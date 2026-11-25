#pragma once

#include <common/src/error.h>
#include <functional>
#include <memory>
#include <string>

namespace orion {

class Downloader {
public:
    struct Progress {
        size_t chunk_received;     // size of the last chunk received
        size_t total_received;     // total bytes received so far
        size_t content_length;     // 0 if unknown
        double estimated_progress; // 0.0 to 1.0 (0.0 if unknown)
    };

    virtual ~Downloader() = default;

    virtual void set_uri(const std::string& uri) = 0;
    //NOTE: to keep this as a platform agnostic interface, the implementation itself needs to handle file creation
    //orion::ErrorCode::open is the reserved error code for when this happens, to allow retries
    virtual void set_target_path(const std::string& path) = 0; 
    virtual void set_http_method(const std::string& method) = 0;
    virtual void set_request_body(const std::string& content_type, std::vector<char>&& data) = 0;
    virtual void set_range(int64_t start, int64_t end) = 0;
    virtual void add_header(const std::string& name, const std::string& value) = 0;
    virtual void add_cookie(const std::string& domain, const std::string& path, const std::string& name, const std::string& value, int max_age, bool secure, bool http_only) = 0;
    virtual void set_finished_cb(std::function<void(Error)> cb) = 0;
    virtual void set_progress_cb(std::function<void(const Progress&)> cb) = 0;
    virtual void set_recommended_filename_cb(std::function<void(const std::string&)> cb) = 0;
    virtual void set_content_type_cb(std::function<void(const std::string&)> cb) = 0;
    virtual void set_overwrite_existing(bool overwrite) = 0;
    virtual void start_download() = 0; // download process can be started without a set target path, in that case, the download will only obtain headers and wait for a target path
    virtual void cancel() = 0;
    virtual void clean_tmp_files() = 0;

    virtual std::string id() const = 0; // unique identifier of the downloader instance
    virtual std::string target_path() const = 0;
};

class DownloaderFactory {
public:
    virtual ~DownloaderFactory() = default;
    virtual std::shared_ptr<Downloader> create(const char* app_id) = 0;
};

} // namespace orion
