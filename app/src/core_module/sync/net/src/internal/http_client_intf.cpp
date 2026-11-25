#include "http_client_intf.h"

namespace orion::net {

int HttpClientIntf::empty_result_queue(DoneRequest& item) {
    int count = 0;
    if (item.cb) {
        item.cb(std::move(item.response));
        ++count;
    }
    while (!m_result_queue.empty()) {
        DoneRequest item = m_result_queue.pop();
        if (item.cb) {
            item.cb(std::move(item.response));
            ++count;
        } else {
            break;
        }
    }
    return count;
};

bool
HttpClientIntf::process()
{
    DoneRequest item =
        m_result_queue.wait_for_and_pop(std::chrono::milliseconds(200));
    if (item.cb) {
        item.cb(std::move(item.response));
    }
    return true;
}

bool
HttpClientIntf::block()
{
    DoneRequest item =
        m_result_queue.wait_for_and_pop(std::chrono::milliseconds(HTTP_CLIENT_DEFAULT_TIMEOUT_MS));
    return empty_result_queue(item) > 0;
}

void
HttpClientIntf::try_dispatch_cancel_cb()
{
    for (auto& cb : m_cancelled_cbs) {
        if (cb) {
            //TODO here we cannot use any app run loop predefined threads(as the ones accessible via CorApi)
            // so not sure what to dispatch the cb on?
            cb();
        }
    }
    m_cancelled_cbs.clear();
}

}// namespace orion::net