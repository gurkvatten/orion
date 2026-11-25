#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <core/dto/profile_dto.h>
#include <core/dto/extension_dto.h>
#include <core/webextensions/multicast_delegate.h>

namespace orion { class CoreApi; }

namespace WebExtensions {

class WebExtensionResources;
struct WebExtensionResourceData;

enum class WebExtensionDocumentKind {
    popup,
    sidebar,
    background_page,
    background_service_worker
};

struct WebExtensionContentScript {
    orion::ExtensionId extension_id;
    std::vector<std::string> matches;
    std::vector<std::string> exclude_matches;
    std::vector<std::string> js;
    std::vector<std::string> css;
    std::optional<std::string> run_at;
    std::optional<std::string> world;
    bool all_frames = false;
};

class WebExtension {
public:
    virtual ~WebExtension() {};

    virtual orion::ExtensionId extension_id() const = 0;
    virtual const WebExtensionResources& resources() const = 0;

    virtual std::optional<std::string> manifest_uri() const = 0;
    virtual std::optional<std::string> document_uri(WebExtensionDocumentKind kind) const = 0;
    virtual std::optional<std::string> default_popup_uri() const = 0;
    virtual std::optional<std::string> default_sidebar_uri() const = 0;
    virtual std::optional<std::string> background_page_uri() const = 0;
    virtual std::optional<std::string> background_service_worker_uri() const = 0;
    virtual bool uses_background_service_worker() const = 0;
    virtual std::vector<std::string> background_script_uris() const = 0;
    virtual std::vector<WebExtensionContentScript> content_scripts() const = 0;
    virtual std::vector<std::string> permissions() const = 0;

    virtual std::optional<std::string> default_title() const = 0;
    virtual std::optional<WebExtensionResourceData> default_icon_data() const = 0;

    virtual std::optional<std::string> default_locale() const = 0;

private:
    static std::shared_ptr<WebExtension> create(
        orion::ExtensionId extension_id,
        std::shared_ptr<WebExtensionResources> resources,
        bool debug_mode);
    friend class WebExtensionsRuntime;
};

} // namespace WebExtensions

using WebExtensions::WebExtension;
using WebExtensions::WebExtensionContentScript;
using WebExtensions::WebExtensionDocumentKind;
