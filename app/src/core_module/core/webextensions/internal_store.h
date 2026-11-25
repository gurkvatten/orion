#pragma once

#include <vector>
#include <memory>
#include <functional>

#include <core/dto/extension_dto.h>
#include <core/webkit_bridge.h>
#include <core/webextensions/install_source.h>

namespace orion { class CoreApi; }

namespace WebExtensions {

class WebExtensionInstallSource;

class WebExtensionsInternalStore {
public:
    virtual ~WebExtensionsInternalStore() {};

    typedef std::function<void(const WebExtensions::ExtensionList& extension_list)> FetchExtensionListReply;
    virtual void parse_extension_list(const std::string& source, FetchExtensionListReply reply) = 0;
    virtual void get_extension_list(FetchExtensionListReply reply) = 0;

    typedef std::function<void(std::shared_ptr<WebExtensionInstallSource> source)> FetchCompletionHandler;
    virtual void fetch_extension(const ExtensionInfo& extension, FetchCompletionHandler completion_handler) = 0;

    virtual orion::ExtensionSku get_extension_sku(const ExtensionInfo& extension) = 0;

    virtual orion::ExtensionType get_extension_type(const ExtensionInfo& extension) = 0;

protected:
    static std::shared_ptr<WebExtensionInstallSource> make_source(orion::CoreApi* api, const ExtensionInfo& extension, const std::string& filepath);

private:
    static std::shared_ptr<WebExtensionsInternalStore> create(orion::CoreApi* api);
    friend class orion::CoreApi;
};

} // namespace WebExtensions

using WebExtensions::WebExtensionsInternalStore;
