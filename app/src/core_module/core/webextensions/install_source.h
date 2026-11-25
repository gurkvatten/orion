#pragma once

#include <functional>
#include <memory>

#include <core/webkit_bridge.h>
#include <core/dto/profile_dto.h>
#include <core/dto/extension_dto.h>

namespace orion { class CoreApi; }

namespace WebExtensions {

class WebExtensionsInternalStore;

class WebExtensionInstallSource {
public:
    virtual ~WebExtensionInstallSource() {};

    typedef std::function<void(orion::InstallExtensionReply)> InstallCompletionHandler;
    virtual void install(orion::ProfileId profile_id, InstallCompletionHandler completion_handler) = 0;

private:
    static std::shared_ptr<WebExtensionInstallSource> create(orion::CoreApi* api, const ExtensionInfo& extension, const std::string& filepath);
    friend class WebExtensionsInternalStore;
};

} // namespace WebExtensions

using WebExtensions::WebExtensionInstallSource;
