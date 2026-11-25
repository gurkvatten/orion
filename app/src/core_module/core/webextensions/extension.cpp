#include <common/src/log.h>
#include <common/src/uri.h>
#include <common/src/coalesce.h>
#include <core/webextensions/extension.h>
#include <core/webextensions/repository.h>

#include <algorithm>

using namespace orion;

namespace WebExtensions {

class WebExtensionImpl : public WebExtension {
public:
    WebExtensionImpl(
        orion::ExtensionId extension_id,
        std::shared_ptr<WebExtensionResources> resources,
        bool debug_mode);

    orion::ExtensionId extension_id() const override;
    const WebExtensionResources& resources() const override;

    std::optional<std::string> manifest_uri() const override;
    std::optional<std::string> document_uri(WebExtensionDocumentKind kind) const override;
    std::optional<std::string> default_popup_uri() const override;
    std::optional<std::string> default_sidebar_uri() const override;
    std::optional<std::string> background_page_uri() const override;
    std::optional<std::string> background_service_worker_uri() const override;
    bool uses_background_service_worker() const override;
    std::vector<std::string> background_script_uris() const override;
    std::vector<WebExtensionContentScript> content_scripts() const override;
    std::vector<std::string> permissions() const override;

    std::optional<std::string> default_title() const override;
    std::optional<WebExtensionResourceData> default_icon_data() const override;

    std::optional<std::string> default_locale() const override;

protected:
    ManifestBrowserAction action() const;
    ManifestSidebarAction sidebar_action() const;
    ManifestSidePanel side_panel() const;
    ManifestBackground background() const;

private:
    orion::ExtensionId m_extension_id;
    std::shared_ptr<WebExtensionResources> m_resources;
    bool m_debug_mode = false;
};

static std::optional<std::string>
make_uri(const WebExtension& extension, const std::optional<std::string>& path)
{
    if(!path || path->empty()) {
        return std::nullopt;
    }
    std::string prefix = extension.resources().scheme() + "://" + extension.extension_id().value;
    if(path.value()[0] != '/') {
        prefix += "/";
    }
    return prefix + path.value();
}

WebExtensionImpl::WebExtensionImpl(
    orion::ExtensionId extension_id,
    std::shared_ptr<WebExtensionResources> resources,
    bool debug_mode)
{
    m_extension_id = extension_id;
    m_resources = resources;
    m_debug_mode = debug_mode;
}

orion::ExtensionId
WebExtensionImpl::extension_id() const
{
    return m_extension_id;
}

const WebExtensionResources&
WebExtensionImpl::resources() const
{
    return *m_resources.get();
}

std::optional<std::string>
WebExtensionImpl::manifest_uri() const
{
    return make_uri(*this, "manifest.json");
}

ManifestBrowserAction
WebExtensionImpl::action() const
{
    switch(m_resources->manifest().manifest_version.value_or(2)) {
        case 2: return m_resources->manifest().browser_action.value_or(ManifestBrowserAction());
        case 3: return m_resources->manifest().action.value_or(ManifestBrowserAction());
        default: return ManifestBrowserAction();
    }
}

ManifestSidebarAction
WebExtensionImpl::sidebar_action() const
{
    return m_resources->manifest().sidebar_action.value_or(ManifestSidebarAction());
}

ManifestSidePanel
WebExtensionImpl::side_panel() const
{
    return m_resources->manifest().side_panel.value_or(ManifestSidePanel());
}

ManifestBackground
WebExtensionImpl::background() const
{
    return m_resources->manifest().background.value_or(ManifestBackground());
}

std::optional<std::string>
WebExtensionImpl::document_uri(WebExtensionDocumentKind kind) const
{
    switch(kind) {
        case WebExtensionDocumentKind::popup:
            return default_popup_uri();
        case WebExtensionDocumentKind::sidebar:
            return default_sidebar_uri();
        case WebExtensionDocumentKind::background_page:
            return background_page_uri();
        case WebExtensionDocumentKind::background_service_worker:
            return background_service_worker_uri();
    }
    return std::nullopt;
}

std::optional<std::string>
WebExtensionImpl::default_popup_uri() const
{
    return make_uri(*this, action().default_popup);
}

std::optional<std::string>
WebExtensionImpl::default_sidebar_uri() const
{
    return make_uri(*this, coalesce(sidebar_action().default_panel, side_panel().default_path));
}

std::optional<std::string>
WebExtensionImpl::background_page_uri() const
{
    return make_uri(*this, m_resources->background_page_path());
}

std::optional<std::string>
WebExtensionImpl::background_service_worker_uri() const
{
    return make_uri(*this, background().service_worker);
}

bool
WebExtensionImpl::uses_background_service_worker() const
{
    return m_resources->background_page_is_service_worker();
}

std::vector<std::string>
WebExtensionImpl::background_script_uris() const
{
    std::vector<std::string> result;
    for(const auto& script : background().scripts.value_or(std::vector<std::string>())) {
        auto uri = make_uri(*this, script);
        if(uri) {
            result.push_back(*uri);
        }
    }
    return result;
}

std::vector<WebExtensionContentScript>
WebExtensionImpl::content_scripts() const
{
    std::vector<WebExtensionContentScript> result;
    for(const auto& script : m_resources->manifest().content_scripts.value_or(std::vector<ManifestContentScript>())) {
        result.push_back(WebExtensionContentScript {
            .extension_id = m_extension_id,
            .matches = script.matches.value_or(std::vector<std::string>()),
            .exclude_matches = script.exclude_matches.value_or(std::vector<std::string>()),
            .js = script.js.value_or(std::vector<std::string>()),
            .css = script.css.value_or(std::vector<std::string>()),
            .run_at = script.run_at,
            .world = script.world,
            .all_frames = script.all_frames.value_or(false),
        });
    }
    return result;
}

std::vector<std::string>
WebExtensionImpl::permissions() const
{
    auto permissions = m_resources->manifest().permissions.value_or(std::vector<std::string>());

    // The debug capability is declared like a normal permission, but it is
    // granted only when Orion itself runs in debug mode.
    if (!m_debug_mode) {
        std::erase(permissions, "debug");
    }
    return permissions;
}

std::optional<std::string>
WebExtensionImpl::default_title() const
{
    return coalesce(action().default_title, m_resources->manifest().name);
}

std::optional<WebExtensionResourceData>
WebExtensionImpl::default_icon_data() const
{
    auto default_icon = coalesce(action().default_icon, m_resources->manifest().icons);
    if(default_icon && !default_icon->is_null()) {
        auto& variant = default_icon->variant();
        if (auto* value = std::get_if<std::string>(&variant)) {
            return m_resources->data_for_path(*value);
        }
        if (auto* value = std::get_if<rfl::Object<rfl::Generic>>(&variant)) {
            rfl::Generic last_icon = value->crbegin()->second;
            if(auto* value = std::get_if<std::string>(&last_icon.variant())) {
                return m_resources->data_for_path(*value);
            }
        }
    }
    return std::nullopt;
}

std::optional<std::string>
WebExtensionImpl::default_locale() const
{
    return m_resources->manifest().default_locale;
}

std::shared_ptr<WebExtension>
WebExtension::create(
    orion::ExtensionId extension_id,
    std::shared_ptr<WebExtensionResources> resources,
    bool debug_mode)
{
    return std::make_shared<WebExtensionImpl>(extension_id, resources, debug_mode);
}

} // namespace WebExtensions
