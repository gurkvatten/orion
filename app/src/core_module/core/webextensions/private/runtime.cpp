#include <common/src/uri.h>
#include <core/webextensions/extension.h>
#include <core/webextensions/private/runtime.h>
#include <core/webextensions/runtime_impl.h>

#include <algorithm>

namespace WebExtensions {

void
RuntimeImpl::set_profile_world(
    std::weak_ptr<orion::bridge::ProfileWorld> profile_world)
{
    m_profile_world = profile_world;
}

void
RuntimeImpl::setUninstallURL(v2::Promise<void> promise, const std::string*)
{
    // Orion does not currently run an extension-provided URL on uninstall,
    // but accepting the registration preserves the WebExtension API contract.
    promise.resolve();
}

void
RuntimeImpl::reload(v2::Promise<void> promise)
{
    reload_extension(m_profile_world, std::move(promise));
}

detached_task
RuntimeImpl::reload_extension(
    std::weak_ptr<orion::bridge::ProfileWorld> profile_world,
    v2::Promise<void> promise)
{
    auto world = profile_world.lock();
    if(!world) co_return;
    auto* api = &world->core_api();

    const orion::ExtensionId extension_id(promise.extension_id());
    world.reset();
    const auto clear_error = co_await async(invoke(
        api,
        &orion::CoreApi::extension_session_storage_clear), extension_id);
    world = profile_world.lock();
    auto* runtime = world
        ? static_cast<WebExtensionsRuntimeImpl*>(world->runtime())
        : nullptr;
    if(!runtime) co_return;
    if (clear_error) {
        promise.reject(clear_error.message());
        co_return;
    }

    const auto reload_error = co_await runtime->reload_extension(extension_id);
    if (reload_error) {
        promise.reject(reload_error.message());
        co_return;
    }
    promise.resolve();
}

std::string
RuntimeImpl::script_source_for_extension(WebExtension* extension)
{
    std::optional<WebExtensionResourceData> manifest_data = extension->resources().data_for_path("manifest.json");
    std::string manifest = "{}";
    if(manifest_data) {
        manifest = std::string((const char*)manifest_data->data, manifest_data->datalen);
    }
    const std::string id = extension->extension_id().value;
    const std::string scheme = extension->resources().scheme();
    std::stringstream ss;
    ss << R"v2runtime(
(function(){
    const implementations = window.__webextensions_v2_implementations__ =
        window.__webextensions_v2_implementations__ || {};

    function consume(operation) {
        if(operation && typeof operation.then === "function") {
            operation.catch((error) => console.error("WebExtensions v2: Runtime operation failed", error));
        }
    }

    function publicSender(sender) {
        if(!sender) return undefined;
        return {id: sender.id, url: sender.url, origin: sender.origin};
    }

    function wrapPortListener(port, target, disconnect, afterDispatch = undefined) {
        const entries = [];
        const wrapped = (sender, message) => {
            if(!sender || sender.socket !== port.__socket) return;
            const arguments = disconnect ? [port] : [message, port];
            try {
                for(const entry of entries.slice()) {
                    entry.callback(...arguments);
                }
            } finally {
                if(afterDispatch !== undefined) afterDispatch();
            }
        };

        // Register before runtime.connect is sent to native code. JavaScript
        // event-loop ordering gives the caller a chance to attach public Port
        // listeners before a peer response can be dispatched.
        target.addListener(wrapped);

        const event = {
            addListener(callback) {
                if(entries.some((entry) => entry.callback === callback)) return;
                entries.push({callback});
            },
            removeListener(callback) {
                const index = entries.findIndex((entry) => entry.callback === callback);
                if(index === -1) return;
                entries.splice(index, 1);
            },
            hasListener(callback) {
                return entries.some((entry) => entry.callback === callback);
            },
            hasListeners() {
                return entries.length !== 0;
            }
        };
        return {
            event,
            dispatch: wrapped,
            dispose() { target.removeListener(wrapped); }
        };
    }

    function createPort(context, socket = crypto.randomUUID(), name = "", sender = undefined) {
        const port = {
            __socket: socket,
            name,
            sender,
            disconnect() {
                consume(context.invokeNative("__portDisconnect", {socket}));
            },
            postMessage(message) {
                consume(context.invokeNative("__portPostMessage", {socket, message}));
            }
        };
        let disconnectListener;
        let messageListener;
        const dispose = () => {
            disconnectListener.dispose();
            messageListener.dispose();
        };
        disconnectListener = wrapPortListener(
            port, context.internal.__portOnDisconnect, true, dispose);
        messageListener = wrapPortListener(
            port, context.internal.__portOnMessage, false);
        port.onDisconnect = disconnectListener.event;
        port.onMessage = messageListener.event;
        if(sender !== undefined) {
            consume(context.invokeNative("__portTakePendingMessages", {socket}).then((messages) => {
                if(!Array.isArray(messages)) return;
                for(const message of messages) {
                    messageListener.dispatch({socket}, message);
                }
            }));
        }
        return port;
    }

    function wrapConnectEvent(context, target, replayExternal = undefined) {
        const entries = [];
        const connectedSockets = new Set();
        let replayStarted = false;
        const wrapped = (sender) => {
            if(!sender || entries.length === 0 || connectedSockets.has(sender.socket)) return;
            const port = createPort(
                context,
                sender.socket,
                sender.name || "",
                publicSender(sender));
            connectedSockets.add(sender.socket);
            for(const entry of entries.slice()) {
                entry.callback(port);
            }
        };

        // Native state is the sole startup buffer. If a foreground page
        // connects before the emulated service worker installs onConnect,
        // replay the still-live native ports when its first listener appears.
        target.addListener(wrapped);
        context.internal.__portOnDisconnect.addListener((sender) => {
            if(!sender) return;
            connectedSockets.delete(sender.socket);
        });

        return {
            addListener(callback) {
                if(entries.some((entry) => entry.callback === callback)) return;
                entries.push({callback});
                if(!replayStarted && replayExternal !== undefined) {
                    replayStarted = true;
                    consume(context.invokeNative("__portReplayConnections", {
                        external: replayExternal
                    }).then((senders) => {
                        if(entries.length === 0) {
                            replayStarted = false;
                            return;
                        }
                        if(!Array.isArray(senders)) return;
                        for(const sender of senders) wrapped(sender);
                    }));
                }
            },
            removeListener(callback) {
                const index = entries.findIndex((entry) => entry.callback === callback);
                if(index === -1) return;
                entries.splice(index, 1);
            },
            hasListener(callback) {
                return entries.some((entry) => entry.callback === callback);
            },
            hasListeners() {
                return entries.length !== 0;
            }
        };
    }

    function wrapMessageEvent(context, target) {
        const entries = [];
        return {
            addListener(callback) {
                if(entries.some((entry) => entry.callback === callback)) return;
                const wrapped = (message, sender) => {
                    let completed = false;
                    const complete = (responded, response, error) => {
                        if(completed) return;
                        completed = true;
                        const args = {requestId: sender.requestId, responded};
                        if(response !== undefined) args.response = response;
                        if(error) args.error = error;
                        consume(context.invokeNative("__sendMessageResponse", args));
                    };
                    const sendResponse = (response) => complete(true, response);
                    let result;
                    try {
                        result = callback(message, publicSender(sender), sendResponse);
                    } catch(error) {
                        complete(false, undefined, error && error.message
                            ? error.message : String(error));
                        return;
                    }
                    if(completed) return;
                    if(result && typeof result.then === "function") {
                        result.then(
                            (response) => complete(true, response),
                            (error) => complete(false, undefined, error && error.message
                                ? error.message : String(error)));
                    } else if(result !== true) {
                        complete(false);
                    }
                };
                entries.push({callback, wrapped});
                target.addListener(wrapped);
            },
            removeListener(callback) {
                const index = entries.findIndex((entry) => entry.callback === callback);
                if(index === -1) return;
                target.removeListener(entries[index].wrapped);
                entries.splice(index, 1);
            },
            hasListener(callback) {
                return entries.some((entry) => entry.callback === callback);
            },
            hasListeners() {
                return entries.length !== 0;
            }
        };
    }

    const emptyEvent = {
        addListener() {},
        removeListener() {},
        hasListener() { return false; },
        hasListeners() { return false; }
    };

    implementations["Runtime.id"] = () => ")v2runtime" << id << R"v2runtime(";
    implementations["Runtime.lastError"] = () =>
        window.__bridge_v2__ ? window.__bridge_v2__.lastError : null;
    implementations["Runtime.getManifest"] = () => ()v2runtime" << manifest << R"v2runtime();
    implementations["Runtime.getVersion"] = () => {
        const manifest = implementations["Runtime.getManifest"]();
        return manifest && typeof manifest.version === "string" ? manifest.version : "";
    };
    implementations["Runtime.getURL"] = (context) =>
        ")v2runtime" << scheme << R"v2runtime(://)v2runtime" << id << R"v2runtime(/" + context.args.path;
    implementations["Runtime.getFrameId"] = () => 0;

    implementations["Runtime.connect"] = (context) => {
        const socket = crypto.randomUUID();
        const connectInfo = context.args.connectInfo || {};
        const port = createPort(context, socket, connectInfo.name || "");
        if(context.args.extensionId === undefined) {
            consume(context.invokeNative("__portConnect", {socket, connectInfo}));
        } else {
            consume(context.invokeNative("__portConnectExternal", {
                socket,
                extensionId: context.args.extensionId,
                connectInfo
            }));
        }
        return port;
    };

    implementations["Runtime.connectNative"] = (context) => {
        const socket = crypto.randomUUID();
        const port = createPort(context, socket);
        consume(context.invokeNative("__portConnectNative", {
            socket,
            application: context.args.application
        }));
        return port;
    };

    implementations["Runtime.sendMessage"] = (context) => {
        if(context.args.extensionId === undefined) {
            return context.invokeNative("__sendMessageLocal", {
                message: context.args.message,
                options: context.args.options || {}
            });
        }
        return context.invokeNative("__sendMessageExternal", {
            extensionId: context.args.extensionId,
            message: context.args.message,
            options: context.args.options || {}
        });
    };

    implementations["Runtime.sendNativeMessage"] = (context) =>
        context.invokeNative("__sendNativeMessage", {
            application: context.args.application,
            message: context.args.message
        });

    implementations["Runtime.requestUpdateCheck"] = (context) =>
        context.invokeNative("requestUpdateCheck").then((result) => [
            result.status,
            result.version === undefined ? undefined : {version: result.version}
        ]);

    implementations["Runtime.events"] = (context) => {
        switch(context.id) {
        case "onConnect":
            return wrapConnectEvent(context, context.internal.__onConnect, false);
        case "onConnectExternal":
            return wrapConnectEvent(context, context.internal.__onConnectExternal, true);
        case "onUserScriptConnect":
            return wrapConnectEvent(context, context.internal.__onUserScriptConnect);
        case "onMessage":
            return wrapMessageEvent(context, context.internal.__onMessage);
        case "onMessageExternal":
            return wrapMessageEvent(context, context.internal.__onMessageExternal);
        case "onUserScriptMessage":
            return wrapMessageEvent(context, context.internal.__onUserScriptMessage);
        default:
            return emptyEvent;
        }
    };
})();
)v2runtime";
    return ss.str();
}

void
RuntimeImpl::__sendMessageLocal(
    v2::Promise<rfl::Generic> promise, const rfl::Generic& message,
    const v2::SendMessageOptions* options)
{
    const std::string extension_id = promise.extension_id();
    __sendMessageExternal(promise, extension_id, message, options);
}

void
RuntimeImpl::__sendMessageExternal(
    v2::Promise<rfl::Generic> promise, const std::string& extension_id,
    const rfl::Generic& message, const v2::SendMessageOptions*)
{
    const std::string source_extension_id = promise.extension_id();
    const std::string target = extension_id.empty() ? source_extension_id : extension_id;
    if(!has_extension(target)) {
        promise.reject("No extension with id " + target);
        return;
    }

    auto webview = promise.webview().lock();
    if(!webview) {
        promise.reject("Web view is unavailable");
        return;
    }
    v2::RuntimeInternalMessageSenderStorage sender = sender_for(source_extension_id, *webview);
    sender.requestId = source_extension_id + ":" + std::to_string(m_next_request_id++);
    auto is_not_source_context = [source = webview, source_world = promise.script_world()](
                                     auto& candidate, const auto& registration) {
        return &candidate != source.get() || registration.script_world != source_world;
    };
    auto& listeners = target == source_extension_id ? __onMessage : __onMessageExternal;
    const size_t listener_count = listeners.count_extension_if(target, is_not_source_context);
    if(listener_count == 0) {
        promise.resolve();
        return;
    }

    m_pending_messages.emplace(sender.requestId.value(), PendingMessage {promise, listener_count, target});
    const v2::RuntimeInternalMessageSender sender_view = v2::from_storage(sender);
    listeners.notify_extension_if(target, is_not_source_context, message, sender_view);
}

void
RuntimeImpl::__sendMessageResponse(
    v2::Promise<void> promise, const std::string& request_id, const bool& responded,
    const rfl::Generic* response, const std::string* error)
{
    if(request_id.empty()) {
        promise.reject("Missing runtime message request id");
        return;
    }

    auto it = m_pending_messages.find(request_id);
    if(it == m_pending_messages.end()) {
        promise.resolve();
        return;
    }

    PendingMessage& pending = it->second;
    if(promise.extension_id() != pending.target_extension_id) {
        promise.reject("Runtime message response came from the wrong extension");
        return;
    }
    if(error && !error->empty()) {
        pending.promise.reject(*error);
        m_pending_messages.erase(it);
    } else if(responded) {
        if(response) {
            pending.promise.resolve(*response);
        } else {
            pending.promise.resolve();
        }
        m_pending_messages.erase(it);
    } else if(--pending.remaining_listeners == 0) {
        pending.promise.resolve();
        m_pending_messages.erase(it);
    }
    promise.resolve();
}

void
RuntimeImpl::__sendNativeMessage(
    v2::Promise<rfl::Generic> promise, const std::string& application,
    const rfl::Generic& message)
{
    // Safari builds of Bitwarden route clipboard access through their native
    // app. Provide only that narrow protocol locally; arbitrary native hosts
    // remain forbidden.
    if(application != "com.bitwarden.desktop") {
        promise.reject("Access to the specified native messaging host is forbidden");
        return;
    }
    auto profile_world = m_profile_world.lock();
    if(!profile_world) {
        promise.reject("Clipboard bridge is unavailable");
        return;
    }
    const auto* object = std::get_if<rfl::Generic::Object>(&message.get());
    if(!object) {
        promise.reject("Invalid native clipboard message");
        return;
    }
    const auto command_it = std::find_if(
        object->begin(), object->end(),
        [](const auto& property) { return property.first == "command"; });
    if(command_it == object->end()) {
        promise.reject("Invalid native clipboard message");
        return;
    }
    const auto* command = std::get_if<std::string>(&command_it->second.get());
    if(!command) {
        promise.reject("Invalid native clipboard command");
        return;
    }
    if(*command == "copyToClipboard") {
        const auto data_it = std::find_if(
            object->begin(), object->end(),
            [](const auto& property) { return property.first == "data"; });
        const auto* text = data_it == object->end()
            ? nullptr
            : std::get_if<std::string>(&data_it->second.get());
        if(!text) {
            promise.reject("Invalid clipboard text");
            return;
        }
        auto bridge = profile_world->clipboard_bridge().lock();
        const auto error = bridge
            ? bridge->write_from_core(*text)
            : std::optional<std::string>("clipboard is unavailable");
        if(error) {
            promise.reject(*error);
        } else {
            promise.resolve();
        }
        return;
    }
    if(*command == "readFromClipboard") {
        auto bridge = profile_world->clipboard_bridge().lock();
        if(!bridge) {
            promise.reject("clipboard is unavailable");
        } else {
            bridge->read_from_core(
                [promise](std::optional<std::string> text, std::optional<std::string> error) mutable {
                    if(error) {
                        promise.reject(*error);
                    } else {
                        promise.resolve(rfl::Generic(text.value_or("")));
                    }
                });
        }
        return;
    }
    promise.reject("Access to the specified native messaging command is forbidden");
}

void
RuntimeImpl::__portConnect(
    v2::Promise<void> promise, const std::string& socket, const v2::ConnectInfo* connect_info)
{
    const std::string extension_id = promise.extension_id();
    __portConnectExternal(promise, socket, extension_id, connect_info);
}

void
RuntimeImpl::__portConnectExternal(
    v2::Promise<void> promise, const std::string& socket, const std::string& extension_id,
    const v2::ConnectInfo* connect_info)
{
    if(socket.empty()) {
        promise.reject("runtime.connect requires a socket id");
        return;
    }
    const std::string source_extension_id = promise.extension_id();
    const std::string target = extension_id.empty() ? source_extension_id : extension_id;
    if(!has_extension(target)) {
        promise.reject("No extension with id " + target);
        return;
    }
    if(m_ports.contains(socket)) {
        promise.reject("Port socket is already connected");
        return;
    }

    auto webview = promise.webview().lock();
    if(!webview) {
        promise.reject("Web view is unavailable");
        return;
    }
    v2::RuntimeInternalMessageSenderStorage sender = sender_for(source_extension_id, *webview);
    sender.socket = socket;
    sender.name = connect_info && connect_info->name ? *connect_info->name : "";
    auto [port_it, inserted] = m_ports.emplace(
        socket,
        PortConnection {
            source_extension_id,
            target,
            webview->weak_from_this(),
            promise.script_world(),
            std::move(sender),
            {},
        });
    if(!inserted) {
        promise.reject("Port socket is already connected");
        return;
    }

    const v2::RuntimeInternalMessageSender sender_view = v2::from_storage(port_it->second.sender);
    auto is_not_source_context = [source = webview, source_world = promise.script_world()](
                                     auto& candidate, const auto& registration) {
        return &candidate != source.get() || registration.script_world != source_world;
    };
    if(target == source_extension_id) {
        __onConnect.notify_extension_if(target, is_not_source_context, sender_view);
    } else {
        __onConnectExternal.notify_extension_if(target, is_not_source_context, sender_view);
    }
    promise.resolve();
}

void
RuntimeImpl::__portConnectNative(
    v2::Promise<void> promise, const std::string&, const std::string&)
{
    promise.reject("runtime.connectNative is not implemented");
}

void
RuntimeImpl::__portReplayConnections(
    v2::Promise<std::vector<v2::RuntimeInternalMessageSender>> promise, const bool& external)
{
    const std::string& extension_id = promise.extension_id();
    auto webview = promise.webview().lock();
    if(!webview) {
        promise.resolve({});
        return;
    }
    std::vector<v2::RuntimeInternalMessageSender> connections;
    for(const auto& [socket, connection] : m_ports) {
        const bool is_external = connection.source_extension_id != connection.target_extension_id;
        if(connection.target_extension_id != extension_id
            || (connection.source_webview.lock() == webview
                && connection.source_script_world == promise.script_world())
            || is_external != external) {
            continue;
        }
        connections.push_back(v2::from_storage(connection.sender));
    }
    promise.resolve(connections);
}

void
RuntimeImpl::__portTakePendingMessages(
    v2::Promise<std::vector<rfl::Generic>> promise, const std::string& socket)
{
    auto it = m_ports.find(socket);
    if(it == m_ports.end()) {
        promise.resolve({});
        return;
    }

    PortConnection& connection = it->second;
    auto webview = promise.webview().lock();
    if(!webview) {
        promise.reject("Web view is unavailable");
        return;
    }
    if(connection.target_extension_id != promise.extension_id()
        || (connection.source_webview.lock() == webview
            && connection.source_script_world == promise.script_world())) {
        promise.reject("Port does not belong to this receiving context");
        return;
    }

    std::vector<rfl::Generic> messages = std::move(connection.pending_messages);
    connection.pending_messages.clear();
    promise.resolve(messages);
}

void
RuntimeImpl::__portDisconnect(v2::Promise<void> promise, const std::string& socket)
{
    auto it = m_ports.find(socket);
    if(it == m_ports.end()) {
        promise.resolve();
        return;
    }

    const PortConnection connection = it->second;
    const std::string& extension_id = promise.extension_id();
    if(extension_id != connection.source_extension_id && extension_id != connection.target_extension_id) {
        promise.reject("Port does not belong to this extension");
        return;
    }
    m_ports.erase(it);

    v2::RuntimeInternalMessageSenderStorage sender;
    sender.socket = socket;
    const v2::RuntimeInternalMessageSender sender_view = v2::from_storage(sender);
    __portOnDisconnect.notify_extension(connection.source_extension_id, sender_view);
    if(connection.target_extension_id != connection.source_extension_id) {
        __portOnDisconnect.notify_extension(connection.target_extension_id, sender_view);
    }
    promise.resolve();
}

void
RuntimeImpl::__portPostMessage(
    v2::Promise<void> promise, const std::string& socket, const rfl::Generic& message)
{
    auto it = m_ports.find(socket);
    if(it == m_ports.end()) {
        promise.reject("Attempting to use a disconnected port object");
        return;
    }
    PortConnection& connection = it->second;
    const std::string& extension_id = promise.extension_id();
    if(extension_id != connection.source_extension_id && extension_id != connection.target_extension_id) {
        promise.reject("Port does not belong to this extension");
        return;
    }

    v2::RuntimeInternalMessageSenderStorage sender;
    sender.socket = socket;
    const v2::RuntimeInternalMessageSender sender_view = v2::from_storage(sender);
    auto webview = promise.webview().lock();
    if(!webview) {
        promise.reject("Web view is unavailable");
        return;
    }
    auto is_not_source_context = [source = webview, source_world = promise.script_world()](
                                     auto& candidate, const auto& registration) {
        return &candidate != source.get() || registration.script_world != source_world;
    };
    size_t notified = __portOnMessage.notify_extension_if(
        connection.source_extension_id, is_not_source_context, sender_view, message);
    if(connection.target_extension_id != connection.source_extension_id) {
        notified += __portOnMessage.notify_extension_if(
            connection.target_extension_id, is_not_source_context, sender_view, message);
    }
    if(notified == 0) {
        connection.pending_messages.push_back(message);
    }
    promise.resolve();
}

bool
RuntimeImpl::has_extension(const std::string& extension_id) const
{
    auto profile_world = m_profile_world.lock();
    if(!profile_world) return false;
    auto* runtime = profile_world->runtime();
    if(!runtime) return false;
    for(const auto& weak_extension : runtime->get_extensions().value_or(WebExtensionList())) {
        auto extension = weak_extension.lock();
        if(extension && extension->extension_id().value == extension_id) {
            return true;
        }
    }
    return false;
}

v2::RuntimeInternalMessageSenderStorage
RuntimeImpl::sender_for(
    const std::string& extension_id,
    orion::bridge::WebViewBridge& webview_bridge) const
{
    v2::RuntimeInternalMessageSenderStorage sender;
    sender.id = extension_id;
    if(!m_profile_world.expired()) {
        sender.url = webview_bridge.web_view_uri();
        const orion::Uri uri = orion::Uri::parse_strict(sender.url->c_str());
        if(!uri.scheme().empty() && !uri.authority().empty()) {
            sender.origin = uri.scheme() + "://" + uri.authority();
        }
    }
    return sender;
}

} // namespace WebExtensions
