#define ENABLE_VERBOSE_LOGS 0

#include <core/core_api.h>
#include <common/src/coroutine.h>
#include <common/src/dispatch.h>
#include <common/src/log.h>
#include <common/src/uri.h>
#include <common/src/memory.h>
#include <core/webextensions/runtime.h>
#include <core/webextensions/extension.h>
#include <core/webextensions/repository.h>
#include <core/webextensions/runtime_impl.h>
#include <core/webextensions/web_view.h>
#include <core/webkit_bridge/API/bridge.h>
#include <rfl/json.hpp>

#include <algorithm>
#include <cassert>
#include <cctype>

using namespace orion;

namespace WebExtensions {

struct NativeCallMessage {
    std::optional<int> version;
    rfl::Rename<"class", std::string> clazz;
    std::string function;
    std::string arguments;
    int promise = 0;
};

struct NativeEventMessage {
    std::optional<int> version;
    rfl::Rename<"class", std::string> clazz;
    std::string event;
    std::string command;
    std::optional<std::string> arguments;
    int listener = 0;
};

namespace {

std::string
content_script_world(const orion::ExtensionId& extension_id)
{
    return "WebExtensionContentScript:" + extension_id.value;
}

std::string
content_script_handler_name(const char* prefix, const orion::ExtensionId& extension_id)
{
    std::string result = prefix;
    result += '_';
    for(unsigned char character : extension_id.value) {
        result += std::isalnum(character) ? static_cast<char>(character) : '_';
    }
    return result;
}

const char*
security_profile_name(v2::SecurityProfile security_profile)
{
    switch(security_profile) {
        case v2::SecurityProfile::BackgroundScript: return "BackgroundScript";
        case v2::SecurityProfile::ExtensionPage: return "ExtensionPage";
        case v2::SecurityProfile::ContentScript: return "ContentScript";
    }
    return "ExtensionPage";
}

std::string
profile_handler_name(const char* prefix, v2::SecurityProfile security_profile)
{
    return std::string(prefix) + security_profile_name(security_profile);
}

std::vector<std::string>
content_script_match_patterns(const std::vector<std::string>& matches)
{
    std::vector<std::string> result;
    for(const auto& match : matches) {
        if(match == "<all_urls>") {
            result.push_back("*://*/*");
            result.push_back("file://*/*");
            result.push_back("ftp://*/*");
        } else {
            result.push_back(match);
        }
    }
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

std::optional<std::string>
content_script_resource(const WebExtension& extension, const std::string& path)
{
    auto resource = extension.resources().data_for_path(path);
    if(!resource) {
        orion::log_warning(
            "WebExtensions: content script resource not found for %s: %s",
            extension.extension_id().value.c_str(), path.c_str());
        return std::nullopt;
    }
    return std::string(reinterpret_cast<const char*>(resource->data), resource->datalen);
}

std::string
content_script_source_uri(const WebExtension& extension, const std::string& path)
{
    std::string normalized_path = path;
    while(!normalized_path.empty() && normalized_path.front() == '/') {
        normalized_path.erase(normalized_path.begin());
    }
    return extension.resources().scheme() + "://" + extension.extension_id().value + "/" + normalized_path;
}

std::string
javascript_content_script_source(
    const WebExtension& extension, const std::string& path, const std::string& source)
{
    return source + "\n//# sourceURL=" + content_script_source_uri(extension, path) + "\n";
}

std::string
css_content_script_source(
    const WebExtension& extension, const std::string& path, const std::string& source)
{
    return "(function(){const style=document.createElement(\"style\");"
        "style.textContent=" + rfl::json::write(source) + ";"
        "const target=document.head||document.documentElement;"
        "if(target)target.appendChild(style);})();\n//# sourceURL="
        + content_script_source_uri(extension, path) + "\n";
}

bool
uses_main_world(const WebExtensionContentScript& content_script)
{
    return content_script.world.value_or("ISOLATED") == "MAIN";
}

orion::WebViewScriptInjectionTime
content_script_injection_time(const WebExtensionContentScript& content_script)
{
    return content_script.run_at.value_or("document_idle") == "document_start"
        ? orion::WebViewScriptInjectionTime::document_start
        : orion::WebViewScriptInjectionTime::document_end;
}

orion::WebViewScriptFrames
content_script_frames(const WebExtensionContentScript& content_script)
{
    return content_script.all_frames
        ? orion::WebViewScriptFrames::all_frames
        : orion::WebViewScriptFrames::top_frame;
}

std::string
content_script_bridge_configuration(
    const std::string& native_call_handler, const std::string& native_event_handler)
{
    return "window.__webextensions_native_call_handler__="
        + rfl::json::write(native_call_handler)
        + ";window.__webextensions_native_event_handler__="
        + rfl::json::write(native_event_handler) + ";";
}

} // namespace

class NativeBridgeContext {
public:
    NativeBridgeContext(
        orion::bridge::WebViewBridge& web_view_bridge,
        const WebExtensionList& extensions,
        std::weak_ptr<orion::bridge::ProfileWorld> profile_world,
        std::string script_world = {},
        std::optional<orion::ExtensionId> extension_id = std::nullopt,
        v2::SecurityProfile security_profile = v2::SecurityProfile::ExtensionPage);
public:

    void on_native_call(const std::string& message_json);
    void on_native_event(const std::string& message_json);

protected:
    void handle_error(const std::string& error);

    std::optional<orion::ExtensionId> current_extension(
        orion::bridge::WebViewBridge&);
    std::vector<std::string> permissions_for_extension(
        const orion::ExtensionId& extension_id) const;

private:
    std::weak_ptr<orion::bridge::WebViewBridge> m_web_view_bridge;
    std::optional<WebExtensionList> m_extensions;
    std::weak_ptr<orion::bridge::ProfileWorld> m_profile_world;
    std::string m_script_world;
    std::optional<orion::ExtensionId> m_extension_id;
    v2::SecurityProfile m_security_profile;
};

NativeBridgeContext::NativeBridgeContext(
    orion::bridge::WebViewBridge& web_view_bridge,
    const WebExtensionList& extensions,
    std::weak_ptr<orion::bridge::ProfileWorld> profile_world,
    std::string script_world,
    std::optional<orion::ExtensionId> extension_id,
    v2::SecurityProfile security_profile)
    : m_web_view_bridge(web_view_bridge.weak_from_this())
    , m_extensions(extensions)
    , m_profile_world(profile_world)
    , m_script_world(std::move(script_world))
    , m_extension_id(std::move(extension_id))
    , m_security_profile(security_profile)
{
}

void
NativeBridgeContext::handle_error(const std::string& error)
{
    orion::log_warning("WebExtensions: %s", error.c_str());
}

void
NativeBridgeContext::on_native_call(const std::string& message_json)
{
    auto profile_world = m_profile_world.lock();
    auto web_view_bridge = m_web_view_bridge.lock();
    auto* runtime = profile_world
        ? static_cast<WebExtensionsRuntimeImpl*>(profile_world->runtime())
        : nullptr;
    if(!runtime || !web_view_bridge) return;
    std::optional<orion::ExtensionId> extension_id =
        current_extension(*web_view_bridge);
    if(!extension_id) {
        handle_error("inaccessible context");
        return;
    }
    std::string error;
    auto parsed = WebExtensions::deserialize_json<NativeCallMessage>(message_json, &error);
    if(!parsed) {
#if defined(_DEBUG) && _DEBUG
        orion::log_debug("WebExtensions: received JSON: %s", message_json.c_str());
#endif
        handle_error("while parsing message: " + error);
        return;
    }
    const auto& payload = parsed.value();
    const int promise = payload.promise;
    const std::string clazz = payload.clazz.get();
    const std::string function = payload.function;
    const std::string arguments_json = payload.arguments;
#if _DEBUG
    if(function != "__sendNativeMessage") {
        //orion::log_debug("WebExtensions: received JSON: %s", message_json.c_str());
        orion::log_debug("WebExtensions: ->%s.%s(%s) -> Promise{%d}", clazz.c_str(), function.c_str(), arguments_json.c_str(), promise);
    }
#endif
    const bool is_v2 = payload.version.value_or(1) == 2;
    const auto permissions = permissions_for_extension(*extension_id);
    const auto impls = WebExtensions::make_implementations(runtime);
    const auto impls_v2 = WebExtensions::v2::make_implementations(runtime);
    const bool handled = is_v2
        ? v2::native_call_bridge_received(promise, clazz, function, arguments_json, *web_view_bridge, extension_id->value, m_script_world, m_security_profile, permissions, impls_v2, error)
        : WebExtensions::native_call_bridge_received(promise, clazz, function, arguments_json, *web_view_bridge, extension_id->value, m_script_world, impls, error);
    if(!handled) {
        error = "while calling " + clazz + "." + function + "(" + arguments_json +  "): " + error;
        handle_error(error);
        // Settle the JS promise; without this the caller's await hangs forever
        // and the promise registry entry leaks.
        if(is_v2) {
            v2::Promise<void>{
                extension_id->value, *web_view_bridge, promise,
                m_script_world, m_security_profile}.reject(error);
        } else {
            WebExtensions::Promise<void>{
                *web_view_bridge, promise, m_script_world}.reject(error);
        }
        return;
    }
}

void
NativeBridgeContext::on_native_event(const std::string& message_json)
{
    auto profile_world = m_profile_world.lock();
    auto web_view_bridge = m_web_view_bridge.lock();
    auto* runtime = profile_world
        ? static_cast<WebExtensionsRuntimeImpl*>(profile_world->runtime())
        : nullptr;
    if(!runtime || !web_view_bridge) return;
    std::optional<orion::ExtensionId> extension_id =
        current_extension(*web_view_bridge);
    if(!extension_id) {
        handle_error("inaccessible context");
        return;
    }
    std::string error;
    auto parsed = WebExtensions::deserialize_json<NativeEventMessage>(message_json, &error);
    if(!parsed) {
#if defined(_DEBUG) && _DEBUG
        orion::log_debug("WebExtensions: received JSON: %s", message_json.c_str());
#endif
        handle_error("while parsing message: " + error);
        return;
    }
    const auto& payload = parsed.value();
    const int listener = payload.listener;
    const std::string clazz = payload.clazz.get();
    const std::string event = payload.event;
    const std::string command = payload.command;
    const std::string registration_arguments_json = payload.arguments.value_or("{}");
#if _DEBUG
    //orion::log_debug("WebExtensions: received JSON: %s", message_json.c_str());
    //orion::log_debug("WebExtensions: %s %s::%s -> Event{%d}", command.c_str(), clazz.c_str(), event.c_str(), listener);
#endif
    const bool is_v2 = payload.version.value_or(1) == 2;
    const auto permissions = permissions_for_extension(*extension_id);
    const auto impls = WebExtensions::make_implementations(runtime);
    const auto impls_v2 = WebExtensions::v2::make_implementations(runtime);
    const bool handled = is_v2
        ? v2::native_event_bridge_received(listener, clazz, event, command, registration_arguments_json, *web_view_bridge, extension_id->value, m_script_world, m_security_profile, permissions, impls_v2, error)
        : WebExtensions::native_event_bridge_received(listener, clazz, event, command, *web_view_bridge, extension_id->value, m_script_world, impls, error);
    if(!handled) {
        error = "while calling " + clazz + "." + event + "." + command + "(" + std::to_string(listener) +  "): " + error;
        handle_error(error);
        return;
    }
}

std::optional<orion::ExtensionId>
NativeBridgeContext::current_extension(
    orion::bridge::WebViewBridge& web_view_bridge)
{
    if(m_extension_id) {
        return m_extension_id;
    }
    std::string security_origin_host =
        web_view_bridge.web_view_security_origin_get_host();
    for(auto& weakref : m_extensions.value_or(WebExtensionList())) {
        auto extension = weakref.lock();
        if(!extension) {
            continue;
        }
        if(extension->extension_id().value == security_origin_host) {
            return extension->extension_id();
        }
    }
    return std::nullopt;
}

std::vector<std::string>
NativeBridgeContext::permissions_for_extension(
    const orion::ExtensionId& extension_id) const
{
    for(const auto& weakref : m_extensions.value_or(WebExtensionList())) {
        auto extension = weakref.lock();
        if(extension && extension->extension_id() == extension_id) {
            return extension->permissions();
        }
    }
    return {};
}

WebExtensionsRuntimeImpl::WebExtensionsRuntimeImpl(
    orion::bridge::ProfileWorld& profile_world)
    : m_profile_world(profile_world.weak_from_this())
{
    orion::log_verbose(
        "WebExtensionsRuntime: profile_id=%d private=%d",
        profile_world.profile_id(),
        static_cast<int>(profile_world.private_mode())
    );
}

void
WebExtensionsRuntimeImpl::initialize()
{
    auto profile_world = m_profile_world.lock();
    if(!profile_world) return;
    auto* api = &profile_world->core_api();
    initialize_implementations(
        m_profile_world, static_cast<StorageV2Impl*>(this));
    m_extensions_change_id = api->extensions_change().add_observer(
    [profile_world=m_profile_world](const ExtensionsChange& extensions_change){
        auto world = profile_world.lock();
        auto* runtime = world
            ? static_cast<WebExtensionsRuntimeImpl*>(world->runtime())
            : nullptr;
        if(!runtime) return;
        runtime->on_extensions_change(extensions_change).detach();
    });
    reload_extensions();
}

WebExtensionsRuntimeImpl::~WebExtensionsRuntimeImpl()
{
    auto profile_world = m_profile_world.lock();
    if(profile_world) {
        profile_world->core_api().extensions_change().remove_observer(
            m_extensions_change_id);
    }
    static_cast<ContextMenusV2Impl*>(this)->clear_all();

    if(profile_world) {
        orion::log_verbose(
            "~WebExtensionsRuntime: profile_id=%d private=%d",
            profile_world->profile_id(),
            static_cast<int>(profile_world->private_mode())
        );
    }
}

async_task<void>
WebExtensionsRuntimeImpl::on_extensions_change(ExtensionsChange extension_change)
{
    auto profile_world = m_profile_world;
    auto initial_world = profile_world.lock();
    if(!initial_world) co_return;
    const auto runtime_profile_id = initial_world->profile_id();
    initial_world.reset();
    bool changed = false;
    for(const auto& extension_info : extension_change.created) {
        if(extension_info.profile_id != runtime_profile_id || !extension_info.is_enabled) {
            continue;
        }
        auto world = profile_world.lock();
        auto* runtime = world
            ? static_cast<WebExtensionsRuntimeImpl*>(world->runtime())
            : nullptr;
        if(!runtime) co_return;
        std::shared_ptr<WebExtension> extension =
            co_await runtime->add_extension(extension_info);
        extension = nullptr;
        changed = true;
    }
    for(const auto& extension_id : extension_change.deleted) {
        auto world = profile_world.lock();
        auto* runtime = world
            ? static_cast<WebExtensionsRuntimeImpl*>(world->runtime())
            : nullptr;
        if(!runtime) co_return;
        if(runtime->remove_extension(extension_id)) {
            changed = true;
        }
    }
    for(const auto& extension_info : extension_change.updated) {
        const bool belongs_to_runtime =
            extension_info.profile_id == runtime_profile_id && extension_info.is_enabled;
        auto world = profile_world.lock();
        auto* runtime = world
            ? static_cast<WebExtensionsRuntimeImpl*>(world->runtime())
            : nullptr;
        if(!runtime) co_return;
        const bool is_loaded = runtime->m_extensions.contains(extension_info.id);
        if(!belongs_to_runtime) {
            if(runtime->remove_extension(extension_info.id)) {
                changed = true;
            }
            continue;
        }

        const bool resources_changed = extension_change.updated_file_path || extension_change.updated_type;
        if(!is_loaded || resources_changed) {
            std::shared_ptr<WebExtension> extension =
                co_await runtime->add_extension(extension_info);
            if(profile_world.expired()) co_return;
            extension = nullptr;
            changed = true;
        }
    }
    if(changed) {
        auto world = profile_world.lock();
        auto* runtime = world
            ? static_cast<WebExtensionsRuntimeImpl*>(world->runtime())
            : nullptr;
        if(!runtime) co_return;
        runtime->notify_update();
    }
    co_return;
}

async_task<std::shared_ptr<WebExtension>>
WebExtensionsRuntimeImpl::add_extension(const ExtensionDto& extension)
{
    log_debug("WebExtensionsRuntime> Adding extension {%s} %s", extension.id.value.c_str(), extension.name.c_str());
    auto profile_world = m_profile_world;
    auto world = profile_world.lock();
    if(!world) co_return nullptr;
    auto* api = &world->core_api();
    world.reset();
    std::shared_ptr<WebExtensionResources> resources = co_await async(invoke(
        &api->repository(), &WebExtensionsRepository::get_extension_resources), extension);
    world = profile_world.lock();
    auto* runtime = world
        ? static_cast<WebExtensionsRuntimeImpl*>(world->runtime())
        : nullptr;
    if(!runtime) co_return nullptr;
    auto ext = make_extension(extension.id, resources, api->debug_mode());
    runtime->m_extensions[extension.id] = ext;
    co_return ext;
}

bool
WebExtensionsRuntimeImpl::remove_extension(const ExtensionId& extension_id)
{
    if(m_extensions.erase(extension_id)) {
        static_cast<ContextMenusV2Impl*>(this)->clear_extension(extension_id.value);
        log_debug("WebExtensionsRuntime> Removed extension {%s}", extension_id.value.c_str());
        return true;
    }
    return false;
}

void
WebExtensionsRuntimeImpl::notify_update()
{
    WebExtensionList extension_list;
    for(const auto& [extension_id, extension] : m_extensions) {
        extension_list.push_back(extension);
    }
    m_extension_list = extension_list;
    on_extensions_update(extension_list);

    std::vector<std::function<void()>> pending_ready_replies;
    pending_ready_replies.swap(m_pending_ready_replies);
    for(auto& reply : pending_ready_replies) {
        reply();
    }
}

detached_task
WebExtensionsRuntimeImpl::reload_extensions()
{
    auto profile_world = m_profile_world;
    auto world = profile_world.lock();
    if(!world) co_return;
    clear_extensions();
    GetInstalledExtensionIdsQuery extension_ids_query;
    extension_ids_query.profile_id = world->profile_id();
    auto* api = &world->core_api();
    world.reset();
    GetAllInstalledExtensionIdsReply extension_ids_reply = co_await async(
        invoke(api, &CoreApi::get_installed_extension_ids), extension_ids_query);
    if(profile_world.expired()) co_return;
    GetInstalledExtensionsQuery extensions_query;
    extensions_query.ids = extension_ids_reply.ids;
    GetInstalledExtensionsReply extensions_reply = co_await async(
        invoke(api, &CoreApi::get_installed_extensions), extensions_query);
    world = profile_world.lock();
    auto* runtime = world
        ? static_cast<WebExtensionsRuntimeImpl*>(world->runtime())
        : nullptr;
    if(!runtime) co_return;
    ExtensionsChange extension_change;
    extension_change.created = extensions_reply.extensions;
    runtime->m_extension_list = WebExtensionList();
    co_await runtime->on_extensions_change(extension_change);
    world = profile_world.lock();
    runtime = world
        ? static_cast<WebExtensionsRuntimeImpl*>(world->runtime())
        : nullptr;
    if(!runtime) co_return;
    runtime->notify_update();
    co_return;
}

async_task<orion::Error>
WebExtensionsRuntimeImpl::reload_extension(const ExtensionId& extension_id)
{
    auto profile_world = m_profile_world;
    if (!m_extensions.contains(extension_id)) {
        co_return MAKE_ERROR(
            ErrorCode::not_found, "extension %s is not loaded",
            extension_id.value.c_str());
    }

    GetInstalledExtensionsQuery query;
    query.ids = {extension_id};
    auto world = profile_world.lock();
    if(!world) {
        co_return MAKE_ERROR(ErrorCode::invalid_state, "profile world was destroyed");
    }
    auto* api = &world->core_api();
    world.reset();
    const auto reply = co_await async(invoke(
        api, &CoreApi::get_installed_extensions), query);
    world = profile_world.lock();
    auto* runtime = world
        ? static_cast<WebExtensionsRuntimeImpl*>(world->runtime())
        : nullptr;
    if(!runtime) {
        co_return MAKE_ERROR(ErrorCode::invalid_state, "extension runtime was destroyed");
    }
    if (reply.err) {
        co_return reply.err;
    }
    if(reply.extensions.size() != 1
        || reply.extensions.front().profile_id != runtime->profile_id()
        || !reply.extensions.front().is_enabled) {
        co_return MAKE_ERROR(
            ErrorCode::not_found, "extension %s is not enabled in this runtime",
            extension_id.value.c_str());
    }

    static_cast<ContextMenusV2Impl*>(runtime)->clear_extension(extension_id.value);
    const auto extension = co_await runtime->add_extension(reply.extensions.front());
    if (!extension) {
        co_return MAKE_ERROR(ErrorCode::invalid_state, "extension runtime was destroyed");
    }
    world = profile_world.lock();
    runtime = world
        ? static_cast<WebExtensionsRuntimeImpl*>(world->runtime())
        : nullptr;
    if(!runtime) {
        co_return MAKE_ERROR(ErrorCode::invalid_state, "extension runtime was destroyed");
    }
    runtime->notify_update();
    co_return orion::Error{};
}

void WebExtensionsRuntimeImpl::clear_extensions()
{
    static_cast<ContextMenusV2Impl*>(this)->clear_all();
    m_extensions.clear();
}

ProfileId
WebExtensionsRuntimeImpl::profile_id()
{
    auto profile_world = m_profile_world.lock();
    assert(profile_world);
    return profile_world->profile_id();
}

orion::PrivateMode
WebExtensionsRuntimeImpl::private_mode()
{
    auto profile_world = m_profile_world.lock();
    assert(profile_world);
    return profile_world->private_mode();
}

std::optional<WebExtensionList>
WebExtensionsRuntimeImpl::get_extensions()
{
    return m_extension_list;
}

bool
WebExtensionsRuntimeImpl::is_ready() const
{
    return m_extension_list.has_value();
}

void
WebExtensionsRuntimeImpl::when_ready(std::function<void()> reply)
{
    if(is_ready()) {
        reply();
        return;
    }
    m_pending_ready_replies.push_back(std::move(reply));
}

std::shared_ptr<WebExtension>
WebExtensionsRuntimeImpl::extension_for_id(ExtensionId extension_id) const
{
    auto extension = m_extensions.find(extension_id);
    return extension == m_extensions.end() ? nullptr : extension->second;
}

std::optional<std::string>
WebExtensionsRuntimeImpl::document_uri(
    ExtensionId extension_id, WebExtensionDocumentKind kind) const
{
    auto extension = extension_for_id(extension_id);
    return extension ? extension->document_uri(kind) : std::nullopt;
}

std::vector<std::string>
WebExtensionsRuntimeImpl::script_source_for_extension(
    WebExtension* extension, v2::SecurityProfile security_profile)
{
    std::vector<std::string> scripts;
    scripts.reserve(ImplementsCount + 3);
    std::stringstream ss;
    ss << "(function(){" << std::endl;
    ss << "    window.__extern__ = {};" << std::endl;
    ss << "})();" << std::endl;
    scripts.push_back(ss.str());
    gather_script_source_for_extension(scripts, extension);
    scripts.push_back(WebExtensions::native_bridge_script());
    scripts.push_back(WebExtensions::v2::native_bridge_script({
        .security_profile = security_profile,
        .permissions = extension->permissions(),
    }));
    return scripts;
}

void
WebExtensionsRuntimeImpl::configure_web_view(
    orion::bridge::WebViewBridge& web_view_bridge)
{
    struct ContentScriptRegistration {
        orion::ExtensionId extension_id;
        std::string script_world;
        std::string native_call_handler;
        std::string native_event_handler;
    };

    auto profile_world = m_profile_world;
    const auto add_profile_scripts =
        [profile_world, &web_view_bridge](
            WebExtension* extension,
            v2::SecurityProfile security_profile,
            const std::vector<std::string>& allow_list,
            const std::vector<std::string>& block_list)
    {
        auto world = profile_world.lock();
        auto* runtime = world
            ? static_cast<WebExtensionsRuntimeImpl*>(world->runtime())
            : nullptr;
        if(!runtime) return;
        if(allow_list.empty()) return;
        const std::string native_call_handler =
            profile_handler_name("nativeCallBridge", security_profile);
        const std::string native_event_handler =
            profile_handler_name("nativeEventBridge", security_profile);
        web_view_bridge.web_view_add_script(
            allow_list, block_list,
            content_script_bridge_configuration(
                native_call_handler, native_event_handler));
        for(const auto& script :
            runtime->script_source_for_extension(extension, security_profile)) {
            web_view_bridge.web_view_add_script(
                allow_list, block_list, script, {},
                WebViewScriptInjectionTime::document_start,
                WebViewScriptFrames::all_frames);
        }
    };
    const auto append_document_match =
        [](std::vector<std::string>& matches,
            const std::optional<std::string>& document_uri)
    {
        if(document_uri) matches.push_back(*document_uri);
    };

    std::vector<std::string> extension_page_matches;
    std::vector<std::string> popup_matches;
    std::vector<std::string> sidebar_matches;
    std::vector<std::string> background_matches;
    std::vector<ContentScriptRegistration> content_script_bridges;
    for(const auto& weakref : m_extension_list.value_or(WebExtensionList())) {
        auto extension = weakref.lock();
        if(!extension) {
            continue;
        }

        const std::vector<std::string> extension_matches {
            extension->resources().scheme() + "://"
                + extension->extension_id().value + "/*"
        };
        extension_page_matches.push_back(extension_matches.front());

        std::vector<std::string> extension_background_matches;
        append_document_match(
            extension_background_matches, extension->background_page_uri());
        background_matches.insert(
            background_matches.end(),
            extension_background_matches.begin(),
            extension_background_matches.end());
        append_document_match(popup_matches, extension->default_popup_uri());
        append_document_match(sidebar_matches, extension->default_sidebar_uri());

        add_profile_scripts(
            extension.get(), v2::SecurityProfile::ExtensionPage,
            extension_matches, extension_background_matches);
        add_profile_scripts(
            extension.get(), v2::SecurityProfile::BackgroundScript,
            extension_background_matches, {});

        const auto content_scripts = extension->content_scripts();
        const std::string isolated_world =
            content_script_world(extension->extension_id());
        std::vector<std::string> isolated_api_matches;
        bool isolated_api_all_frames = false;
        for(const auto& content_script : content_scripts) {
            if(uses_main_world(content_script) || content_script.js.empty()) {
                continue;
            }
            auto matches =
                content_script_match_patterns(content_script.matches);
            isolated_api_matches.insert(
                isolated_api_matches.end(), matches.begin(), matches.end());
            isolated_api_all_frames =
                isolated_api_all_frames || content_script.all_frames;
        }
        std::sort(isolated_api_matches.begin(), isolated_api_matches.end());
        isolated_api_matches.erase(
            std::unique(
                isolated_api_matches.begin(), isolated_api_matches.end()),
            isolated_api_matches.end());

        if(!isolated_api_matches.empty()) {
            const std::string native_call_handler =
                content_script_handler_name(
                    "nativeCallBridgeContentScript",
                    extension->extension_id());
            const std::string native_event_handler =
                content_script_handler_name(
                    "nativeEventBridgeContentScript",
                    extension->extension_id());
            const auto bridge_frames = isolated_api_all_frames
                ? WebViewScriptFrames::all_frames
                : WebViewScriptFrames::top_frame;
            web_view_bridge.web_view_add_script(
                isolated_api_matches, {},
                content_script_bridge_configuration(
                    native_call_handler, native_event_handler),
                isolated_world, WebViewScriptInjectionTime::document_start,
                bridge_frames);
            for(const auto& script : script_source_for_extension(
                extension.get(), v2::SecurityProfile::ContentScript)) {
                web_view_bridge.web_view_add_script(
                    isolated_api_matches, {}, script, isolated_world,
                    WebViewScriptInjectionTime::document_start, bridge_frames);
            }
            content_script_bridges.push_back({
                .extension_id = extension->extension_id(),
                .script_world = isolated_world,
                .native_call_handler = native_call_handler,
                .native_event_handler = native_event_handler,
            });
        }

        for(const auto& content_script : content_scripts) {
            const auto matches =
                content_script_match_patterns(content_script.matches);
            if(matches.empty()) {
                continue;
            }
            const auto exclude_matches =
                content_script_match_patterns(content_script.exclude_matches);
            const std::string script_world = uses_main_world(content_script)
                ? std::string()
                : isolated_world;
            const auto frames = content_script_frames(content_script);

            for(const auto& path : content_script.css) {
                auto source = content_script_resource(*extension, path);
                if(!source) {
                    continue;
                }
                web_view_bridge.web_view_add_script(
                    matches, exclude_matches,
                    css_content_script_source(*extension, path, *source),
                    script_world, WebViewScriptInjectionTime::document_start,
                    frames);
            }
            for(const auto& path : content_script.js) {
                auto source = content_script_resource(*extension, path);
                if(!source) {
                    continue;
                }
                web_view_bridge.web_view_add_script(
                    matches, exclude_matches,
                    javascript_content_script_source(
                        *extension, path, *source),
                    script_world,
                    content_script_injection_time(content_script), frames);
            }
        }
    }

    const auto normalize_matches = [](std::vector<std::string>& matches) {
        std::sort(matches.begin(), matches.end());
        matches.erase(
            std::unique(matches.begin(), matches.end()), matches.end());
    };
    normalize_matches(extension_page_matches);
    normalize_matches(popup_matches);
    normalize_matches(sidebar_matches);
    normalize_matches(background_matches);

    std::vector<std::string> specialized_view_matches = popup_matches;
    specialized_view_matches.insert(
        specialized_view_matches.end(),
        sidebar_matches.begin(), sidebar_matches.end());
    specialized_view_matches.insert(
        specialized_view_matches.end(),
        background_matches.begin(), background_matches.end());
    normalize_matches(specialized_view_matches);

    const auto add_view_type_script =
        [&web_view_bridge](
            const std::vector<std::string>& allow_list,
            const std::vector<std::string>& block_list,
            const char* view_type)
    {
        if(allow_list.empty()) return;
        web_view_bridge.web_view_add_script(
            allow_list, block_list,
            "window.__orionWebExtensionViewType="
                + rfl::json::write(std::string(view_type)) + ";", {},
            WebViewScriptInjectionTime::document_start,
            WebViewScriptFrames::all_frames);
    };
    add_view_type_script(
        extension_page_matches, specialized_view_matches, "tab");
    add_view_type_script(popup_matches, {}, "popup");
    add_view_type_script(sidebar_matches, {}, "sidebar");
    add_view_type_script(background_matches, {}, "background");
    if(!extension_page_matches.empty()) {
        web_view_bridge.web_view_add_script(
            extension_page_matches, {}, extension_proxy_script_source(), {},
            WebViewScriptInjectionTime::document_start,
            WebViewScriptFrames::all_frames);
    }

    const auto extensions =
        get_extensions().value_or(WebExtensionList());
    auto destroy_data = [](void* context) {
        delete static_cast<std::shared_ptr<NativeBridgeContext>*>(context);
    };
    auto on_native_call =
        [](const std::string& message_json, void* context) {
            static_cast<std::shared_ptr<NativeBridgeContext>*>(context)
                ->get()->on_native_call(message_json);
        };
    auto on_native_event =
        [](const std::string& message_json, void* context) {
            static_cast<std::shared_ptr<NativeBridgeContext>*>(context)
                ->get()->on_native_event(message_json);
    };
    const auto register_profile_bridge =
        [profile_world, &web_view_bridge, &extensions,
            destroy_data, on_native_call, on_native_event](
            v2::SecurityProfile security_profile)
    {
        auto context = std::make_shared<NativeBridgeContext>(
            web_view_bridge, extensions, profile_world,
            std::string(), std::nullopt,
            security_profile);
        const std::string call_handler =
            profile_handler_name("nativeCallBridge", security_profile);
        const std::string event_handler =
            profile_handler_name("nativeEventBridge", security_profile);
        if(!web_view_bridge.web_view_register_script_message_handler(
            call_handler, on_native_call,
            new std::shared_ptr<NativeBridgeContext>(context), destroy_data)) {
            orion::log_debug(
                "WebExtensions: failed to register %s call handler",
                security_profile_name(security_profile));
        }
        if(!web_view_bridge.web_view_register_script_message_handler(
            event_handler, on_native_event,
            new std::shared_ptr<NativeBridgeContext>(context), destroy_data)) {
            orion::log_debug(
                "WebExtensions: failed to register %s event handler",
                security_profile_name(security_profile));
        }
    };
    register_profile_bridge(v2::SecurityProfile::ExtensionPage);
    register_profile_bridge(v2::SecurityProfile::BackgroundScript);

    for(const auto& bridge : content_script_bridges) {
        auto content_context = std::make_shared<NativeBridgeContext>(
            web_view_bridge, extensions, profile_world, bridge.script_world,
            bridge.extension_id,
            v2::SecurityProfile::ContentScript);
        if(!web_view_bridge.web_view_register_script_message_handler(
            bridge.native_call_handler, on_native_call,
            new std::shared_ptr<NativeBridgeContext>(content_context),
            destroy_data, bridge.script_world)) {
            orion::log_debug(
                "WebExtensions: failed to register content-script call "
                "handler for %s",
                bridge.extension_id.value.c_str());
        }
        if(!web_view_bridge.web_view_register_script_message_handler(
            bridge.native_event_handler, on_native_event,
            new std::shared_ptr<NativeBridgeContext>(content_context),
            destroy_data, bridge.script_world)) {
            orion::log_debug(
                "WebExtensions: failed to register content-script event "
                "handler for %s",
                bridge.extension_id.value.c_str());
        }
    }
}

std::shared_ptr<WebExtension>
WebExtensionsRuntime::make_extension(
    orion::ExtensionId extension_id,
    std::shared_ptr<WebExtensionResources> resources,
    bool debug_mode)
{
    return WebExtension::create(extension_id, resources, debug_mode);
}

std::unique_ptr<WebExtensionsRuntime>
WebExtensionsRuntime::create(orion::bridge::ProfileWorld& profile_world)
{
    return std::make_unique<WebExtensionsRuntimeImpl>(profile_world);
}

} // namespace WebExtensions
