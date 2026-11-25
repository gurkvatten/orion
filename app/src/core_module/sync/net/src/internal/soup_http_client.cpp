#include "http_client_intf.h"
#if NET_LIB == USING_SOUP
//#include <../common/src/string_util.h>
//#include <../common/src/uuid.h>
//#include <../common/src/crypto.h>
#include <../common/src/defer.h>

#include <libsoup/soup.h>
#include <fstream>
#include <regex>
#include <condition_variable>
#include <stddef.h>

/*available signals
id=67 name=wrote-headers n_params=0 ret=void
id=68 name=wrote-body-data n_params=1 ret=void
id=69 name=wrote-body n_params=0 ret=void
id=70 name=got-informational n_params=0 ret=void
id=71 name=got-headers n_params=0 ret=void
id=72 name=got-body-data n_params=1 ret=void
id=73 name=got-body n_params=0 ret=void
id=74 name=content-sniffed n_params=2 ret=void
id=75 name=starting n_params=0 ret=void
id=76 name=restarted n_params=0 ret=void
id=77 name=finished n_params=0 ret=void
id=78 name=authenticate n_params=2 ret=gboolean
id=79 name=network-event n_params=2 ret=void
id=80 name=accept-certificate n_params=2 ret=gboolean
id=81 name=request-certificate n_params=1 ret=gboolean
id=82 name=request-certificate-password n_params=1 ret=gboolean
id=83 name=hsts-enforced n_params=0 ret=void

To list signals of GObject do:
    SoupMessage* msg = soup_message_new(request.method.c_str(), request.raw_url.c_str()); // we should ensure this is freed
    defer([msg]{ g_object_unref(msg); });
    GType my_type = G_OBJECT_TYPE(msg);///g_object_get_type(msg);
    guint n_signals;
    guint* signal_ids = g_signal_list_ids(my_type, &n_signals);
    for (guint i = 0; i < n_signals; ++i) {
        GSignalQuery query;
        g_signal_query(signal_ids[i], &query);
        printf("id=%u name=%s n_params=%u ret=%s\n", signal_ids[i], query.signal_name, query.n_params, g_type_name(query.return_type));
    }
*/
//https://dpino.pages.gitlab.gnome.org/libsoup/SoupMessage.html#SoupMessage-network-event
namespace orion::net {
#define ENABLE_VERBOSE_LOGS 0
#define USING_SOUP_ASYNC 1
#ifdef _DEBUG
constexpr int output_buffer_size = 256 + 1;
#else
constexpr int output_buffer_size = HTTP_MAX_BODY_LENGTH + 1; //if the body is long make each chunk exceed HTTP_MAX_BODY_LENGTH so that we trigger
                                                            //the logic to either dispatch or ignore it.
#endif
struct PendingRequest {
    HttpClient::ResultCb cb = nullptr;
    HttpClient::RequestDto request = {};
    HttpClient::DataCb data_cb = nullptr;
};

struct SoapRequestData;
// See also Kagi/Core/API Handlers/KagiService.swift in https://github.com/kagisearch/browser.git

class SoupHttpClient : public HttpClientIntf {
public: // interface implementations
    void get(const std::string& end_point, HttpClient::ResultCb cb = nullptr) override;
    void post(const std::string& end_point,  const HttpClient::RequestDto::Body&& body, HttpClient::ResultCb cb = nullptr, HttpClient::DataCb data_cb = nullptr) override;
    void remove(const std::string& end_point, HttpClient::ResultCb cb = nullptr) override;
    void patch(const std::string& end_point,  const HttpClient::RequestDto::Body&& body, HttpClient::ResultCb cb = nullptr, HttpClient::DataCb data_cb = nullptr) override;

    void execute(HttpClient::RequestDto request, HttpClient::ResultCb cb = nullptr, HttpClient::DataCb data_cb = nullptr) override;

    bool process() override;
    bool block() override;
    void cancel(HttpClient::CancelCb cb) override;

    // New: initialize session in its own thread
    void initialize_session_in_thread();

public:
    SoupHttpClient(const Uri&& base_url, std::string session_cookie_names = "")
        : HttpClientIntf(std::forward<const Uri>(base_url), std::move(session_cookie_names)) {
            m_session = nullptr;
            m_cancellable = nullptr;
            initialize_session_in_thread();
        }
    ~SoupHttpClient() {
        m_stop_worker = true;
        if (m_worker_thread.joinable()) {
            m_result_queue.clear_and_push({}); // push an empty request to unblock the worker thread if it's waiting
            m_worker_thread.join();
        }
        cleanup();
    }

    void dispatch_response_callback(HttpClient::ResultCb cb, const HttpClient::ResponseDto& response);
    void callback_finished(const char* name);
    static void dispatch_error_response(SoapRequestData* data, const GError* error);

public:
    GCancellable* m_cancellable = nullptr;
    std::mutex m_shutdown_mutex;
    std::condition_variable m_shutdown_mutex_cv;
    //the m_finished_request_data is here solely as it's not possible to track when to actually delete the structure when the req is cancelled.
    //the whne cancelled the finish signal can be emitted from inside of any method called in it middle of soup callback.
    //at the same moment there 's no way to pass a structure neither any map as there's no way to map it to anything as the callbacks are getting session and not
    //the actuall soup message. That's a dissaster.
    //so we just keep this map and empty it periodically upon issuing another request. 
    std::vector<SoapRequestData*> m_finished_request_data;

private: // usings
    void callback_started(const char* name);
    const std::string& session_cookie_names() { return m_session_cookie_names; }
    virtual void try_dispatch_cancel_cb() override;
    HttpClient::Cookies extract_cookies_from_soup_message(SoupMessage* msg);
    bool maybe_reset_cancellable();
    void clear_finished_request_data();
    // Post a request to the worker thread
    void post_request_to_worker(const PendingRequest& req) {
        m_request_queue.push(req);
    }
    void cleanup() {
        if (m_session) {
            g_object_unref(m_session);
            m_session = nullptr;
        }
        if (m_cancellable) {
            g_object_unref(m_cancellable);
            m_cancellable = nullptr;
        }
        clear_finished_request_data();
    }

private: // static helper functions
    static void do_http_request(SoupHttpClient* self, HttpClient::RequestDto& request, HttpClient::ResultCb cb, HttpClient::DataCb data_cb = nullptr);
    static void soup_session_send_complete_cb(GObject* object, GAsyncResult* result, gpointer user_data);

private: // data members
    volatile int32_t m_pending_callbacks = 0;
    CrossThreadQueue<PendingRequest> m_request_queue;
    std::thread m_worker_thread;
    std::atomic_bool m_stop_worker = false;

private: // structs
    SoupSession* m_session = nullptr;
};

// Create and initialize SoupSession in its own thread
void
SoupHttpClient::initialize_session_in_thread()
{
    m_worker_thread = std::thread([this]() {
        // Create session and cancellable in worker thread
        GMainContext* main_context = g_main_context_new();
        g_main_context_push_thread_default(main_context);

        m_session = soup_session_new();
        m_cancellable = g_cancellable_new();
        while (!m_stop_worker) {
            /*  Considerations:
            * here we cannot just wait indefinetely for a request since we could aqctually get here after the below itteration loop exited
            * while in fact there's still a stuff to do for the soup async machinery and hence by indefinite wait we would freeze the async libsoup processing.
            * it's been acknowledge that most of the time this will just run idle since there's really nothing to do.
            * So maybe we could actually remove the do_http_request from here and let it run in the user thread so that here we can just worry about the it loop.
            * ,However we would sill need to have some sort of wait here when g_main_context_pending is false unless we want g_main_context_iteration to block, which I'm not so sure about.

            * Overall chacking g_main_context_pending and waithing 200ms is extremely small load and given the act that this is inherantly a short lived thread, just for the span of the API usage,
            * then let's have it this way for now. 
            */
            m_request_queue.wait_for_and_pop(std::chrono::milliseconds(200), [this](PendingRequest penging_request) {
                //here we are quarantied to receive a valid pending req
                do_http_request(this, penging_request.request, penging_request.cb, penging_request.data_cb);            
                return penging_request;
            });
            if (m_stop_worker) {
                break;
            }

            int it_counter = 0;
            while (!m_stop_worker && g_main_context_pending(main_context)) {//this condition could also be based on m_pending_callbacks > 0.
                //orion::log_debug("Worker thread: processing main context events");
                g_main_context_iteration(main_context, FALSE);
                //if we cancel then this loop should exit.
                ++it_counter;
                if ((it_counter %= 10) == 0) {
                    //orion::log_debug("checking for incoming requests while itterating");
                    m_request_queue.pop([this](PendingRequest penging_request) {
                        //here we are quarantied to receive a valid pending req
                        do_http_request(this, penging_request.request, penging_request.cb, penging_request.data_cb);            
                        return penging_request;
                    }); //no wait here.
                } 
            }
        }
        cleanup();

        g_main_context_pop_thread_default(main_context);
        g_main_context_unref(main_context);
        orion::log_debug("Worker thread: exiting...");
    });
}

struct SoapRequestData {
    SoupHttpClient* client = nullptr;
    SoupMessage* msg = nullptr;
    HttpClient::ResultCb callback = nullptr;
    HttpClient::DataCb data_callback = nullptr;
    HttpClient::ResponseDto response = {};
    int wrote_body_data_signal_id = 0;
    int finished_signal_id = 0;
    GInputStream *in = nullptr; //this is an input for us to receive lib soup body data.
    GInputStream *out = nullptr; //this is an input for lib soup to receive body data from us.
    size_t out_buffered_body_data = 0;
    GBytes* body_bytes = nullptr;
    char output_buffer[output_buffer_size] = {};
    HttpClient::RequestDto request = {};

    void close_input_stream() {
        if (in) {
            g_object_unref(in);
            in = nullptr;
        }
    }

    void close_output_stream() {
        if (out) {
            g_object_unref(out);
            out = nullptr;
        }
    }

    ~SoapRequestData() {
        if (wrote_body_data_signal_id) {
            g_signal_handler_disconnect(msg, wrote_body_data_signal_id);
        }
        if (finished_signal_id) {
            g_signal_handler_disconnect(msg, finished_signal_id);
        }
        close_input_stream();
        close_output_stream();
        if (body_bytes) {
            g_bytes_unref(body_bytes);
        }
        if (msg) {
            g_object_unref(msg);
        }
    }
};

//this method should only ever have 1 implementation and that is the one using libsoup
//, if we using a different native net lib ten there needs to somewhere be a different implementation of 
//and this one has to be conditionally #ifdeffed out possibly with the rest of the file content.
std::shared_ptr<HttpClientIntf>
HttpClientIntf::create(const Uri&& base_url, std::string session_cookie_names)
{
    auto client = std::make_shared<SoupHttpClient>(std::forward<const Uri>(base_url), std::move(session_cookie_names));
    return client;
}

void
SoupHttpClient::callback_started(const char* name)
{
    m_pending_callbacks = m_pending_callbacks + 1;
    orion::log_debug("soup: callback_started: pending_callbacks=%d name=%s", m_pending_callbacks, name);
}

void
SoupHttpClient::callback_finished(const char* name)
{
    m_pending_callbacks = m_pending_callbacks - 1;
    orion::log_debug("soup: callback_finished: pending_callbacks=%d name=%s", m_pending_callbacks, name);
}

void
SoupHttpClient::dispatch_response_callback(HttpClient::ResultCb cb, const HttpClient::ResponseDto& response)
{
#ifdef USING_SOUP_ASYNC
    m_result_queue.push({DoneRequest{.cb = cb, .response = response}});
    orion::log_debug("soup: dispatch_response_callback: queued response callback, pending_callbacks=%d", m_pending_callbacks);
#else
    cb(std::move(response));
#endif
}

void
SoupHttpClient::dispatch_error_response(SoapRequestData* data, const GError* error)
{
    if (!error) {
        orion::log_debug("dispatch_error_response called with null error");
        return;
    }
    orion::log_error("error->message: %s, error->code: %d", error->message, error->code);
    if (data->callback) {
        data->client->m_result_queue.push({.cb = data->callback
        , .response = HttpClient::ResponseDto{.error = MAKE_ERROR(ErrorCode::net_io, error ? error->message : "unknown error")
                                                , .body = {}
                                                , .state = HttpClient::ResponseState::done}});
   }
}

static void
request_cleanup(SoapRequestData* rd, const char* name)
{
    rd->client->callback_finished(name);
    rd->client->m_finished_request_data.push_back(rd);
}

static void
on_soup_signal_finished(SoupSession *session, gpointer user_data)
{
    SoapRequestData* rd = reinterpret_cast<SoapRequestData*>(user_data);
    request_cleanup(rd, "on_soup_signal_finished"); //for the moment we assume that this signal happens every time, even in case of error, 
    //if that's not the case we need to move this call to a more appropriate place. (see where this has been commented out.)
}

HttpClient::Cookies
SoupHttpClient::extract_cookies_from_soup_message(SoupMessage* msg)
{
    if (m_session_cookie_names.empty()) {
        return {};
    }
    GSList* cookies = soup_cookies_from_response(msg);
    defer([cookies]{ soup_cookies_free(cookies); });
    
    HttpClient::Cookies cookie_dtos;
    for (GSList* l = cookies; l != NULL; l = l->next) {
        SoupCookie* cookie = reinterpret_cast<SoupCookie*>(l->data);

        HttpClient::CookieDto cookie_dto{
            .name = soup_cookie_get_name(cookie),
            .value = soup_cookie_get_value(cookie),
            .domain = soup_cookie_get_domain(cookie),
            .path = soup_cookie_get_path(cookie),
            .secure = soup_cookie_get_secure(cookie) != 0,
            .http_only = soup_cookie_get_http_only(cookie) != 0,
            .expires = g_date_time_to_unix(soup_cookie_get_expires(cookie)),
        };

        orion::log_debug(
            "cookie: %s=%s; domain=%s; path=%s; secure=%d; http_only=%d",
            cookie_dto.name.c_str(),
            cookie_dto.value.c_str(),
            cookie_dto.domain.c_str(),
            cookie_dto.path.c_str(),
            cookie_dto.secure,
            cookie_dto.http_only
        );
        if (m_session_cookie_names.find('&' + cookie_dto.name + '&') != std::string::npos) {
            cookie_dtos.emplace_back(std::move(cookie_dto));
        }
    }
    return cookie_dtos;
}

static Headers
soup_message_headers_to_vector(SoupMessage* msg, SoupMessageHeaders* response_headers)
{
    Headers headers;
    SoupMessageHeadersIter hdr_iter;
 
    soup_message_headers_iter_init(&hdr_iter, response_headers);
    const char* name, *value;       
    while (soup_message_headers_iter_next(&hdr_iter, &name, &value)) {
        headers.emplace_back(name, value);
    }
    return headers;
}

static void
on_soup_signal_wrote_body_data(SoupSession *session, gsize chunk_size, gpointer user_data)
{
    SoapRequestData* rd = reinterpret_cast<SoapRequestData*>(user_data);
    //G_MEMORY_INPUT_STREAM
    //sanity check
    if (rd->out_buffered_body_data < chunk_size) {
        throw std::logic_error("chunk_size is larger than buffered body data, this should not happen");
    }
    //feed the stream only if we've sent enought data already so that only a reasonable amount of body data if buffered.
    if ((rd->out_buffered_body_data -= chunk_size) < (output_buffer_size / 2)) {
        size_t max_size = output_buffer_size;
        char* buffer = new char[max_size];
        //TODO shouldn't we call this callback using similar thing as dispatch_response_callback? Or leave it as is?
        size_t supplied_size = rd->data_callback(rd->response, buffer, max_size);
        //assuming that the callback will not return more data than max_size, if it does we will just truncate it here.
        if (supplied_size) {
            g_memory_input_stream_add_data(G_MEMORY_INPUT_STREAM(rd->out), buffer, supplied_size, [](gpointer data) {
                delete[] static_cast<char*>(data);
            });
            rd->out_buffered_body_data += supplied_size;
        } else {
            delete[] buffer;
            //here we can either do the below to keep the stream busy and keep connection alive just in case the data source is currently waiting for more
            //g_memory_input_stream_add_data(G_MEMORY_INPUT_STREAM(rd->out), "", 0, nullptr);
            //or we can just regard it as the end of the stream.
            //TODO maybe to have some indication as to how to distinguish the above two options?
            rd->close_output_stream();
        }
    }
}

static void
write_callback(GObject *source_object, GAsyncResult *res, gpointer user_data)
{
    SoapRequestData* rd = reinterpret_cast<SoapRequestData*>(user_data);
    if (source_object != (GObject*)(rd->in)) {
        orion::log_error("write_callback called with an invalid input stream, expected %p but got %p", rd->in, source_object);
        throw std::logic_error("write_callback called with an invalid input stream");
    }
    GError *error = NULL;
    gsize bytes_read = g_input_stream_read_finish(rd->in, res, &error);

    if (error) {
        SoupHttpClient::dispatch_error_response(rd, error);
        g_error_free(error);
        rd->close_input_stream();
    } else if (bytes_read) {
        orion::log_debug("write_callback read %zu bytes", bytes_read);
        rd->response.body.content.append(rd->output_buffer, bytes_read);
        if (rd->response.body.content.size() > HTTP_MAX_BODY_LENGTH) {
            rd->response.state = HttpClient::ResponseState::data;
            //TODO we cannot really clear the body content here since the dispatch might be async so we relt on the client to clear it after processing.
            //This however, can lead to reace is we run in different thread that user processes the callback.
            //Hence we should introduce a 2 synchronized methods into ResponseDTO:
            //  - one for appending data
            //  - one for getting & clearing it
            rd->client->dispatch_response_callback(rd->callback, rd->response);
            rd->response.body.content.clear();
        }
        g_input_stream_read_async (rd->in, rd->output_buffer, output_buffer_size, G_PRIORITY_DEFAULT, rd->client->m_cancellable
                , write_callback, rd);
    } else {
        orion::log_debug("write_callback: no more data to read, last bytes read: %zu", rd->response.body.content.size());
        rd->response.state = HttpClient::ResponseState::done;
        rd->client->dispatch_response_callback(rd->callback, rd->response);
        rd->close_input_stream();//this has to be AFTER dispatching the callback since this causes finish signal to be emitted ,hence completing the send callbacks => pottentially
        //setting m_pending_callbacks to 0, causing process() / block() to exist without being able to fetch the last response from the queue.
    }
}

void
SoupHttpClient::soup_session_send_complete_cb(GObject* object, GAsyncResult* result, gpointer user_data)
{
    SoapRequestData* rd = reinterpret_cast<SoapRequestData*>(user_data);
    if (rd->in) {
        orion::log_debug("soup_session_send_complete_cb called but data->in is not null, this should not happen, data->in should only be set in this callback and should be null before this callback is called");
        throw std::logic_error("data input stream must be null when soup_session_send_complete_cb is called");
    }

    SoupHttpClient* self = rd->client;
    SoupSession* session = SOUP_SESSION(object);
    SoupMessage* msg = rd->msg;

    GError* error = NULL;
    rd->in = soup_session_send_finish(session, result, &error);
    //defer([in]{ if (in) { g_object_unref(in); } });

    if (!rd->in || error) {
        defer([error] { if (error) { g_error_free(error); } });
        SoupHttpClient::dispatch_error_response(rd, error);
        rd->close_input_stream();
        return;
    }

    int32_t status_code = soup_message_get_status(msg);
    HttpClient::Cookies cookie_dtos = self->extract_cookies_from_soup_message(msg);
 
    Headers headers = soup_message_headers_to_vector(msg, soup_message_get_response_headers(msg));
    size_t body_length = soup_message_headers_get_content_length(soup_message_get_response_headers(msg));
    rd->response.error = Error::ok();
    rd->response.body.total_content_length = body_length;
    rd->response.headers = std::move(headers);
    rd->response.cookies = std::move(cookie_dtos);
    rd->response.status_code = status_code;
    rd->response.state = HttpClient::ResponseState::headers;
    self->dispatch_response_callback(rd->callback, rd->response);

    //not really clear how g_input_stream_read_all_async behaves, specifically what happens if there;s less data to read then the buffer length.
    g_input_stream_read_async (rd->in, rd->output_buffer, output_buffer_size, G_PRIORITY_DEFAULT, self->m_cancellable
            , write_callback, rd);
 
}

bool
SoupHttpClient::maybe_reset_cancellable()
{
    bool was_cancelled = g_cancellable_is_cancelled(m_cancellable);
    if (was_cancelled && m_pending_callbacks == 0 && m_request_queue.empty()) {
        g_object_unref(m_cancellable);
        m_cancellable = g_cancellable_new();
        clear_finished_request_data();
        return true;
    }
    return !was_cancelled;
}

void
SoupHttpClient::clear_finished_request_data() {
    for (auto* rd : m_finished_request_data) {
        delete rd;
    }
    m_finished_request_data.clear();
}

void
SoupHttpClient::do_http_request(SoupHttpClient* self, HttpClient::RequestDto& request, HttpClient::ResultCb cb, HttpClient::DataCb data_cb)
{
    if (self->m_stop_worker) {
       return;
    }

    //well, hmmm, if a user cancels and then immediately makes another request 
    //then we might end up in a situation where the cancellable is cancelled but the callback for the 
    //new request is not yet called so we will not dispatch the cancel cb until the new request completes
    //, which sort of denies the purpose of the cancel all together. Huh.
    if (!self->maybe_reset_cancellable()) {
        if (cb) {
            cb(HttpClient::ResponseDto{.error = MAKE_ERROR(ErrorCode::cancelled, "Request made while client is cancelled and there're still ongoing requests to finish."), .body = {}
                , .state = HttpClient::ResponseState::done });
        } else {
            orion::log_warning("Request made while client is cancelled and there're still ongoing requests to finish, but no callback to report the error.");
        }
        return;
    }
    SoupMessage* msg = soup_message_new(request.method.c_str(), request.raw_url.c_str()); // we should ensure this is freed

    if (!msg) {
        orion::log_error("do_http_request: failed to create SoupMessage");
        if (cb) {
            cb(HttpClient::ResponseDto{.error = MAKE_ERROR(ErrorCode::net_io, "failed to create SoupMessage"), .body = {}
                , .state = HttpClient::ResponseState::done });
        }
        return;
    }

    //Headers headers = soup_message_headers_to_vector(msg, soup_message_get_request_headers(msg));
    if (self->m_interceptor) {
        self->m_interceptor(request);
    }
    for (const auto& header : request.headers) {
        soup_message_headers_append(soup_message_get_request_headers(msg), header.first.c_str(), header.second.c_str());
    }
    if (!request.body.content_type.empty()) {
        soup_message_headers_set_content_type(soup_message_get_request_headers(msg), request.body.content_type.c_str(), NULL);
    }
 
    SoapRequestData* data = new SoapRequestData{
        .client = self, //does it make sens to guard it by shared_from_this or weak_from_this? or we should just rely on
        //sanity of the user of the HttpClientIntf that they will not destroy the client until the callback is called?
        .msg = msg,
        .callback = std::move(cb),//Here we are sort of prepared for the fact that each request has it's own callback.
        .data_callback = std::move(data_cb)
    };
    if (data->data_callback) {
        //sending HTTP request with body content provided by the data_callback callback.
        std::string cl = headers_get(request.headers, "Content-Length", "");
        gsize total_content_length = request.body.total_content_length != HTTP_CONTENT_LENGTH_UNDEFINED ? request.body.total_content_length
                            : (!cl.empty() ? std::stoll(cl) : HTTP_CONTENT_LENGTH_UNDEFINED);
        if (total_content_length == HTTP_CONTENT_LENGTH_UNDEFINED) {
            //configure chunked transfer encoding if content length is not provided. this is needed for streaming upload of request body when we don't know the total size in advance.
            SoupMessageHeaders* request_headers = soup_message_get_request_headers(msg);
            soup_message_headers_set_encoding(request_headers, SOUP_ENCODING_CHUNKED);
            soup_message_headers_set_content_length(request_headers, HTTP_CONTENT_LENGTH_UNDEFINED);
            soup_message_get_connection_id(msg); //this is needed to make soup use chunked transfer encoding for the request body when content length is not provided. 
            //seems like a weird quirk of libsoup but without this call the request body will be sent with content length 0 even if we set the encoding to 
            //chunked and content length to undefined.
        } else {
            soup_message_headers_set_content_length(soup_message_get_request_headers(msg), total_content_length);
        }
        //here we need to copy the data unfortunatelly, as the stream takes ownership of the pointer.
        char* buffer = new char[request.body.content.size()];
        std::copy(request.body.content.data(), request.body.content.data() + request.body.content.size(), buffer);

        data->out = g_memory_input_stream_new_from_data(buffer, request.body.content.size(), [](gpointer data) {
            delete[] static_cast<char*>(data);
        });
        soup_message_set_request_body(msg, request.body.content_type.c_str(), data->out, total_content_length);
        data->wrote_body_data_signal_id = g_signal_connect(data->msg, "wrote-body-data", G_CALLBACK(on_soup_signal_wrote_body_data), data);
        data->out_buffered_body_data = request.body.content.size();

    } else if (request.has_body()) {
        //this call doesn't copy data but the data must not be manipulated by the caller after this call since soup will use the provided pointer directly when sending the request body.
        data->body_bytes = g_bytes_new_with_free_func(request.body.content.data(), request.body.content.size()
                                                , [](gpointer data) {
                                                 /*does nothing since we don't want the date to be in fact deleted*/}
                                                 , nullptr);
 
        soup_message_set_request_body_from_bytes(msg, request.body.content_type.c_str(), data->body_bytes);
        //to be able to use chunked transfer we need to use server mode of the soap lib, uff
    }
    data->finished_signal_id = g_signal_connect(data->msg, "finished", G_CALLBACK(on_soup_signal_finished), data);
    self->callback_started("soup_session_send_complete_cb");
    data->request = std::move(request);

    soup_session_send_async(
        self->m_session,
        msg,
        G_PRIORITY_DEFAULT,
        self->m_cancellable, // cancellable
        SoupHttpClient::soup_session_send_complete_cb,
        data // user_data
    );
}

bool
SoupHttpClient::process()
{
    HttpClientIntf::process();
    try_dispatch_cancel_cb();
    orion::log_debug("process m_pending_callbacks %d, m_result_queue %d, m_request_queue %d", m_pending_callbacks, m_result_queue.empty(), m_request_queue.empty());
    return m_pending_callbacks > 0 || !m_result_queue.empty() || !m_request_queue.empty();
}

bool

SoupHttpClient::block()
{
    bool result = true;
    while (result && (m_pending_callbacks > 0 || !m_request_queue.empty())) {
        result = HttpClientIntf::block();
    }
    try_dispatch_cancel_cb();
    return result;
}

//public interface

void SoupHttpClient::get(const std::string& end_point, HttpClient::ResultCb cb)
{
    HttpClient::RequestDto request{
        .method = METHOD_GET.data(),
        .raw_url = m_base_url.uri() + end_point,
        .body = {},
        .headers = {}
    };
    post_request_to_worker(PendingRequest{cb, request, nullptr});
}


void SoupHttpClient::post(const std::string& end_point, const HttpClient::RequestDto::Body&& body, HttpClient::ResultCb cb, HttpClient::DataCb data_cb)
{
    HttpClient::RequestDto request;
    post_request_to_worker(PendingRequest{cb, {
        .method = METHOD_POST.data(),
        .raw_url = m_base_url.uri() + end_point,
        .body = std::move(body),
        .headers = {}
    }, data_cb});
}


void SoupHttpClient::remove(const std::string& end_point, HttpClient::ResultCb cb)
{
    post_request_to_worker(PendingRequest{cb, {
        .method = METHOD_DELETE.data(),
        .raw_url = m_base_url.uri() + end_point,
        .body = {},
        .headers = {}
    }, nullptr});
}


void SoupHttpClient::patch(const std::string& end_point, const HttpClient::RequestDto::Body&& body, HttpClient::ResultCb cb, HttpClient::DataCb data_cb)
{
    post_request_to_worker(PendingRequest{cb, {
        .method = METHOD_PATCH.data(),
        .raw_url = m_base_url.uri() + end_point,
        .body = std::move(body),
        .headers = {}
    }, data_cb});
}

void SoupHttpClient::execute(HttpClient::RequestDto request, HttpClient::ResultCb cb, HttpClient::DataCb data_cb)
{
    //check if we have the full url, otherwise prepend base
    std::regex rgx ("^https?://", std::regex_constants::syntax_option_type::_S_icase);
    if (!std::regex_search(request.raw_url, rgx, std::regex_constants::match_continuous)) {
        request.raw_url = m_base_url.uri() + request.raw_url;
    }
    //std::regex_search(request.raw_url.begin(), request.raw_url.end(), rgx, std::regex_constants::match_continuous|std::regex_constants::icase);
    post_request_to_worker(PendingRequest{std::move(cb), std::move(request), data_cb ? std::move(data_cb) : nullptr});
}

void
SoupHttpClient::try_dispatch_cancel_cb()
{
    if (m_pending_callbacks == 0) {
        HttpClientIntf::try_dispatch_cancel_cb();
    }
}

void
SoupHttpClient::cancel(HttpClient::CancelCb cb)
{
    g_cancellable_cancel(m_cancellable);
    HttpClientIntf::cancel(cb);
}

} // namespace orion::sync
#endif // USING_SOUP
