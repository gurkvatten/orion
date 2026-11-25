#include <core/webkit_bridge/API/bridge.h>

#include <cassert>
#include <utility>

namespace orion::bridge {

ClipboardBridge::ClipboardBridge() = default;

ClipboardBridge::~ClipboardBridge()
{
    m_host = nullptr;
    decltype(m_read_completions) completions;
    completions.swap(m_read_completions);
    for(auto& [request, completion] : completions) {
        completion(std::nullopt, "clipboard is unavailable");
    }
}

HostAttachment
ClipboardBridge::attach(ClipboardHost& host)
{
    assert(!m_host || m_host == &host);
    m_host = &host;
    std::weak_ptr<ClipboardBridge> weak_bridge = weak_from_this();
    return HostAttachment([weak_bridge, host = &host] {
        if(auto bridge = weak_bridge.lock()) bridge->detach(*host);
    });
}

void
ClipboardBridge::detach(ClipboardHost& host)
{
    if(m_host != &host) return;
    m_host = nullptr;
    decltype(m_read_completions) completions;
    completions.swap(m_read_completions);
    for(auto& [request, completion] : completions) {
        completion(std::nullopt, "clipboard is unavailable");
    }
}

std::optional<std::string>
ClipboardBridge::write_from_core(const std::string& text)
{
    return m_host
        ? m_host->clipboard_write_text(text)
        : std::optional<std::string>("clipboard is unavailable");
}

void
ClipboardBridge::read_from_core(ReadCompletion completion)
{
    if(!m_host) {
        completion(std::nullopt, "clipboard is unavailable");
        return;
    }
    const ReadRequest request = m_next_request++;
    m_read_completions.emplace(request, std::move(completion));
    m_host->clipboard_read_text(request);
}

void
ClipboardBridge::clipboard_read_text_completed(
    ReadRequest request, std::optional<std::string> text,
    std::optional<std::string> error)
{
    if(!m_host) return;
    auto it = m_read_completions.find(request);
    if(it == m_read_completions.end()) return;
    auto completion = std::move(it->second);
    m_read_completions.erase(it);
    completion(std::move(text), std::move(error));
}

} // namespace orion::bridge
