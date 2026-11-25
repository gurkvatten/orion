#include <core/webextensions/extension.h>
#include <core/webextensions/private/extension_api.h>
#include <core/webextensions/repository.h>
#include <core/webextensions/runtime_impl.h>
#include <core/private_mode.h>

namespace WebExtensions {

std::string
ExtensionImpl::script_source_for_extension(WebExtension* extension)
{
    const std::string id = extension->extension_id().value;
    const std::string scheme = extension->resources().scheme();
    auto profile_world = m_profile_world.lock();
    auto* runtime = profile_world ? profile_world->runtime() : nullptr;
    const bool in_incognito_context =
        runtime && runtime->private_mode() == orion::PrivateMode::yes;
    std::stringstream script;
    script << R"v2extension(
(function(){
    const implementations = window.__webextensions_v2_implementations__ =
        window.__webextensions_v2_implementations__ || {};

    implementations["Extension.lastError"] = () =>
        window.__bridge_v2__ ? window.__bridge_v2__.lastError : null;
    implementations["Extension.inIncognitoContext"] = () => )v2extension"
        << (in_incognito_context ? "true" : "false") << R"v2extension(;
    implementations["Extension.getURL"] = (context) =>
        ")v2extension" << scheme << R"v2extension(://)v2extension" << id << R"v2extension(/" + context.args.path;

    // Window objects cannot cross the native JSON bridge. Expose the local
    // extension view when it matches the requested type; other live views
    // cannot yet be returned across the bridge.
    implementations["Extension.getViews"] = (context) => {
        const viewType = window.__orionWebExtensionViewType;
        if(viewType === undefined) return [];
        const fetchProperties = context.args.fetchProperties || {};
        if(fetchProperties.type !== undefined && fetchProperties.type !== viewType) return [];
        return [window];
    };
    implementations["Extension.getBackgroundPage"] = () =>
        window.__orionServiceWorkerEmulation ? window : undefined;

    implementations["Extension.sendRequest"] = (context) => {
        const runtime = window.chrome && window.chrome.runtime;
        if(!runtime) return Promise.reject(new Error("runtime API is unavailable"));
        return context.args.extensionId === undefined
            ? runtime.sendMessage(context.args.request)
            : runtime.sendMessage(context.args.extensionId, context.args.request);
    };

    implementations["Extension.events"] = (context) => {
        const runtime = window.chrome && window.chrome.runtime;
        if(!runtime) throw new Error("runtime API is unavailable");
        return context.id === "onRequestExternal"
            ? runtime.onMessageExternal
            : runtime.onMessage;
    };
})();
)v2extension";
    return script.str();
}

void
ExtensionImpl::set_profile_world(
    std::weak_ptr<orion::bridge::ProfileWorld> profile_world)
{
    m_profile_world = profile_world;
}

void
ExtensionImpl::isAllowedIncognitoAccess(v2::Promise<bool> promise)
{
    auto profile_world = m_profile_world.lock();
    auto* runtime = profile_world ? profile_world->runtime() : nullptr;
    promise.resolve(runtime && runtime->private_mode() == orion::PrivateMode::yes);
}

void
ExtensionImpl::isAllowedFileSchemeAccess(v2::Promise<bool> promise)
{
    promise.resolve(false);
}

void
ExtensionImpl::setUpdateUrlData(v2::Promise<void> promise, const std::string&)
{
    promise.resolve();
}

} // namespace WebExtensions
