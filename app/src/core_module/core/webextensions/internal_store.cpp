#include "internal_store.h"
#include "common/src/log.h"
#include "common/src/memory.h"
#include "core/core_api.h"
#include "core/webkit_bridge.h"
#include "common/src/dispatch.h"
#include "core/downloader.h"
#include "common/src/uuid.h"
#include "core/webextensions/install_source.h"
#include "embed/debug_extension_list.json.h"
#include "embed/extension_list.json.h"


#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iterator>
#include <map>
#include <utility>

using namespace std::literals::chrono_literals;
using namespace orion;

namespace WebExtensions {

template<typename T>
static void
append_vector(std::optional<std::vector<T>>& destination, const std::optional<std::vector<T>>& source)
{
    if(!source || source->empty()) {
        return;
    }
    if(!destination) {
        destination.emplace();
    }
    std::copy(source->begin(), source->end(), std::back_inserter(*destination));
}

class WebExtensionsInternalStoreImpl : public WebExtensionsInternalStore {
public:
    WebExtensionsInternalStoreImpl(CoreApi* api);

    void parse_extension_list(const std::string& source, FetchExtensionListReply reply) override;
    void get_extension_list(FetchExtensionListReply reply) override;
    void fetch_extension(const ExtensionInfo& extension, FetchCompletionHandler completion_handler) override;
    ExtensionSku get_extension_sku(const ExtensionInfo& extension) override;
    ExtensionType get_extension_type(const ExtensionInfo& extension) override;

private:
    CoreApi* m_api;
};

WebExtensionsInternalStoreImpl::WebExtensionsInternalStoreImpl(CoreApi* api) : m_api(api)
{
}

void
WebExtensionsInternalStoreImpl::parse_extension_list(const std::string& source, FetchExtensionListReply reply)
{
    dispatch(orion::on_main_thread(), [this, reply, source](){
        std::string error;
        auto parsed = deserialize_json<ExtensionList>(source, &error);
        if(!parsed) {
            log_debug("WebExtensions: Failed to parse extension list: %s", error.c_str());
        }
        reply(parsed.value_or(ExtensionList()));
    });
}

void
WebExtensionsInternalStoreImpl::get_extension_list(FetchExtensionListReply reply)
{
    std::string extension_list_source(
        reinterpret_cast<const char*>(extension_list_json_data),
        extension_list_json_length);

    parse_extension_list(extension_list_source, [this, reply](const ExtensionList& extension_list) {
        if(!m_api->debug_mode()) {
            reply(extension_list);
            return;
        }

        std::string debug_extension_list_source(
            reinterpret_cast<const char*>(debug_extension_list_json_data),
            debug_extension_list_json_length);
        ExtensionList combined_extension_list = extension_list;
        parse_extension_list(
            debug_extension_list_source,
            [combined_extension_list = std::move(combined_extension_list), reply](
                const ExtensionList& debug_extension_list) mutable {
                append_vector(combined_extension_list.debug, debug_extension_list.debug);
                append_vector(combined_extension_list.debug, debug_extension_list.passwordManagers);
                append_vector(combined_extension_list.debug, debug_extension_list.adBlockers);
                append_vector(combined_extension_list.debug, debug_extension_list.browserBetter);
                reply(combined_extension_list);
            });
    });
}

void
WebExtensionsInternalStoreImpl::fetch_extension(
    const WebExtensions::ExtensionInfo& extension, FetchCompletionHandler completion_handler)
{
    std::string target_path = m_api->get_extension_fetch_path(extension);
    std::error_code ec;
    if(std::filesystem::exists(target_path, ec)) {
        completion_handler(make_source(m_api, extension, target_path));
        return;
    }
    std::string url = extension.installURL.value_or("");
#ifndef NDEBUG
    constexpr const char* file_url_prefix = "file://";
    if(url.rfind(file_url_prefix, 0) == 0) {
        std::string local_path = url.substr(std::char_traits<char>::length(file_url_prefix));
        if(std::filesystem::is_regular_file(local_path, ec)) {
            completion_handler(make_source(m_api, extension, local_path));
        } else {
            orion::log_debug("WebExtensions: Local extension not found: %s", local_path.c_str());
            completion_handler(nullptr);
        }
        return;
    }
#endif
    auto downloader = m_api->create_downloader();
    if(!downloader) {
        return;
    }
    downloader->set_target_path(target_path);
    downloader->set_uri(url);
    downloader->set_finished_cb([this,guard=downloader,extension,target_path,url,completion_handler](orion::Error err) {
        if(err) {
            orion::log_debug("Fetch failed %s -> %s", url.c_str(), target_path.c_str());
            completion_handler(nullptr);
            return;
        }
        orion::log_debug("Fetched %s -> %s", url.c_str(), target_path.c_str());
        completion_handler(make_source(m_api, extension, target_path));
    });
    downloader->start_download();
}

ExtensionSku
WebExtensionsInternalStoreImpl::get_extension_sku(const ExtensionInfo& extension)
{
    if(extension.webExtID) {
        return orion::ExtensionSku(orion::generate_uuid_v4(extension.webExtID.value()));
    }
    if(extension.geckoId) {
        return orion::ExtensionSku(orion::generate_uuid_v4(extension.geckoId.value()));
    }
    if(extension.installURL) {
        return orion::ExtensionSku(orion::generate_uuid_v4(extension.installURL.value()));
    }
    return orion::ExtensionSku("0");
}

ExtensionType
WebExtensionsInternalStoreImpl::get_extension_type(const ExtensionInfo& extension)
{
    if(!extension.webExtType) {
        return ExtensionType::not_set;
    }
    if(extension.webExtType.value() == "chrome") {
        return ExtensionType::chrome;
    }
    if(extension.webExtType.value() == "firefox") {
        return ExtensionType::firefox;
    }
    return ExtensionType::not_set;
}

std::shared_ptr<WebExtensionInstallSource>
WebExtensionsInternalStore::make_source(
    orion::CoreApi* api, const ExtensionInfo& extension, const std::string& filepath)
{
    return WebExtensionInstallSource::create(api, extension, filepath);
}

std::shared_ptr<WebExtensionsInternalStore>
WebExtensionsInternalStore::create(orion::CoreApi* api)
{
    return std::make_shared<WebExtensionsInternalStoreImpl>(api);
}

} // namespace WebExtensions
