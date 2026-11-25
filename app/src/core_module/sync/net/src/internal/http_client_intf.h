#pragma once
#include <src/net_http_client.h> 
#include <../common/src/cross_thread_queue.h>
#include <memory>

namespace orion::net {

//this could've been std::pair but maybe like that it's more rfeadable?
struct DoneRequest {
    HttpClient::ResultCb cb = nullptr;
    HttpClient::ResponseDto response;
};

class HttpClientIntf {

public:
    static std::shared_ptr<HttpClientIntf> create(const Uri&& base_url, std::string session_cookie_names = "");
    std::string base_url() const { return m_base_url.uri(); }

    virtual ~HttpClientIntf() = default;
    virtual void interceptor_cb(HttpClient::Interceptor interceptor) {m_interceptor = std::move(interceptor);} 

    virtual void get(const std::string& end_point, HttpClient::ResultCb cb = nullptr) = 0;
    virtual void post(const std::string& end_point,  const HttpClient::RequestDto::Body&& body, HttpClient::ResultCb cb = nullptr, HttpClient::DataCb data_cb = nullptr) = 0;
    //remove instead of delete since delete is a reserved word
    virtual void remove(const std::string& end_point, HttpClient::ResultCb cb = nullptr) = 0;
    virtual void patch(const std::string& end_point,  const HttpClient::RequestDto::Body&& body, HttpClient::ResultCb cb = nullptr, HttpClient::DataCb data_cb = nullptr) = 0;

    virtual void execute(HttpClient::RequestDto request, HttpClient::ResultCb cb = nullptr, HttpClient::DataCb data_cb = nullptr) = 0;

    virtual bool process();
    virtual bool block();

    virtual void cancel(HttpClient::CancelCb cb)
    {
        m_cancelled_cbs.emplace_back(std::move(cb));
        try_dispatch_cancel_cb();
    }

protected:
    HttpClientIntf(const Uri&& base_url, std::string session_cookie_names = "")
        : m_base_url(base_url), m_session_cookie_names(std::move(session_cookie_names)) {};

    int empty_result_queue(DoneRequest& item);
    virtual void try_dispatch_cancel_cb();
    

protected:
    Uri m_base_url;
    std::string m_session_cookie_names;
    HttpClient::Interceptor m_interceptor = nullptr;
    CrossThreadQueue<DoneRequest> m_result_queue;
    std::vector<std::function<void()>> m_cancelled_cbs{};
};

} // namespace orion::net