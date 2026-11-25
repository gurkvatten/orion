#pragma once

#include <functional>
#include <memory>
#include <vector>

#include <core/dto/profile_dto.h>
#include <core/dto/extension_dto.h>
#include <core/webextensions/extension.h>
#include <core/webextensions/multicast_delegate.h>
#include <core/private_mode.h>

namespace orion { class CoreApi; }
namespace orion::bridge { class ProfileWorld; class WebViewBridge; }

namespace WebExtensions {

class WebExtensionResources;
class WebExtension;

typedef std::vector<std::weak_ptr<WebExtension>> WebExtensionList;

class WebExtensionsRuntime {
public:
    virtual ~WebExtensionsRuntime() {};

    virtual orion::ProfileId profile_id() = 0;
    virtual orion::PrivateMode private_mode() = 0;

    typedef std::function<void(WebExtensionList)> GetExtensionsCompletionHandler;
    typedef GetExtensionsCompletionHandler OnExtensionsUpdateDelegate;

    MulticastDelegate<WebExtensionList> on_extensions_update;
    virtual std::optional<WebExtensionList> get_extensions() = 0;
    virtual bool is_ready() const = 0;
    virtual void when_ready(std::function<void()> reply) = 0;
    virtual std::shared_ptr<WebExtension> extension_for_id(
        orion::ExtensionId extension_id) const = 0;
    virtual std::optional<std::string> document_uri(
        orion::ExtensionId extension_id,
        WebExtensionDocumentKind kind) const = 0;

    virtual void configure_web_view(orion::bridge::WebViewBridge& web_view) = 0;

protected:
    static std::shared_ptr<WebExtension> make_extension(
        orion::ExtensionId extension_id,
        std::shared_ptr<WebExtensionResources> resources,
        bool debug_mode);

private:
    virtual void initialize() = 0;
    static std::unique_ptr<WebExtensionsRuntime> create(
        orion::bridge::ProfileWorld& profile_world);

    friend class orion::bridge::ProfileWorld;
};

} // namespace WebExtensions

using WebExtensions::WebExtensionsRuntime;
using WebExtensions::WebExtensionList;
