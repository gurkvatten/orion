#define ENABLE_VERBOSE_LOGS 0

#include "app_downloader.h"

#include <common/src/error.h>
#include <common/src/file_system.h>
#include <common/src/log.h>
#include <common/src/uuid.h>
#include <common/src/defer.h>
#include <common/src/uri.h>

#include <filesystem>
#include <optional>

#include <libsoup/soup.h>

namespace orion {

namespace {

struct DownloaderImpl {
    SoupMessage* message = nullptr;
    SoupSession* session = nullptr;
    GCancellable* cancellable = nullptr;
    GInputStream* input_stream = nullptr;

    gulong got_body_data_listener_id = 0;
    gulong got_headers_listener_id = 0;
};

//NOTE: we're using libsoup directly here as using webkitgtk's download does not allow for download pausing, range header

class SoupDownloader final : public Downloader, public std::enable_shared_from_this<SoupDownloader> {
public:
    explicit SoupDownloader(const char* app_id)
        :m_app_id(app_id)
        , m_id(generate_uuid_v4())
    {
        orion::log_verbose("Downloader[id:%s]::create", m_id.c_str());
        m_impl = std::make_unique<DownloaderImpl>();
        m_impl->session = soup_session_new();
    }

    ~SoupDownloader() override
    {
        orion::log_verbose(__PRETTY_FUNCTION__);
        cancel();
        if(m_impl->message)
        {
            g_signal_handler_disconnect(m_impl->message, m_impl->got_body_data_listener_id);
            g_signal_handler_disconnect(m_impl->message, m_impl->got_headers_listener_id);
            g_object_unref(m_impl->message);
        }
        g_object_unref(m_impl->session);
    }

    void
    set_uri(const std::string& uri) override
    {
        orion::log_verbose(__PRETTY_FUNCTION__);
        m_uri = uri;
    }

    void
    set_target_path(const std::string& path) override
    {
        orion::log_verbose(__PRETTY_FUNCTION__);
        m_target_path = path;
        if(m_is_waiting_for_path)
        {
            downloader_open_file(this);
            m_is_waiting_for_path = false;
        }
    }
    
    void
    set_http_method(const std::string& method) override
    {
        orion::log_verbose(__PRETTY_FUNCTION__);
        m_method = method;
    }

    void
    add_header(const std::string& name, const std::string& value) override
    {
        orion::log_verbose(__PRETTY_FUNCTION__);
        m_headers.push_back({name, value});
    }

    void add_cookie(const std::string& domain, const std::string& path, const std::string& name, const std::string& value, int max_age, bool secure, bool http_only) override
    {
        orion::log_verbose(__PRETTY_FUNCTION__);
        SoupCookieJar* cookie_jar = nullptr;
        if(!soup_session_has_feature(m_impl->session, SOUP_TYPE_COOKIE_JAR))
        {
            cookie_jar = soup_cookie_jar_new();
            soup_session_add_feature(m_impl->session, SOUP_SESSION_FEATURE(cookie_jar));
            g_object_unref(cookie_jar);
        }
        else
        {
            cookie_jar = SOUP_COOKIE_JAR(soup_session_get_feature(m_impl->session, SOUP_TYPE_COOKIE_JAR));
        }
        SoupCookie* cookie = soup_cookie_new(name.c_str(), value.c_str(), domain.c_str(), path.c_str(), max_age);
        soup_cookie_set_secure(cookie, secure);
        soup_cookie_set_http_only(cookie, http_only);
        soup_cookie_jar_add_cookie(cookie_jar, cookie);
    }

    void
    set_request_body(const std::string& content_type, std::vector<char>&& data) override
    {
        orion::log_verbose(__PRETTY_FUNCTION__);
        m_request_content_type = content_type;
        m_request_body = std::move(data);
    }
    
    void
    set_range(int64_t start, int64_t end) override
    {
        orion::log_verbose(__PRETTY_FUNCTION__);
        m_start = start;
        m_end = end;
    }

    void
    set_finished_cb(std::function<void(Error)> cb) override
    {
        orion::log_verbose(__PRETTY_FUNCTION__);
        m_finished_cb = cb;
    }

    void
    set_progress_cb(std::function<void(const Progress&)> cb) override
    {
        orion::log_verbose(__PRETTY_FUNCTION__);
        m_progress_cb = cb;
    }

    void
    set_recommended_filename_cb(std::function<void(const std::string&)> cb) override
    {
        orion::log_verbose(__PRETTY_FUNCTION__);
        m_recommended_filename_cb = cb;
    }

    void set_content_type_cb(std::function<void(const std::string&)> cb) override {
        orion::log_verbose(__PRETTY_FUNCTION__);
        m_content_type_cb = cb;
    }

    void
    set_overwrite_existing(bool overwrite) override
    {
        orion::log_verbose(__PRETTY_FUNCTION__);
        m_overwrite_existing = overwrite;
    }

    void
    start_download() override
    {
        orion::log_verbose(__PRETTY_FUNCTION__);
        if(m_is_started)
        {
            orion::log_debug("SoupDownloader::start_download(): called but download is already in progress");
            return;
        }
        m_is_started = true;

        m_impl->cancellable = g_cancellable_new();
        if(!m_impl->message)
        {
            m_impl->message = soup_message_new(m_method.size()?m_method.c_str():SOUP_METHOD_GET, m_uri.c_str());
            if(!m_impl->message)
            {
                orion::log_debug("SoupDownload::start_download(): likely called without a set uri");
                return;
            }
            m_impl->got_body_data_listener_id = g_signal_connect(m_impl->message, "got-body-data", G_CALLBACK(on_soup_message_got_body_data), this);
            m_impl->got_headers_listener_id = g_signal_connect(m_impl->message, "got-headers", G_CALLBACK(on_soup_message_got_headers), this);
        }
        else
        {
            GError* err = NULL;
            GUri* glib_uri = g_uri_parse(m_uri.c_str(), GUriFlags(SOUP_HTTP_URI_FLAGS), &err);
            if(err)
            {
                defer([err]{ g_error_free(err); });
                orion::log_error("failed to set download uri, %s", err->message);
                return;
            }
            defer([glib_uri]{ g_uri_unref(glib_uri); });
            soup_message_set_uri(m_impl->message, glib_uri);
            soup_message_set_method(m_impl->message, m_method.size()?m_method.c_str():SOUP_METHOD_GET);
        }
        SoupMessageHeaders* headers = soup_message_get_request_headers(m_impl->message);
        soup_message_headers_set_range(headers, m_start, m_end);
        for(auto& header : m_headers)
        {
            soup_message_headers_append(headers, header.first.c_str(), header.second.c_str());
        }
        if(m_request_body.size() || m_request_content_type.length())
        {
            GBytes* body_bytes = g_bytes_new_static(m_request_body.data(), m_request_body.size());
            defer([body_bytes]{ g_bytes_unref(body_bytes); });
            soup_message_set_request_body_from_bytes(m_impl->message, m_request_content_type.c_str(), body_bytes);
        }

        auto* this_weak = new std::weak_ptr<SoupDownloader>(this->weak_from_this());
        soup_session_send_async(m_impl->session, m_impl->message, G_PRIORITY_DEFAULT, m_impl->cancellable, on_soup_session_send_finished, this_weak);
    }

    void
    cancel() override
    {
        orion::log_verbose(__PRETTY_FUNCTION__);
        if(m_impl->cancellable)
        {
            g_cancellable_cancel(m_impl->cancellable);
        }
        if(m_impl->input_stream)
        {
            g_input_stream_close_async(m_impl->input_stream, G_PRIORITY_DEFAULT, NULL, NULL, NULL);
            g_clear_object(&m_impl->input_stream);
        }
        if(m_is_waiting_for_path)
        {
            finish_with_error(orion::Error(orion::ErrorCode::cancelled, "cancelled download", __FILE__, __LINE__));
        }
        this->m_is_waiting_for_path = false;
    }

    void
    clean_tmp_files() override
    {
        orion::log_verbose(__PRETTY_FUNCTION__);
        //I dislike this a lot honestly, I do not think this should exist
    }

    std::string
    id() const override
    {
        orion::log_verbose(__PRETTY_FUNCTION__);
        return m_id;
    }

    std::string
    target_path() const override
    {
        orion::log_verbose(__PRETTY_FUNCTION__);
        return m_target_path.value_or("");
    }

private:

    static void
    on_output_stream_splice_finished(GObject* source, GAsyncResult* result, gpointer user_data)
    {
        orion::log_verbose(__PRETTY_FUNCTION__);
        std::unique_ptr<std::weak_ptr<SoupDownloader>> downloader_weak(static_cast<std::weak_ptr<SoupDownloader>*>(user_data));
        auto downloader_strong = downloader_weak->lock();

        GError_autoptr err = NULL;
        g_output_stream_splice_finish(G_OUTPUT_STREAM(source), result, &err);

        if(downloader_strong)
        {
            g_clear_object(&downloader_strong->m_impl->cancellable);

            if(err)
            {
                //TODO: error decoding
                if(err->code == G_IO_ERROR_CANCELLED)
                {
                    downloader_strong->finish_with_error(orion::Error(orion::ErrorCode::cancelled, "cancelled download", __FILE__, __LINE__));
                }
                else
                {
                    downloader_strong->finish_with_error(orion::Error(orion::ErrorCode::io, err->message, __FILE__, __LINE__));
                }
            }
            else
            {
                downloader_strong->finish_with_error(orion::Error());
            }
        }
    }

    static void
    downloader_open_file(SoupDownloader* downloader)
    {
        auto& m_impl = downloader->m_impl;
        GFile* file = g_file_new_for_path(downloader->m_target_path.value().c_str());
        defer([file]{ g_object_unref(file); });
        m_impl->cancellable = g_cancellable_new();
        auto* downloader_weak = new std::weak_ptr<SoupDownloader>(downloader->weak_from_this());
        if(downloader->m_overwrite_existing)
        {
            //TODO: perhaps use this thing's etag to only replace our temporary files?
            g_file_replace_async(file, NULL, false, G_FILE_CREATE_REPLACE_DESTINATION, G_PRIORITY_DEFAULT,
            m_impl->cancellable, on_file_replace_finished, downloader_weak);
        }
        else
        {
            g_file_create_async(file, G_FILE_CREATE_NONE, G_PRIORITY_DEFAULT,
                m_impl->cancellable, on_file_create_finished, downloader_weak);
        }
    }

    static void
    on_soup_session_send_finished(GObject* source, GAsyncResult* result, gpointer user_data)
    {
        orion::log_verbose(__PRETTY_FUNCTION__);
        std::unique_ptr<std::weak_ptr<SoupDownloader>> downloader_weak(static_cast<std::weak_ptr<SoupDownloader>*>(user_data));
        auto downloader_strong = downloader_weak->lock();

        GError_autoptr err = NULL;
        GInputStream_autoptr input_stream = soup_session_send_finish(SOUP_SESSION(source), result, &err);

        if(downloader_strong)
        {
            auto& m_impl = downloader_strong->m_impl;
            m_impl->input_stream = g_steal_pointer(&input_stream);
            g_clear_object(&m_impl->cancellable);
            if(err)
            {
                //TODO: error decoding
                if(err->code == G_IO_ERROR_CANCELLED)
                {
                    downloader_strong->finish_with_error(orion::Error(orion::ErrorCode::cancelled, "cancelled download", __FILE__, __LINE__));
                }
                else
                {
                    downloader_strong->finish_with_error(orion::Error(orion::ErrorCode::io, err->message, __FILE__, __LINE__));
                }
                orion::log_debug("%s: error occurred during download", __func__);
                return;
            }


            if(downloader_strong->m_target_path.has_value())
            {
                downloader_open_file(downloader_strong.get());
            }
            else
            {
                downloader_strong->m_is_waiting_for_path = true;
            }
        }
    }

    static void on_file_replace_finished(GObject* source, GAsyncResult* result, gpointer user_data)
    {
        orion::log_verbose(__PRETTY_FUNCTION__);
        std::unique_ptr<std::weak_ptr<SoupDownloader>> downloader_weak(static_cast<std::weak_ptr<SoupDownloader>*>(user_data));
        auto downloader_strong = downloader_weak->lock();
        
        GError_autoptr err = NULL;
        GOutputStream_autoptr output_stream = G_OUTPUT_STREAM(g_file_replace_finish(G_FILE(source), result, &err));

        if(downloader_strong)
        {
            auto& m_impl = downloader_strong->m_impl;
            g_clear_object(&m_impl->cancellable);
            defer([istream = &m_impl->input_stream]{ g_clear_object(istream); });

            if(err)
            {
                //TODO: error decoding
                if(err->code == G_IO_ERROR_CANCELLED)
                {
                    downloader_strong->finish_with_error(orion::Error(orion::ErrorCode::cancelled, "cancelled download", __FILE__, __LINE__));
                }
                else
                {
                    downloader_strong->finish_with_error(orion::Error(orion::ErrorCode::open, err->message, __FILE__, __LINE__));
                }
                g_input_stream_close_async(m_impl->input_stream, G_PRIORITY_DEFAULT, NULL, NULL, NULL);
                return;
            }

            
            m_impl->cancellable = g_cancellable_new();
            //the output stream should be unreffed because of it being an autopointer, so this async task is the only thing holding a ref
            //the close flags mean that all file descriptors should be guaranteed to close, checking glib source code seems to confirm
            //do not let a chatbot convince you otherwise
            g_output_stream_splice_async(output_stream, m_impl->input_stream,
                GOutputStreamSpliceFlags(G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE | G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET),
                G_PRIORITY_DEFAULT, m_impl->cancellable, on_output_stream_splice_finished, downloader_weak.release());
        }
    }

    static void
    on_file_create_finished(GObject* source, GAsyncResult* result, gpointer user_data)
    {
        orion::log_verbose(__PRETTY_FUNCTION__);
        std::unique_ptr<std::weak_ptr<SoupDownloader>> downloader_weak(static_cast<std::weak_ptr<SoupDownloader>*>(user_data));
        auto downloader_strong = downloader_weak->lock();
        
        GError_autoptr err = NULL;
        GOutputStream_autoptr output_stream = G_OUTPUT_STREAM(g_file_create_finish(G_FILE(source), result, &err));

        if(downloader_strong)
        {
            auto& m_impl = downloader_strong->m_impl;
            g_clear_object(&m_impl->cancellable);
            defer([istream = &m_impl->input_stream]{ g_clear_object(istream); });

            if(err)
            {
                //TODO: error decoding
                if(err->code == G_IO_ERROR_CANCELLED)
                {
                    downloader_strong->finish_with_error(orion::Error(orion::ErrorCode::cancelled, "cancelled download", __FILE__, __LINE__));
                }
                else
                {
                    downloader_strong->finish_with_error(orion::Error(orion::ErrorCode::open, err->message, __FILE__, __LINE__));
                }
                g_input_stream_close_async(m_impl->input_stream, G_PRIORITY_DEFAULT, NULL, NULL, NULL);
                return;
            }

            
            m_impl->cancellable = g_cancellable_new();
            g_output_stream_splice_async(output_stream, m_impl->input_stream,
                GOutputStreamSpliceFlags(G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE | G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET),
                G_PRIORITY_DEFAULT, m_impl->cancellable, on_output_stream_splice_finished, downloader_weak.release());
        }
    }



    static void
    on_soup_message_got_headers(SoupMessage* message, SoupDownloader* downloader)
    {
        orion::log_verbose(__PRETTY_FUNCTION__);
        auto& m_impl = downloader->m_impl;
        auto& m_progress_cb = downloader->m_progress_cb;
        auto& m_recommended_filename_cb = downloader->m_recommended_filename_cb;
        auto& m_content_type_cb = downloader->m_content_type_cb;
        SoupMessageHeaders* headers = soup_message_get_response_headers(message);
        downloader->m_content_length = soup_message_headers_get_content_length(headers);
        if(m_progress_cb)
        {
            Progress progress{};
            progress.chunk_received = 0;
            progress.total_received = downloader->m_total_received;
            progress.content_length = downloader->m_content_length;
            progress.estimated_progress = ((double)(downloader->m_total_received))/(downloader->m_content_length);
            m_progress_cb(std::move(progress));
        }
        if(m_recommended_filename_cb)
        {
            std::string filename = "unknown";
            char* disposition = NULL;
            GHashTable* params = NULL;
            if(soup_message_headers_get_content_disposition(headers, &disposition, &params))
            {
                defer([disposition]{ g_free(disposition); });
                defer([params]{ g_hash_table_unref(params); });
                const auto* filename_c = static_cast<const char*>(g_hash_table_lookup(params, "filename"));
                const auto* filename_utf8_c = static_cast<const char*>(g_hash_table_lookup(params, "filename*"));
                if(filename_utf8_c)
                {
                    filename = filename_utf8_c;
                }
                else if(filename_c)
                {
                    filename = filename_c;
                }
            }
            else
            {
                const char* raw_path = g_uri_get_path(soup_message_get_uri(m_impl->message));
                char* unescaped_string = g_uri_unescape_string(raw_path, nullptr);
                if(unescaped_string)
                {
                    std::filesystem::path path = unescaped_string;
                    if(path.has_filename())
                    {
                        filename = path.filename().string();
                    }
                    g_free(unescaped_string);
                }
            }
            m_recommended_filename_cb(filename);
        }
        if(m_content_type_cb)
        {
            const char* content_type = soup_message_headers_get_content_type(headers, NULL);
            m_content_type_cb(content_type?content_type:"");
        }
    }

    static void
    on_soup_message_got_body_data(SoupMessage* message, uint chunk_size, SoupDownloader* downloader)
    {
        orion::log_verbose(__PRETTY_FUNCTION__);
        auto& m_progress_cb = downloader->m_progress_cb;
        downloader->m_total_received += chunk_size;
        if(m_progress_cb)
        {
            Progress progress{};
            progress.chunk_received = chunk_size;
            progress.total_received = downloader->m_total_received;
            progress.content_length = downloader->m_content_length;
            progress.estimated_progress = ((double)(downloader->m_total_received))/(downloader->m_content_length);
            m_progress_cb(std::move(progress));
        }
    }
        
    static void
    on_input_stream_close_finished(GObject* source, GAsyncResult* result, gpointer data)
    {
        auto* self_weak = static_cast<std::weak_ptr<SoupDownloader>*>(data);
        defer([self_weak]{ delete self_weak; });
        auto self_strong = self_weak->lock();
        if(self_strong)
        {
            g_clear_object(&self_strong->m_impl->input_stream);
        }
        
        GError* err = NULL;
        g_input_stream_close_finish(G_INPUT_STREAM(source), result, &err);
        if(err)
        {
            defer([err]{ g_error_free(err); });
            orion::log_error("%s", err->message);
        }
    }

    void
    finish_with_error(Error err)
    {
        orion::log_verbose(__PRETTY_FUNCTION__);
        if(m_finished_cb)
        {
            m_finished_cb(err);
        }
        m_is_started = false;
        m_is_waiting_for_path = false;
        m_content_length = 0;
        m_total_received = 0;
    }

    DownloaderImpl*
    impl() const
    {
        orion::log_verbose(__PRETTY_FUNCTION__);
        return m_impl.get();
    }

private:
    std::function<void(Error)> m_finished_cb;
    std::function<void(const Progress&)> m_progress_cb;
    std::function<void(const std::string&)> m_recommended_filename_cb;
    std::function<void(const std::string&)> m_content_type_cb;
    std::string m_uri;
    std::optional<std::string> m_target_path;
    bool m_overwrite_existing = false;
    bool m_is_started = false;
    bool m_is_closed = false;
    bool m_is_waiting_for_path = false;
    int64_t m_start = 0;
    int64_t m_end = -1;
    size_t m_content_length = 0;
    size_t m_total_received = 0;
    std::string m_app_id;
    std::string m_id;
    std::string m_method;
    std::string m_request_content_type;
    std::vector<std::pair<std::string, std::string>> m_headers;
    std::vector<char> m_request_body;
    std::unique_ptr<DownloaderImpl> m_impl;
};

class SoupDownloaderFactory final : public DownloaderFactory {
public:
    std::shared_ptr<Downloader>
    create(const char* app_id) override
    {
        return std::make_shared<SoupDownloader>(app_id);
    }
private:
};

} // namespace

DownloaderFactory*
app_downloader_factory()
{
    static SoupDownloaderFactory factory;
    return &factory;
}

} // namespace orion
