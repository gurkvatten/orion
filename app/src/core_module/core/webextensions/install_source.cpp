#include "install_source.h"

#include <common/src/log.h>
#include <common/src/zipper.h>
#include <common/src/dispatch.h>
#include <common/src/coroutine.h>
#include <core/core_api.h>

using namespace orion;

namespace WebExtensions {

class WebExtensionInstallSourceImpl : public WebExtensionInstallSource {
public:
    WebExtensionInstallSourceImpl(CoreApi* api, const ExtensionInfo& extension, const std::string& filepath);
    void install(orion::ProfileId profile_id, InstallCompletionHandler completion_handler) override;

private:
    CoreApi* m_api;
    ExtensionInfo m_extension;
    std::string m_filepath;
};

std::shared_ptr<WebExtensionInstallSource>
WebExtensionInstallSource::create(CoreApi* api, const ExtensionInfo& extension, const std::string& filepath)
{
    return std::make_shared<WebExtensionInstallSourceImpl>(api, extension, filepath);
}

WebExtensionInstallSourceImpl::WebExtensionInstallSourceImpl(
    CoreApi* api, const ExtensionInfo& extension, const std::string& filepath
) : m_api(api), m_extension(extension), m_filepath(filepath)
{
}

void
WebExtensionInstallSourceImpl::install(orion::ProfileId profile_id, InstallCompletionHandler completion_handler)
{
    orion::InstallExtensionQuery query;
    query.profile_id = profile_id;
    query.sku = m_api->internal_store().get_extension_sku(m_extension);
    query.type = m_api->internal_store().get_extension_type(m_extension);
    query.file_path = m_filepath;
    query.name = m_extension.displayName.value_or("");
    query.description = m_extension.info.value_or("");
    query.icon_ref = m_extension.iconURL.value_or("");
    m_api->install_extension(query, completion_handler);
}

} // namespace WebExtensions
