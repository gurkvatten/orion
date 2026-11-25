
#include "file_streamer.h"
#include <../common/src/log.h>
#include <../common/src/string_util.h>
#include <sys/stat.h>
#include <fstream>
#include <filesystem>
#include <string.h> //strerror


namespace orion::net {

class FileStreamerImpl {
public:
    FileStreamerImpl() = delete;
    FileStreamerImpl(const std::string&& filepath, const std::string&& content_type) : 
        m_filepath(std::forward<const std::string>(filepath))
        , m_content_type(std::forward<const std::string>(content_type))
    {}
    void
    progress_cb(ProgressCb progress_cb)
    {
        m_progress_cb = std::move(progress_cb);
    }
    bool
    is_done() const
    {
        return m_is_done;
    }
    void
    reset()
    {
        if (m_file.is_open()) {
            m_file.close();
        }
        m_total = m_done = 0;

        m_is_done = false;
        m_error_code = 0;
        m_error_message.clear();
    }
    orion::Error
    error() const
    {
        return m_error_code ? MAKE_ERROR(ErrorCode::io, m_error_message.c_str()) : Error::ok();
    }
    ~FileStreamerImpl()
    {
        done();
    }

protected:
    void
    log_error(int error_code, std::string message)
    {
        orion::log_error(message.c_str());
        error(error_code, message);
    }
    inline void
    log_not_open(int err)
    {
        log_error(-1, orion::str::format(FileStreamerImpl::closed_err_msg_format, m_filepath.c_str()));
    }
    inline void
    log_cannot_open(int err)
    {
        log_error(err?err:-1, orion::str::format(FileStreamerImpl::open_err_msg_format, m_filepath.c_str(), strerror(err)));
    }
    inline void
    log_cannot_create(int err)
    {
        log_error(err?err:-1, orion::str::format(FileStreamerImpl::create_err_msg_format, m_filepath.c_str(), strerror(err)));
    }
    inline void
    log_couldnt_read_write(int err)
    {
        log_error(err?err:-1, orion::str::format(FileStreamerImpl::read_err_msg_format, m_filepath.c_str(), strerror(err)));
    }
    void progress(size_t last_chunk)
    {
        m_done += last_chunk;
        if (m_progress_cb) {
            m_progress_cb(m_done, m_total);
        }
    }
    void
    done()
    {
        if (m_file.is_open()) {
            m_file.pubsync();
            m_file.close();
        }
        m_is_done = true;
    }
    bool
    is_error() const 
    { 
        return m_error_code;
    }
    void
    error(int error_code, std::string error_message)
    {
        m_error_message = std::move(error_message);
        m_error_code = error_code;
    }

private:
    static constexpr const char* read_err_msg_format = "Could not read from %s, error: %s";
    static constexpr const char* closed_err_msg_format = "file %s to transfer is closed";
    static constexpr const char* open_err_msg_format = "file %s cannot be opened for transfer, error: %s";
    static constexpr const char* create_err_msg_format = "file %s cannot be created for transfer, error: %s";

protected:
    std::filebuf m_file;
    std::string m_filepath;
    std::string m_content_type;
    size_t m_total;

private:
    size_t m_done;

    int m_error_code = 0;
    std::string m_error_message;
    bool m_is_done = false;

    ProgressCb m_progress_cb;
};

#define CONTENT_BUFFER_SIZE 1024
class FileUploaderImpl : public FileStreamerImpl {
public:
    FileUploaderImpl() = delete;
    FileUploaderImpl(const std::string&& filepath, const std::string&& content_type) : 
        FileStreamerImpl(std::forward<const std::string>(filepath), std::forward<const std::string>(content_type))
    {}
    HttpClient::RequestDto::Body
    body()
    {
        //TODO get file size?
        HttpClient::RequestDto::Body body;
        errno = 0;
        if (m_file.open(m_filepath, std::ios::in)) {
            struct stat file_stat;
            if (::stat(m_filepath.c_str(), &file_stat) == 0) {
                m_total = body.total_content_length = file_stat.st_size;
            }
            body.content.reserve(CONTENT_BUFFER_SIZE);

            errno = 0;
            std::streamsize read = m_file.sgetn(body.content.data(), CONTENT_BUFFER_SIZE);
            if (read > 0) {
                //set the final length.
                body.content.resize(read);
            } else if (errno) {
                log_couldnt_read_write(errno);
                body.content.clear();
            }
            body.content_type = m_content_type;
        } else {
            log_cannot_open(errno);
        }
        return body;
    }    
    HttpClient::DataCb
    data_cb()
    {
        return [this](const HttpClient::ResponseDto& response, char* buffer, size_t max_size) -> size_t {
            orion::log_debug("data_cb called with max_size: %zu", max_size);
            if (is_done()) {
                orion::log_debug("We've already done sendingg");
                return 0;
            } else if (is_error()) {
                done();
                return 0;
            }
            std::streamsize read = 0;
            if (m_file.is_open()) {//the file needs to be opened at this stage
                //use errno to check reading errors?
                errno = 0; //is this gonna work even though no doc suggests that it should? There's no error indication  whatsoever in that convenient std::filebuf class. Uffff
                read = m_file.sgetn(buffer, max_size);
                if (errno) {
                    log_couldnt_read_write(errno);
                } else {
                    orion::log_debug("Read %zd bytes from %s", read, m_filepath.c_str());
                    if (read == 0) {
                        done();
                    } else {
                        progress(read);
                    }
                }
            } else {
                log_not_open(errno);
            }
            return read;
        };
    }
    ~FileUploaderImpl() = default;
};

FileUploader::FileUploader(const std::string&& filepath, const std::string&& content_type) :
    m_impl(std::make_unique<FileUploaderImpl>(std::forward<const std::string>(filepath)
                                            , std::forward<const std::string>(content_type)))
{
}

//this needs to be here due to the std::unique_ptr<FileStreamerImpl> m_impl; otherwise we would need to introduce allocator
FileUploader::~FileUploader()
{
}

void
FileUploader::progress_cb(ProgressCb progress_cb)
{
    m_impl->progress_cb(std::move(progress_cb));
}


orion::Error
FileUploader::start(std::shared_ptr<HttpClient>& http_client, HttpClient::RequestDto request, HttpClient::ResultCb cb)
{
    orion::Error err = m_impl->error();
    if (!err) {
        HttpClient::RequestDto::Body body = m_impl->body();
        //body.total_content_length = HTTP_CONTENT_LENGTH_UNDEFINED;
        if (!body.content.empty()) {
            request.body = std::move(body);
            http_client->execute(std::move(request), std::move(cb), m_impl->data_cb());
        }
        return m_impl->error();
    }
    return err;
}

bool
FileUploader::is_done() const
{
    return m_impl->is_done();
}

void
FileUploader::reset()
{
    m_impl->reset();
}

orion::Error
FileUploader::error() const
{
    return m_impl->error();
}

class FileDownloaderImpl : public FileStreamerImpl {
public:
    FileDownloaderImpl() = delete;
    FileDownloaderImpl(const std::string&& filepath, const std::string&& content_type) : 
        FileStreamerImpl(std::forward<const std::string>(filepath), std::forward<const std::string>(content_type))
    {}
    void
    done_cb(FileDownloader::DoneCb done_cb)
    {
        m_done_cb = std::move(done_cb);
    }
    HttpClient::ResultCb
    result_cb()
    {
        return [this](HttpClient::ResponseDto response) {
            if (response.error) {
                orion::log_error("Request failed with error: %s", response.error.message().c_str());
                return;
            }
            //TODO prepare for redirect and potebtial rewind, for the moment we expect to get 200
            switch (response.state) {
            case HttpClient::ResponseState::headers:
                m_total = response.body.total_content_length;
                break;
            case HttpClient::ResponseState::data:
                write_chunk(response.body.content);
                break;
            case HttpClient::ResponseState::done:
                if (response.status_code != 200) {
                    done();
                    std::filesystem::remove(m_filepath);
                    log_error(response.status_code, str::format("Request failed with status code: %d", response.status_code));
                } else {
                    write_chunk(response.body.content);
                    done();
                }
                break;        
            default:
                orion::log_warning("Got unexpected result state: %zu", response.state);
            }
        };
    }
    ~FileDownloaderImpl() {
        done();
        if (error()) {
            std::filesystem::remove(m_filepath);
        }
    }

private:
    void
    write_chunk(const std::string& content)
    {
        if (content.empty() || error()) {
        } else if (!m_file.is_open() && m_file.open(m_filepath, std::ios_base::out) == NULL) {
            log_cannot_create(errno);
        } else {
            std::streamsize n = m_file.sputn(content.c_str(), content.size());
            if (n != static_cast<std::streamsize>(content.size())) {
                log_couldnt_read_write(errno);
            } else {
                progress(content.size());
            }
        }
    }
    
private:
    FileDownloader::DoneCb m_done_cb;
};

FileDownloader::FileDownloader(const std::string&& filepath, const std::string&& content_type) :
    m_impl(std::make_unique<FileDownloaderImpl>(std::forward<const std::string>(filepath)
                                            , std::forward<const std::string>(content_type)))
{}

orion::Error
FileDownloader::start(std::shared_ptr<HttpClient>& http_client, HttpClient::RequestDto request, FileDownloader::DoneCb done_cb)
{
    m_impl->done_cb(std::move(done_cb));
    orion::Error err = m_impl->error();
    if (!err) {
        http_client->execute(std::move(request), m_impl->result_cb());
        return m_impl->error();
    }
    return err;
}

} //end orion::net