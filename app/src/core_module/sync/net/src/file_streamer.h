#pragma once
#include "net_http_client.h"

namespace orion::net {

class FileUploaderImpl;

using ProgressCb = std::function<bool(size_t done, size_t toatl)>; //if total unknown then HTTP_CONTENT_LENGTH_UNDEFINED

class FileUploader {
public:

public:
    FileUploader(const std::string&& filepath, const std::string&& content_type);

    void progress_cb(ProgressCb progress_cb);
    orion::Error start(std::shared_ptr<HttpClient>& http_client, HttpClient::RequestDto request, HttpClient::ResultCb cb);

    bool is_done() const;
    void reset();

    orion::Error error() const;

    virtual ~FileUploader();
    
protected:
    std::unique_ptr<FileUploaderImpl> m_impl;
};

class FileDownloaderImpl;

class FileDownloader {

public:
    using DoneCb = std::function<void(const FileDownloader& fd, HttpClient::ResponseDto& result)>; //if total unknown then HTTP_CONTENT_LENGTH_UNDEFINED

public:
    FileDownloader() = delete;
    FileDownloader(const std::string&& filepath, const std::string&& content_type);

    void progress_cb(ProgressCb progress_cb);
    orion::Error start(std::shared_ptr<HttpClient>& http_client, const HttpClient::RequestDto request, DoneCb done_cb);

    bool is_done() const;
    void reset();

    orion::Error error() const;

    ~FileDownloader() = default;    
protected:
    std::unique_ptr<FileDownloaderImpl> m_impl;
};

} //end orion::net