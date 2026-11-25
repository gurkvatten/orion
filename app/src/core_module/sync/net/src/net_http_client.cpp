#include "net_http_client.h"
#include <src/internal/http_client_intf.h>

namespace orion::net {

std::string
headers_get(const Headers& headers, const std::string& name, const std::string& default_value)
{
    for (const auto& header : headers) {
        if (header.first == name) {
            return header.second;
        }
    }
    return default_value;
}

void
headers_set(Headers& headers, const std::string& name, const std::string& value)
{
    for (auto& header : headers) {
        if (header.first == name) {
            header.second = value;
            return;
        }
    }
    headers.emplace_back(name, value);
}

std::shared_ptr<HttpClient>
HttpClient::create(const Uri&& base_url, std::string session_cookie_names)
{
    auto impl = HttpClientIntf::create(std::forward<const Uri>(base_url), std::move(session_cookie_names));
    return std::make_shared<HttpClient>(impl);
}

std::string
HttpClient::base_url() const
{
    return m_impl->base_url();
}

void
HttpClient::interceptor_cb(Interceptor cb)
{
    m_impl->interceptor_cb(std::move(cb));
}

void
HttpClient::get(const std::string& end_point, ResultCb cb )
{
    m_impl->get(end_point, std::move(cb));
}

void
HttpClient::post(const std::string& end_point, const RequestDto::Body body, ResultCb cb, DataCb data_cb)
{
    m_impl->post(end_point, std::move(body), std::move(cb), std::move(data_cb));
}

//remove instead of delete since delete is a reserved word
void
HttpClient::remove(const std::string& end_point, ResultCb cb)
{
    m_impl->remove(end_point, std::move(cb));
}

void
HttpClient::patch(const std::string& end_point, const RequestDto::Body body, ResultCb cb, DataCb data_cb)
{
    m_impl->patch(end_point, std::move(body), std::move(cb), std::move(data_cb));
}

void
HttpClient::execute(const HttpClient::RequestDto request, HttpClient::ResultCb cb, HttpClient::DataCb data_cb)
{
    m_impl->execute(std::move(request), std::move(cb), data_cb ? std::move(data_cb) : data_cb);
}

bool
HttpClient::process()
{
    return m_impl->process();
}

bool
HttpClient::block()
{
    return m_impl->block();
}

void
HttpClient::cancel(CancelCb cb)
{
    m_impl->cancel(std::move(cb));
}

} // namespace orion::net