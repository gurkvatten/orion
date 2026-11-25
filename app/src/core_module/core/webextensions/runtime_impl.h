#pragma once

#include <core/core_api.h>
#include <common/src/coroutine.h>
#include <core/webextensions/runtime.h>
#include <core/webextensions/extension.h>
#include <core/webextensions/private/action_v2.h>
#include <core/webextensions/private/notifications.h>
#include <core/webextensions/private/context_menus_v2.h>
#include <core/webextensions/private/alarms.h>
#include <core/webextensions/private/extension_api.h>
#include <core/webextensions/private/permissions.h>
#include <core/webextensions/private/runtime.h>
#include <core/webextensions/private/tabs_v2.h>
#include <core/webextensions/private/webrequest_v2.h>
#include <core/webextensions/private/webnavigation_v2.h>
#include <core/webextensions/private/windows.h>
#include <core/webextensions/private/scripting_v2.h>
#include <core/webextensions/private/storage.h>
#include <core/webextensions/private/i18n.h>
#include <core/webextensions/private/test.h>

namespace WebExtensions {

class WebExtensionsRuntimeImpl;

template<typename T, typename = void>
struct has_set_profile_world : std::false_type {};

template<typename T>
struct has_set_profile_world<T, std::void_t<
    decltype(std::declval<T&>().set_profile_world(
        std::declval<std::weak_ptr<orion::bridge::ProfileWorld>>()))
>> : std::true_type {};

template<typename T, typename = void>
struct has_set_storage : std::false_type {};

template<typename T>
struct has_set_storage<T, std::void_t<
    decltype(std::declval<T&>().set_storage(std::declval<StorageV2Impl*>()))
>> : std::true_type {};

template<typename T, typename = void>
struct has_script_source : std::false_type {};

template<typename T>
struct has_script_source<T, std::void_t<
    decltype(std::declval<T&>().script_source_for_extension(std::declval<WebExtension*>()))
>> : std::true_type {};

template<typename... Args>
class Implements : public Args... {
public:
    static constexpr int ImplementsCount = sizeof...(Args);
    void initialize_implementations(
        std::weak_ptr<orion::bridge::ProfileWorld> profile_world,
        StorageV2Impl* storage) {
        (call_set_profile_world<Args>(profile_world), ...);
        (call_set_storage<Args>(storage), ...);
    }
    std::vector<std::string>
    gather_script_source_for_extension(std::vector<std::string>& scripts, WebExtension* extension)
    {
        (call_script_source_for_extension<Args>(scripts, extension), ...);
        return scripts;
    }
private:
    template<typename T>
    void
    call_set_profile_world(std::weak_ptr<orion::bridge::ProfileWorld> profile_world)
    {
        if constexpr (has_set_profile_world<T>::value)
            static_cast<T&>(*this).set_profile_world(profile_world);
    }
    template<typename T>
    void
    call_set_storage(StorageV2Impl* storage)
    {
        if constexpr (has_set_storage<T>::value)
            static_cast<T&>(*this).set_storage(storage);
    }
    template<typename T>
    void
    call_script_source_for_extension(std::vector<std::string>& scripts, WebExtension* extension)
    {
        if constexpr (has_script_source<T>::value)
            return scripts.push_back(static_cast<T&>(*this).script_source_for_extension(extension));
    }
};

class WebExtensionsRuntimeImpl :
    public WebExtensionsRuntime,
    public Implements<
        AlarmsImpl, ActionV2Impl, NotificationsImpl, ContextMenusV2Impl, RuntimeImpl, PermissionsImpl, ExtensionImpl,
        WebRequestV2Impl, WebNavigationV2Impl, TabsV2Impl,
        StorageV2Impl, StorageAreaLocalV2Impl, StorageAreaSyncV2Impl,
        StorageAreaManagedV2Impl, StorageAreaSessionV2Impl,
        I18nImpl, WindowsImpl, ScriptingV2Impl,
        PageAction, TestImpl>
{
public:
    std::vector<std::string> script_source_for_extension(
        WebExtension* extension, v2::SecurityProfile security_profile);
    explicit WebExtensionsRuntimeImpl(orion::bridge::ProfileWorld& profile_world);
    ~WebExtensionsRuntimeImpl();
    void initialize() override;
    detached_task reload_extensions();
    async_task<orion::Error> reload_extension(
        const orion::ExtensionId& extension_id);
    void clear_extensions();
public:
    orion::ProfileId profile_id() override;
    orion::PrivateMode private_mode() override;
    std::optional<WebExtensionList> get_extensions() override;
    bool is_ready() const override;
    void when_ready(std::function<void()> reply) override;
    std::shared_ptr<WebExtension> extension_for_id(
        orion::ExtensionId extension_id) const override;
    std::optional<std::string> document_uri(
        orion::ExtensionId extension_id,
        WebExtensionDocumentKind kind) const override;
    void configure_web_view(orion::bridge::WebViewBridge& web_view) override;
protected:
    async_task<void> on_extensions_change(orion::ExtensionsChange extension_change);
    async_task<std::shared_ptr<WebExtension>> add_extension(const orion::ExtensionDto& extension);
    bool remove_extension(const orion::ExtensionId& extension_id);
    void notify_update();
private:
    std::weak_ptr<orion::bridge::ProfileWorld> m_profile_world;
    orion::ObserverId m_extensions_change_id;
    std::map<orion::ExtensionId, std::shared_ptr<WebExtension>> m_extensions;
    std::optional<WebExtensionList> m_extension_list;
    std::vector<std::function<void()>> m_pending_ready_replies;
};

} // namespace WebExtensions
