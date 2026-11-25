#pragma once

#include <cstdint>
#include <map>
#include <memory>

#include <common/src/coroutine.h>
#include <core/core_api.h>

namespace WebExtensions {

class WebExtensionsRuntimeImpl;

class RuntimeImpl : public v2::Runtime {
public:
    static std::string script_source_for_extension(WebExtension* extension);
    void set_profile_world(std::weak_ptr<orion::bridge::ProfileWorld> profile_world);
    void __sendMessageLocal(v2::Promise<rfl::Generic> promise, const rfl::Generic& message, const v2::SendMessageOptions* options) override;
    void __sendMessageExternal(v2::Promise<rfl::Generic> promise, const std::string& extension_id, const rfl::Generic& message, const v2::SendMessageOptions* options) override;
    void __sendMessageResponse(v2::Promise<void> promise, const std::string& request_id, const bool& responded, const rfl::Generic* response, const std::string* error) override;
    void __sendNativeMessage(v2::Promise<rfl::Generic> promise, const std::string& application, const rfl::Generic& message) override;
    void __portConnect(v2::Promise<void> promise, const std::string& socket, const v2::ConnectInfo* connect_info) override;
    void __portConnectExternal(v2::Promise<void> promise, const std::string& socket, const std::string& extension_id, const v2::ConnectInfo* connect_info) override;
    void __portConnectNative(v2::Promise<void> promise, const std::string& socket, const std::string& application) override;
    void __portReplayConnections(v2::Promise<std::vector<v2::RuntimeInternalMessageSender>> promise, const bool& external) override;
    void __portTakePendingMessages(v2::Promise<std::vector<rfl::Generic>> promise, const std::string& socket) override;
    void __portDisconnect(v2::Promise<void> promise, const std::string& socket) override;
    void __portPostMessage(v2::Promise<void> promise, const std::string& socket, const rfl::Generic& message) override;
    void setUninstallURL(v2::Promise<void> promise, const std::string* url) override;
    void reload(v2::Promise<void> promise) override;

private:
    detached_task reload_extension(
        std::weak_ptr<orion::bridge::ProfileWorld> profile_world,
        v2::Promise<void> promise);
    struct PendingMessage {
        v2::Promise<rfl::Generic> promise;
        size_t remaining_listeners;
        std::string target_extension_id;
    };
    struct PortConnection {
        std::string source_extension_id;
        std::string target_extension_id;
        std::weak_ptr<orion::bridge::WebViewBridge> source_webview;
        std::string source_script_world;
        v2::RuntimeInternalMessageSenderStorage sender;
        std::vector<rfl::Generic> pending_messages;
    };

    bool has_extension(const std::string& extension_id) const;
    v2::RuntimeInternalMessageSenderStorage sender_for(
        const std::string& extension_id,
        orion::bridge::WebViewBridge& webview) const;

    std::weak_ptr<orion::bridge::ProfileWorld> m_profile_world;
    uint64_t m_next_request_id = 1;
    std::map<std::string, PendingMessage> m_pending_messages;
    std::map<std::string, PortConnection> m_ports;
};

} // namespace WebExtensions
