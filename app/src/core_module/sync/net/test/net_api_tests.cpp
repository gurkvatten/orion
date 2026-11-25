#include <src/net_http_client.h>
#include <src/file_streamer.h>
#include <../common/src/log.h>
#include <../common/src/defer.h>
#include <../common/src/string_util.h>
#include <cassert>
#include <fstream>
//#include <iostream>
#include <errno.h>
#include <limits.h>
//#include <format>
#include <string.h> //strerror



namespace orion::net::tests {

static
HttpClient::ResultCb cb = [](HttpClient::ResponseDto response) {
    if (response.error) {
        orion::log_error("Request failed with error: %s", response.error.message().c_str());
        return;
    }
    assert(response.status_code == 200);
    switch (response.state) {
    case HttpClient::ResponseState::headers:
        orion::log_debug("Received headers");
        for (const auto& header : response.headers) {
            orion::log_debug("header: %s : %s", header.first.c_str(), header.second.c_str());
        }
        orion::log_debug("Content length: %s", response.body.total_content_length != HTTP_CONTENT_LENGTH_UNDEFINED ? std::to_string(response.body.total_content_length).c_str() : "undefined");
        break;
    case HttpClient::ResponseState::data:
        orion::log_debug("Received data chunk, length: %zu\n content: %s", response.body.content.size(), response.body.content.c_str());
        break;
    case HttpClient::ResponseState::done:
        orion::log_debug("Received response body, total content length from headers: %zu\n content: %s", response.body.total_content_length, response.body.content.c_str());
        break;        
    default:
        assert(false && "unexpected response state");
    }
};
static
HttpClient::Interceptor post_interceptor = [](HttpClient::RequestDto& request) {
    assert(request.raw_url == "http://localhost:12345/file_transfer");
    headers_set(request.headers, "User-Agent", "orion v0.0.0.0.1");
    headers_set(request.headers, "accept", "*/*");
};
static
HttpClient::Interceptor get_interceptor = [](HttpClient::RequestDto& request) {
    assert(request.raw_url == "http://localhost:12345/hello");
    headers_set(request.headers, "user-agent", "orion v0.0.0.0.1");
    headers_set(request.headers, "accept", "*/*");
};

void
get_test()
{
    std::shared_ptr<HttpClient> client = HttpClient::create(Uri::parse_strict("http://localhost:12345"));
    client->interceptor_cb(get_interceptor);
    client->remove("/hello", cb);
    client->get("/hello", cb);
    while (client->process());
}

static FileUploader file_streamer("/home/mikin/mobiTelco/sync/bookmark.osnc", "text/plain");

static std::shared_ptr<HttpClient>
start_post_test()
{
    std::shared_ptr<HttpClient> client = HttpClient::create(Uri::parse_strict("http://localhost:12345"));
    client->interceptor_cb(post_interceptor);

    if (!file_streamer.start(client, {
            .method = METHOD_PUT.data(),
            .raw_url = "/file_transfer",
            .body = {},
            .headers = {}
        }, std::move(cb))) {
        
        return client;
    }
    assert(false && "Failed to start file streamer");
    return nullptr;
}

void
post_test()
{
    std::shared_ptr<HttpClient> client = start_post_test();
    if (!client) {
        return;
    }
    while (client->process());
}

void
cancel_test()
{
    std::shared_ptr<HttpClient> client = start_post_test();
    if (!client) {
        return;
    }
    int cancel_dispatched = 2;
    while (client->process()) {
        if (!--cancel_dispatched) {
            client->cancel([]() {
                orion::log_debug("Request was cancelled");
            });
            cancel_dispatched = INT_MAX;
        }
    }
}

} // namespace orion::net

