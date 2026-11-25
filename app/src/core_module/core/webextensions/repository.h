#pragma once

#include <functional>
#include <memory>

#include <core/dto/extension_dto.h>
#include <core/webkit_bridge.h>

namespace orion { class CoreApi; }

namespace WebExtensions {

struct WebExtensionResourceData {
    uint8_t* data;
    size_t datalen;
};

class WebExtensionResources {
public:
    virtual ~WebExtensionResources() {};
    virtual const Manifest& manifest() const = 0;
    virtual const std::string& scheme() const = 0;
    virtual std::optional<std::string> background_page_path() const = 0;
    virtual bool background_page_is_service_worker() const = 0;

    virtual std::optional<WebExtensionResourceData> data_for_path(const std::string& path) const = 0;
};

class WebExtensionsRepository {
public:
    typedef std::function<void(std::shared_ptr<WebExtensionResources>)> WebExtensionResourcesCompletionHandler;
    virtual void get_extension_resources(const orion::ExtensionDto& extension, WebExtensionResourcesCompletionHandler completion_handler) = 0;

private:
    static std::shared_ptr<WebExtensionsRepository> create(orion::CoreApi* api);
    friend class orion::CoreApi;
};

} // namespace WebExtensions

using WebExtensions::WebExtensionResourceData;
using WebExtensions::WebExtensionResources;
using WebExtensions::WebExtensionsRepository;
