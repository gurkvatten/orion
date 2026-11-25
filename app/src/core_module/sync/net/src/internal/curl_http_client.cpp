#include "http_client_intf.h"
#define USING_CURL_TIMER_THREAD 0
#if NET_LIB == USING_CURL
#include <../common/src/timer.h>
#include <../common/src/defer.h>
#include <../common/src/string_util.h>
#if USING_CURL_TIMER_THREAD == 1
#include <../common/src/cross_thread_map.h>
#include <thread>
#include <mutex>
#else
#include <../common/src/single_thread_map.h>
#endif
#include <stddef.h>
#include <string>
#include <string.h> //memcpy & strerror & strerror
#include <exception>
#include <chrono>
#include <curl/curl.h>

//https://curl.se/libcurl/c/libcurl-tutorial.html authentication / ssl.

namespace orion::net {

static int64_t s_curl_init_count = 0;

class CurlHttpClient;

struct CurlRequestData {
    CURL* handle = nullptr;
    HttpClient::ResultCb callback;
    HttpClient::DataCb data_callback;
    curl_slist *request_headers = nullptr;
    HttpClient::ResponseDto response = {};
    HttpClient::RequestDto request = {};
    CurlHttpClient* client = nullptr;
    bool body_content_read = true;

    ~CurlRequestData() {
        if (request_headers) {
            curl_slist_free_all(request_headers);
        }
    }
};

#if USING_CURL_TIMER_THREAD == 1
struct PendingRequest {
    HttpClient::ResultCb cb = nullptr;
    HttpClient::RequestDto request = {};
    HttpClient::DataCb data_cb = nullptr;
};
#endif

class CurlHttpClient : public HttpClientIntf {
public: // interface implementations
    void get(const std::string& end_point, HttpClient::ResultCb cb = nullptr) override;
    void post(const std::string& end_point,  const HttpClient::RequestDto::Body&& body, HttpClient::ResultCb cb = nullptr, HttpClient::DataCb data_cb = nullptr) override;
    void remove(const std::string& end_point, HttpClient::ResultCb cb = nullptr) override;
    void patch(const std::string& end_point,  const HttpClient::RequestDto::Body&& body, HttpClient::ResultCb cb = nullptr, HttpClient::DataCb data_cb = nullptr) override;

    void execute(HttpClient::RequestDto request, HttpClient::ResultCb cb = nullptr, HttpClient::DataCb data_cb = nullptr) override;

    bool process() override;
    bool block() override;
    void cancel(HttpClient::CancelCb cb) override;

public:
    CurlHttpClient(const Uri&& base_url, std::string session_cookie_names)
        : HttpClientIntf(std::forward<const Uri>(base_url), std::move(session_cookie_names))
        , m_active_requests([]() { return nullptr; }, [](CurlRequestData* rd) {
            //handles arte cleaned up separately in cleanup_request().
            if (rd) {
                delete rd;
            }
        })
#if USING_CURL_TIMER_THREAD == 1
        , m_multi_processing_pacer(tm::Timer::create())
#endif
    {
        try {
            initialize_curl();
        } catch (const std::runtime_error& ex) {
            orion::log_error("CurlHttpClient initialization failed: %s", ex.what());
            cleanup_all();
        }
    }
    ~CurlHttpClient()
    {
        cleanup_all();
        if (m_multi_handle) {
            curl_multi_cleanup(m_multi_handle);
            m_multi_handle = nullptr;
        }
        if (--s_curl_init_count == 0) {
            curl_global_cleanup();
        }
    }

    void dispatch_response_callback(HttpClient::ResultCb cb, HttpClient::ResponseDto& response);
    HttpClient::Cookies extract_cookies_from_handle(CURL* handle);

private:
    void initialize_curl();
    void cleanup_all();
    int multi_perform();
    int multi_process(bool process_all);
    void do_http_request(HttpClient::RequestDto request, HttpClient::ResultCb cb, HttpClient::DataCb data_cb = nullptr);
    CurlRequestData* request_data_from_active_requests(CURL* handle);
    CURLMsg* next_curl_msg();
    virtual void try_dispatch_cancel_cb() override;

private:
#if USING_CURL_TIMER_THREAD == 1
    using ActiveRequestsMap = CrossThreadMap<CURL*, CurlRequestData*, std::function<CurlRequestData*()>, std::function<void(CurlRequestData*)>>;
#else
    using ActiveRequestsMap = SingleThreadMap<CURL*, CurlRequestData*, std::function<CurlRequestData*()>, std::function<void(CurlRequestData*)>>;
#endif
    CURLM *m_multi_handle = nullptr;
    ActiveRequestsMap m_active_requests;
    int m_msgs_left = 0;

#if USING_CURL_TIMER_THREAD == 1
private:
    void check_multi_process_pacer();
    std::unique_ptr<orion::tm::Timer> m_multi_processing_pacer;
    std::mutex m_multi_processing_mutex;
#endif
};
//this method should only ever have 1 implementation and that is the one using libsoup
//, if we using a different native net lib ten there needs to somewhere be a different implementation of 
//and this one has to be conditionally #ifdeffed out possibly with the rest of the file content.
std::shared_ptr<HttpClientIntf>
HttpClientIntf::create(const Uri&& base_url, std::string session_cookie_names)
{
    auto client = std::make_shared<CurlHttpClient>(std::forward<const Uri>(base_url), std::move(session_cookie_names));
    return client;
}

void
CurlHttpClient::initialize_curl()
{
    CURLcode code;
    if (s_curl_init_count == 0) {
        if ((code = curl_global_init(CURL_GLOBAL_DEFAULT)) != CURLE_OK) {
            throw std::runtime_error(orion::str::format("Failed to initialize curl: %s", curl_easy_strerror(code)));
        }
    }
    m_multi_handle = curl_multi_init();
    if (!m_multi_handle) {
        throw std::runtime_error(orion::str::format("Failed to initialize curl_multi: %s", strerror(errno)));
    }
    s_curl_init_count++;
}

static void
cleanup_request(CURL* handle, CURLM* multi_handle)
{
#if USING_CURL_TIMER_THREAD == 1
    std::lock_guard<std::mutex> lock(m_multi_processing_mutex);
#endif
    curl_multi_remove_handle(multi_handle, handle);
    curl_easy_cleanup(handle);
}

void
CurlHttpClient::cleanup_all()
{
    m_active_requests.for_each([this](CURL* const& handle, CurlRequestData* const& rd) {
        cleanup_request(const_cast<CURL*>(handle), m_multi_handle);
    });
    m_active_requests.clear();
#if USING_CURL_TIMER_THREAD == 1
    m_multi_processing_pacer->stop();
#endif
}

void
CurlHttpClient::dispatch_response_callback(HttpClient::ResultCb cb, HttpClient::ResponseDto& response)
{
    //response needs to be copied here since it's still needed as the request progresses.
#if USING_CURL_TIMER_THREAD == 1
    m_result_queue.push({DoneRequest{.cb = std::move(cb), .response = response}});
#else
    cb(response);
#endif
}

CurlRequestData*
CurlHttpClient::request_data_from_active_requests(CURL* handle)
{
    auto rd = m_active_requests.find_and_erase_entry(handle);
    return rd;
}

static size_t
header_callback(char *buffer, size_t size, size_t nitems, void *userdata)
{
    CurlRequestData *rd = static_cast<CurlRequestData*>(userdata);
    Headers &headers = rd->response.headers;
    std::string header(buffer, size * nitems);
    // Remove trailing \r\n
    if (!header.empty() && header.back() == '\n') header.pop_back();
    if (!header.empty() && header.back() == '\r') header.pop_back();
    if (!header.empty()) {
        size_t colon_pos = header.find(':');
        if (colon_pos != std::string::npos) {
            std::string name = header.substr(0, colon_pos);
            std::string value = header.substr(colon_pos + 1);
            // Trim leading/trailing spaces
            str::trim(&name);
            str::trim(&value);
            headers.emplace_back(name, value);
        }
    }
    std::string cl = headers_get(rd->response.headers, "Content-Length", "");
    rd->response.body.total_content_length = !cl.empty() ? std::stoll(cl) : HTTP_CONTENT_LENGTH_UNDEFINED;
    rd->response.error = Error::ok();
    rd->response.cookies = rd->client->extract_cookies_from_handle(rd->handle);
    rd->response.status_code = 200; //don't check the status just yet so we can get to the potential error message in the body and we'll check the correct status at the end
    rd->response.state = HttpClient::ResponseState::headers;
    rd->client->dispatch_response_callback(rd->callback, rd->response);

    return size * nitems;
}

HttpClient::Cookies
CurlHttpClient::extract_cookies_from_handle(CURL* handle)
{
    struct curl_slist *cookies = nullptr;
    if (m_session_cookie_names.empty() || curl_easy_getinfo(handle, CURLINFO_COOKIELIST, &cookies) != CURLE_OK || !cookies) {
        return {};
    }
    defer([cookies]{ curl_slist_free_all(cookies); });

    HttpClient::Cookies cookie_dtos;
    for (struct curl_slist *l = cookies; l != nullptr; l = l->next) {
        std::string cookie_str(l->data);
        // The cookie string format is: "domain\tflag\tpath\tsecure\texpire\tname\tvalue"
        std::deque<std::string> parts = str::split(cookie_str, '\t');
 
        if (parts.size() == 7) {
            HttpClient::CookieDto cookie_dto{
                .name = parts[5],
                .value = parts[6],
                .domain = parts[0],
                .path = parts[2],
                .secure = parts[3] == "TRUE",
                .http_only = false, // libcurl does not provide httpOnly info in the cookie list
                .expires = std::stoll(parts[4]),
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
    }
    return cookie_dtos;
}

//https://curl.se/libcurl/c/CURLOPT_READFUNCTION.html
static size_t
read_callback(char *buffer, size_t size, size_t nitems, void *user_data)
{   
    CurlRequestData *rd = static_cast<CurlRequestData*>(user_data);
    if (rd->body_content_read && !rd->request.body.content.empty()) {
        //if we have body content to send then we supply it in the first read callback call, if the body content is larger than the buffer size then the rest will be supplied in subsequent calls.
        size_t total_size = size * nitems;
        size_t to_copy = (std::min)(total_size, rd->request.body.content.size());
        ::memcpy(buffer, rd->request.body.content.data(), to_copy);
        rd->request.body.content.erase(0, to_copy);
        rd->body_content_read = !rd->request.body.content.empty(); //we continue reading untill there's some body content left.
        orion::log_debug("read_callback supplying %zu bytes from body content, %zu bytes left", to_copy, rd->request.body.content.size());
        return to_copy; 
    } else if (rd->data_callback) {
        size_t max_size = size * nitems;
        //TODO shouldn't we call this callback using similar thing as dispatch_response_callback? Or leave it as is?
        size_t supplied_size = rd->data_callback(rd->response, buffer, max_size);
        rd->body_content_read = false; //once we'r here we''ll nvere read from the body again for this request
        return supplied_size;
    }
    return CURL_READFUNC_ABORT; //no data callback set, cannot supply data, abort the request
}

static size_t
write_callback(char *received, size_t size, size_t blocks, void *request_data)
{
    CurlRequestData *rd = static_cast<CurlRequestData*>(request_data);
    size_t total_size = size * blocks;
    rd->response.body.content.append(received, total_size);
    //check if we had a content lkngth header and hence can set a total content size. if not we'll leave it as HTTP_CONTENT_LENGTH_UNDEFINED.
    std::string cl = headers_get(rd->response.headers, "Content-Length", "");
    rd->response.body.total_content_length = !cl.empty() ? std::stoll(cl) : HTTP_CONTENT_LENGTH_UNDEFINED;
    rd->response.state = HttpClient::ResponseState::data;
    //TODO we cannot really clear the body content here since the dispatch might be async so we relt on the client to clear it after processing.
    //This however, can lead to reace is we run in different thread that user processes the callback.
    //Hence we should introduce a 2 synchronized methods into ResponseDTO:
    //  - one for appending data
    //  - one for getting & clearing it
    rd->client->dispatch_response_callback(rd->callback, rd->response);
    rd->response.body.content.clear();
    return total_size; //CURL_WRITEFUNC_ERROR aborts the transfer.
}

CURLMsg*
CurlHttpClient::next_curl_msg()
{
#if USING_CURL_TIMER_THREAD == 1
    std::lock_guard<std::mutex> lock(m_multi_processing_mutex);
#endif
    return curl_multi_info_read(m_multi_handle, &m_msgs_left);
}

int
CurlHttpClient::multi_perform()
{
#if USING_CURL_TIMER_THREAD == 1
    std::lock_guard<std::mutex> lock(m_multi_processing_mutex);
#endif
    int running_handles = 0;
    CURLMcode result = curl_multi_perform(m_multi_handle, &running_handles);
    if (result != CURLM_OK) {
        orion::log_error("curl_multi_perform failed: %s", curl_multi_strerror(result));
        //here we need to dispatch it to all active ewquests.
        m_active_requests.for_each([this, result](CURL* const& handle, CurlRequestData* const& rd) {
            rd->response.error = MAKE_ERROR(ErrorCode::net_io, orion::str::format("curl_multi_perform failed: %s", curl_multi_strerror(result)).c_str());
            rd->response.state = HttpClient::ResponseState::done;
            dispatch_response_callback(rd->callback, rd->response);
        });
        cleanup_all();
        return -1;
    }
    return running_handles;
}

//this method is called either from the pacer thread or from the user thread depending on the configuration (see USING_CURL_TIMER_THREAD)
int
CurlHttpClient::multi_process(bool process_all)
{
    //we could use curl_multi_poll here instead of just sleeping for some time in block method
    //, but 
    //      - it seems that curl_multi_poll is not safe to be called from other thread than the one calling curl_multi_add_handle and curl_multi_perform
    //      - the performance gain is not that big since we will be notified about the completed request by the OS anyway by curl_multi_read_info.
    //      - it's a bit of a hassle while cancel since we would need to rely on curl_multi_wakeup() and then remove easy handles.
    int running_handles = multi_perform();
    int done = -1;
    if (running_handles < 0) {
        return done;
    }
    //TODO  curl_multi_info_read seems to be not safe to be called from other thread
    //than curl_multi_add_handle
    CURLMsg* msg = nullptr;
    m_msgs_left = 0;
    ++done;
    //this (running_handles || !m_active_requests.empty()) if there since ,in extreme case or an faast error case, we might finish 
    //the requests earlier then the curl_multi_perform loop comes about to still detect activity.
    while ((running_handles || !m_active_requests.empty()) && (msg = next_curl_msg()) != nullptr) {
        if (msg->msg == CURLMSG_DONE) {
            CURL* handle = msg->easy_handle;
            defer([handle, this](){ //remove handle even if we don't find it in the map below as it's udeless anyway
                cleanup_request(handle, m_multi_handle);
            });

            CurlRequestData* rd = request_data_from_active_requests(handle);
            if (rd) {
                defer([rd](){
                    delete rd;
                });

                long http_code = 0;
                curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &http_code);

                rd->response.status_code = http_code;
                rd->response.error = msg->data.result != CURLE_OK
                    ? MAKE_ERROR(ErrorCode::net_io, curl_easy_strerror(msg->data.result))
                    : Error::ok();
                if (rd->response.body.total_content_length == HTTP_CONTENT_LENGTH_UNDEFINED) {
                    //if we haven't already set content length in the write callback due to exceeding HTTP_MAX_BODY_LENGTH, then we try to get it from headers or set it to 0 if not present.
                    std::string cl = headers_get(rd->response.headers, "Content-Length", "");
                    rd->response.body.total_content_length = !cl.empty() ? std::stoll(cl) : HTTP_CONTENT_LENGTH_UNDEFINED;
                }
                if (rd->response.cookies.empty()) {
                    rd->response.cookies = extract_cookies_from_handle(handle);
                }
                rd->response.state = HttpClient::ResponseState::done;
                ++done;
                dispatch_response_callback(rd->callback, rd->response);
            } else {
                if (curl_easy_getinfo(handle, CURLINFO_PRIVATE, &rd) == CURLE_OK && rd) {
                    delete rd;
                }
            }
        }
    }
    return done;
}

#if USING_CURL_TIMER_THREAD == 1
//these two methods below are always called from the user thread
void
CurlHttpClient::check_multi_process_pacer() {
    if (m_active_requests.empty()) {
        m_multi_processing_pacer->stop();
    } else {
        //no need to check is_running since we alwas ever use the same parameters and the start methid takes care of the rest.
        m_multi_processing_pacer->start([this]() -> bool {
            multi_process(true);
            return true; //keep timer going
        }
        ,std::chrono::milliseconds(200), true);
    }
}
#endif

bool
CurlHttpClient::process()
{
#if USING_CURL_TIMER_THREAD == 1
    check_multi_process_pacer();
    HttpClientIntf::process();
    try_dispatch_cancel_cb();
    return !m_active_requests.empty() || !m_result_queue.empty();
#else
    std::this_thread::sleep_for(std::chrono::microseconds(200));
    int done = multi_process(false);
    try_dispatch_cancel_cb();
    return done > -1 && !m_active_requests.empty(); //process returns true when user should call it again
#endif
}

bool
CurlHttpClient::block()
{
#if USING_CURL_TIMER_THREAD == 1
    check_multi_process_pacer();
    bool result = true;
    while (result && !m_active_requests.empty()) {
        result = HttpClientIntf::block();
    }
    try_dispatch_cancel_cb();
    return result;
#else
    int done = multi_process(true);
    uint64_t elapsed_time = -200;
    while (done > -1 && !m_active_requests.empty() && (elapsed_time += 200) < HTTP_CLIENT_DEFAULT_TIMEOUT_MS) {
        std::this_thread::sleep_for(std::chrono::microseconds(200));
        done = multi_process(true);
    }
    try_dispatch_cancel_cb();
    return done > -1 && m_active_requests.empty();//block succeeds when all has been done
#endif
}

static void
set_request_method(CURL* handle, const CurlRequestData* rd)
{
    if (rd->request.method == METHOD_POST) {
        curl_easy_setopt(handle, CURLOPT_POST, 1L);
    } else if ((rd->request.method == METHOD_PUT || rd->request.method == METHOD_PATCH)
        && rd->data_callback) {
        // CURLOPT_UPLOAD enables libcurl's read/upload state machine. Only use
        // it when a streaming body callback is actually available. In
        // particular, an empty PUT (such as Bitwarden's soft-delete request)
        // must be sent as a bodyless custom request instead of waiting for
        // upload data that will never arrive.
        curl_easy_setopt(handle, CURLOPT_UPLOAD, 1L);
    }
    curl_easy_setopt(handle, CURLOPT_CUSTOMREQUEST, rd->request.method.c_str());
}

#ifdef _DEBUG
/*
typedef enum {
  CURLINFO_TEXT = 0,
  CURLINFO_HEADER_IN,
  CURLINFO_HEADER_OUT,
  CURLINFO_DATA_IN, 
  CURLINFO_DATA_OUT,
  CURLINFO_SSL_DATA_IN,
  CURLINFO_SSL_DATA_OUT,
  CURLINFO_END
} curl_infotype;
*/
int debug_callback(CURL *handle, curl_infotype type, char *data, size_t size, void *clientp)
{
    std::string_view prefix;
    switch (type) {
        case CURLINFO_TEXT: prefix = "CURLINFO_TEXT"; break;
        case CURLINFO_HEADER_IN: prefix = "CURLINFO_HEADER_IN"; break;
        case CURLINFO_HEADER_OUT: prefix = "CURLINFO_HEADER_OUT"; break;
        case CURLINFO_DATA_IN: prefix = "CURLINFO_DATA_IN"; break;
        case CURLINFO_DATA_OUT: prefix = "CURLINFO_DATA_OUT"; break;
        case CURLINFO_SSL_DATA_IN: prefix = "CURLINFO_SSL_DATA_IN"; break;
        case CURLINFO_SSL_DATA_OUT: prefix = "CURLINFO_SSL_DATA_OUT"; break;
        default: prefix = "CURLINFO_?"; break;
    }
    if (type != CURLINFO_DATA_OUT) { //don't log outgoing data since it can be very large and we already log it in the read callback
        orion::log_debug("%.*s: %.*s", (int)prefix.size(), prefix.data(), (int)size, data);
    }
    return 0;
}
#endif

void
CurlHttpClient::do_http_request(HttpClient::RequestDto request, HttpClient::ResultCb cb, HttpClient::DataCb data_cb)
{
    if (!m_multi_handle) {
        if (cb) {
            cb(HttpClient::ResponseDto{.error = MAKE_ERROR(ErrorCode::net_io, "Failed to initialize curl http client multi handle."), .body = {}
                , .state = HttpClient::ResponseState::done });
        }
        return;
    }
    CURL* handle = curl_easy_init();
    if (!handle) {
        if (cb) {
            cb(HttpClient::ResponseDto{.error = MAKE_ERROR(ErrorCode::net_io, "Failed to initialize curl easy handle"), .body = {}
                , .state = HttpClient::ResponseState::done });
        }
        return;
    }

    CurlRequestData* rd = new CurlRequestData{
        .handle = handle,
        .callback = std::move(cb),
        .data_callback = data_cb ? std::move(data_cb) : nullptr,
        .request = std::move(request),
        .client = this
    };

    if (m_interceptor) {
        m_interceptor(rd->request);
    }

    curl_easy_setopt(handle, CURLOPT_URL, rd->request.raw_url.c_str());
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, rd);
    curl_easy_setopt(handle, CURLOPT_HEADERFUNCTION, header_callback);
    curl_easy_setopt(handle, CURLOPT_HEADERDATA, rd);
    curl_easy_setopt(handle, CURLOPT_PRIVATE, rd);
    curl_easy_setopt(handle, CURLOPT_BUFFERSIZE, HTTP_MAX_BODY_LENGTH * 2);
    if (rd->request.follow_redirects) {
        curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(handle, CURLOPT_MAXREDIRS, 20L);
        curl_easy_setopt(handle, CURLOPT_REDIR_PROTOCOLS_STR, "http,https");
    }
    #ifdef _DEBUG
    //curl_easy_setopt(handle, CURLOPT_VERBOSE, 1L);
    //curl_easy_setopt(handle, CURLOPT_DEBUGFUNCTION, debug_callback);
    #endif

    struct curl_slist *slist = nullptr;
    // Set headers
    for (const auto& header : rd->request.headers) {
        if (!rd->request.body.content_type.empty()
            && orion::str::equals_case_insensitive(header.first, "Content-Type")) {
            continue;
        }
        if (rd->request.has_body()
            && orion::str::equals_case_insensitive(header.first, "Content-Length")) {
            continue;
        }
        std::string header_str = header.first + ": " + header.second;
        slist = curl_slist_append(slist, header_str.c_str());
    }

    if (!rd->request.body.content_type.empty()) {
        slist = curl_slist_append(slist, ("Content-Type: " + rd->request.body.content_type).c_str());
    }

    // Set HTTP method
    set_request_method(handle, rd);
    // Set request body if present
    //CURLOPT_READDATA and CURLOPT_READFUNCTION can be set here if we want to support streaming upload of request body.
    //for now this should be enough. CURLOPT_POSTFIELDS can be used with PATCH, PUT and DELETE as well
    //, even if semantically it's weird naming and it doesn't make much sense for DELETE, but some servers might expect it.
    if (rd->data_callback) {
        curl_easy_setopt(handle, CURLOPT_READFUNCTION, read_callback);
        curl_easy_setopt(handle, CURLOPT_READDATA, rd);
        //slist = curl_slist_append(slist, "Connection: keep-alive");
        std::string cl = headers_get(rd->request.headers, "Content-Length", "");
        size_t total_content_length = request.body.total_content_length != HTTP_CONTENT_LENGTH_UNDEFINED ? request.body.total_content_length
                            : (!cl.empty() ? std::stoll(cl) : HTTP_CONTENT_LENGTH_UNDEFINED);
        if (total_content_length == HTTP_CONTENT_LENGTH_UNDEFINED) {
            //configure chunked transfer encoding if content length is not provided. this is needed for streaming upload of request body when we don't know the total size in advance.
            slist = curl_slist_append(slist, "Transfer-Encoding: chunked");
            slist = curl_slist_append(slist, "Expect:"); //disable "Expect: 100-continue" header which causes delay in some cases when sending large body with POST or PUT
        }
        if (rd->request.method == METHOD_PUT) {
            curl_easy_setopt(handle, CURLOPT_INFILESIZE_LARGE, total_content_length);
        } else {
            curl_easy_setopt(handle, CURLOPT_POSTFIELDSIZE_LARGE, total_content_length);
        }
    } else if (rd->request.has_body()) {
        curl_easy_setopt(handle, CURLOPT_POSTFIELDS, rd->request.body.content.c_str());
        curl_easy_setopt(handle, CURLOPT_POSTFIELDSIZE, rd->request.body.content.size());
        std::string content_length_header = "Content-Length: " + std::to_string(rd->request.body.content.size());
        slist = curl_slist_append(slist, content_length_header.c_str());
    }

    if (slist) {
        curl_easy_setopt(handle, CURLOPT_HTTPHEADER, slist);
        rd->request_headers = slist; // Store for cleanup
    }

    CURLMcode mc;
    {
        mc = curl_multi_add_handle(m_multi_handle, handle);
        if (mc == CURLM_OK) {
            m_active_requests.insert(handle, rd);
            return;
        }
    }
    defer([rd](){
        delete rd;
    });
    if (cb) {
        cb(HttpClient::ResponseDto{.error = MAKE_ERROR(ErrorCode::net_io, curl_multi_strerror(mc)), .body = {}
            , .state = HttpClient::ResponseState::done });
    }
    if (slist) {
        curl_slist_free_all(slist);
    }
    curl_easy_cleanup(handle);
}

void
CurlHttpClient::get(const std::string& end_point, HttpClient::ResultCb cb)
{
    do_http_request({
            .method = METHOD_GET.data(),
            .raw_url = m_base_url.uri() + end_point,
            .body = {},
            .headers = {}
        }
        , cb);
}

void
CurlHttpClient::post(const std::string& end_point,  const HttpClient::RequestDto::Body&& body, HttpClient::ResultCb cb, HttpClient::DataCb data_cb)
{
    do_http_request({
            .method = METHOD_POST.data(),
            .raw_url = m_base_url.uri() + end_point,
            .body = std::move(body),
            .headers = {}
        }
        , cb, data_cb);
}

void
CurlHttpClient::remove(const std::string& end_point, HttpClient::ResultCb cb)
{
    do_http_request({
            .method = METHOD_DELETE.data(),
            .raw_url = m_base_url.uri() + end_point,
            .body = {},
            .headers = {}
        }
        , cb);
}

void
CurlHttpClient::patch(const std::string& end_point,  const HttpClient::RequestDto::Body&& body, HttpClient::ResultCb cb, HttpClient::DataCb data_cb)
{
    do_http_request({
            .method = METHOD_PATCH.data(),
            .raw_url = m_base_url.uri() + end_point,
            .body = std::move(body),
            .headers = {}
        }
        , cb, data_cb);
}

void
CurlHttpClient::execute(HttpClient::RequestDto request, HttpClient::ResultCb cb, HttpClient::DataCb data_cb)
{
    std::regex rgx ("^https?://", std::regex_constants::icase);
    if (!std::regex_search(request.raw_url, rgx, std::regex_constants::match_continuous)) {
        request.raw_url = m_base_url.uri() + request.raw_url;
    }
    do_http_request(std::move(request), std::move(cb), std::move(data_cb));
}

void
CurlHttpClient::try_dispatch_cancel_cb()
{
    if (m_active_requests.empty()) {
        HttpClientIntf::try_dispatch_cancel_cb();
#if USING_CURL_TIMER_THREAD == 1
        void check_multi_process_pacer();
#endif
    }
}

void
CurlHttpClient::cancel(HttpClient::CancelCb cb)
{
    //TODO send cancel signal to curl somehow?
    HttpClientIntf::cancel(cb);
    //here we rely on the fact that curl_multi_xxx is thread safe. we could introduce a guard though.
    cleanup_all();
    try_dispatch_cancel_cb();
}

}// namespace orion::net
#endif // NET_LIB == USING_CURL
