#pragma once
#include <../common/src/uri.h>
#include <../common/src/error.h>
#include <string>
#include <functional>
#include <memory>

//this should be decided by CMakeLists.txt WUN32, possibly more generaly on a wish expressed 
//by some NET_XX flag set in cmake files. It's don this way so that the flags here can be set 
//during development in any cmake independent way.
#define USING_SOUP 0
#define USING_CURL 1
//#define NET_LIB USING_CURL

#ifndef NET_LIB
#error "NET_LIB is not defined, please define it in CMakeLists.txt"
#endif

namespace orion::net {
/*
    This is a simple http client wrapper, it is used by the SyncExternalService
    and potentially any other application in need of http client functionality
    to perform http requests to the server
    The idea here is that we will always ever have one implementation of this client.
    Wich one of it will be ia meant to be a build time decission. Currently are evisaged two clients libSoup based and libCurl based.
    libSoup for gtk based platforms and libCurl for non gtk based platforms, as Windows and MacOS.

    The class is NOT thread safe and all api calls are either blocking orr async/callback calls.
    !IMPORTANT: the first iteration needs to have async callback set before any API call otherwise user won't get any indication obout
    the response. Hence currently there's no retun value from any calls
    There's supposed to be a 1-1 relatoonship between a thread and an instance of this class
    , the relationshiop is not enforced by the class even though we could introduce a static map as not having to missuse thread local
    
    The above para suggests that we should always have one requests of one instance active at a time even though the potential
    async/callback nature of the calls permits multiple active calls, however when used by a classical state driven user logic
    , it's best to have one active request at a time.
*/

constexpr std::string_view METHOD_POST = "POST";
constexpr std::string_view METHOD_PUT = "PUT";
constexpr std::string_view METHOD_DELETE = "DELETE";
constexpr std::string_view METHOD_PATCH = "PATCH";
constexpr std::string_view METHOD_GET = "GET";

constexpr int32_t HTTP_CLIENT_DEFAULT_TIMEOUT_MS = 30 * 1000; //30 seconds

//for bodies over the HTTP_MAX_BODY_LENGTH HttpClient start calling HttpClient::ResultCb for each incoming data part, see HTTP_STATUS_CONTINUE below
constexpr size_t HTTP_MAX_BODY_LENGTH = 1024 * 5;
constexpr size_t HTTP_CONTENT_LENGTH_UNDEFINED = ((size_t)(-1));
constexpr int HTTP_STATUS_UNDEFINED = -1;

//it's a vector since there, in theory, could be more occurences of the same header.
using Headers = std::vector<std::pair<std::string, std::string>>;
//2 helper methods to treat Headers as a map.
std::string headers_get(const Headers& headers, const std::string& name, const std::string& default_value = "");
void headers_set(Headers& headers, const std::string& name, const std::string& value);


class HttpClientIntf;

//to modify HttpClient::RequestDto before it's sent
class HttpClient {
public:
//this below status value is used in case of large body, either chunked or large content length whweby the HttpClient::ResultCb is beimng calle for every incomming
//part of the body. It's done when body size exceedes HTTP_MAX_BODY_LENGTH
//after the callback is called with DATA the body content is cleared and the next part of the body will be reported as a new response with the same status as 
//the original one, but with DATA to indicate that it's a continuation of the previous response. The last bit of the data comes in the file callback when the request completes.
    enum ResponseState {
        unknown,
        headers,
        data,
        done
    };

    enum OAuth2Scheme {
        Basic, //credentials are sent in the Authorization header as base64 encoded client_id:client_secret 
            //(i.e. user name, password)
        Digest, //credentials are sent using HTTP Digest authentication
        Bearer, //token
    };

    struct AccessToken {
        std::string access_token;
        std::string token_type;
        int expires_in = 0;
        int refresh_expires_in = 0;
        std::string refresh_token;
        std::string scope;
    };

    struct CookieDto {
        std::string name;
        std::string value;
        std::string domain;
        std::string path;
        bool secure = false;
        bool http_only = false;
        int64_t expires = 0; //unix timestamp
    };
    using Cookies = std::vector<CookieDto>;

    struct ResponseDto {
        struct Body {
            size_t total_content_length;
            std::string content;
        };
        Error error = Error::ok();
        Body body;
        Headers headers = {};
        HttpClient::Cookies cookies = {};
        int status_code = HTTP_STATUS_UNDEFINED;
        ResponseState state = ResponseState::unknown;
    };

    struct RequestDto {
        struct Body {
            std::string content_type;
            std::string content;
            size_t total_content_length = HTTP_CONTENT_LENGTH_UNDEFINED; //if != HTTP_CONTENT_LENGTH_UNDEFINED then it takes precedence over contet length header.
        };
        std::string method;
        std::string raw_url;
        Body body;
        Headers headers;
        bool follow_redirects = false;

        bool has_body() const {return body.content.size();}
    };

public:
    using ResultCb = std::function<void(ResponseDto)>;
    using DataCb = std::function<size_t(const ResponseDto&, char* buffer, size_t)>;//if set then it will be called to get body content instead of using body.content. size_t is the max allowed size for thim cb to retuirn.
    using CancelCb = std::function<void()>; 
    using Interceptor = std::function<void(RequestDto&)>;

public:
    explicit HttpClient(std::shared_ptr<HttpClientIntf> impl) : m_impl(std::move(impl)) {}

    HttpClient(const HttpClient&) = delete;
    HttpClient& operator=(const HttpClient&) = delete;
    HttpClient(HttpClient&&) = default;
    HttpClient& operator=(HttpClient&&) = default;

    //session_cookie_names => an '&' seperated list of cookie names of interest.
    //the session_cookie_names has to also begin and end with '&'.
    static std::shared_ptr<HttpClient> create(const Uri&& base_url, std::string session_cookie_names = "");
    std::string base_url() const;
    //if this cb is set then it will receive results and the method return values are undefined as the calls will become non-blocking
    //not implemented yet
    void interceptor_cb(Interceptor cb);

    //most common methods only so far
    void get(const std::string& end_point, ResultCb cb);
    void post(const std::string& end_point, const RequestDto::Body body, ResultCb cb, DataCb data_cb = nullptr);
    //remove instead of delete since delete is a reserved word
    void remove(const std::string& end_point, ResultCb cb = nullptr);
    void patch(const std::string& end_point, const RequestDto::Body body, ResultCb cb, DataCb data_cb = nullptr);

    void execute(RequestDto request, ResultCb cb = nullptr, DataCb data_cb = nullptr);

    bool process();//to be called from the user thread, will process callbacks for completed requests, should be called often.
    bool block(); //blocks the caller thread until all pending requests are completed, or HTTP_CLIENT_DEFAULT_TIMEOUT_MS is reached.
                    //return false when timeout reached.
    void cancel(CancelCb cb);

    ~HttpClient() = default;

private:
    HttpClient() = default;

private:
    std::shared_ptr<HttpClientIntf> m_impl;
};
 
 } // namespace orion::sync
