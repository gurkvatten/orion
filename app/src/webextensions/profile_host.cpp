#include "webextensions/profile_host.h"
#include "webextensions/clipboard_host.h"
#include "webextensions/context_menu_host.h"

#include <core/core_api.h>

namespace orion {

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT

namespace {

class GtkProfileHost final : public bridge::ProfileHost {
public:
    GtkProfileHost(CoreApi& core_api, ProfileId profile_id)
        : m_attachment(core_api.attach_profile_host(profile_id, *this))
    {
        auto main = core_api.get_profile_world(
            profile_id, PrivateMode::no).lock();
        auto private_world_ptr = core_api.get_profile_world(
            profile_id, PrivateMode::yes).lock();
        auto main_context_menu_bridge = main->context_menu_bridge().lock();
        auto main_clipboard_bridge = main->clipboard_bridge().lock();
        auto private_context_menu_bridge =
            private_world_ptr->context_menu_bridge().lock();
        auto private_clipboard_bridge =
            private_world_ptr->clipboard_bridge().lock();
        m_main_context_menu_host = std::make_unique<GtkContextMenuHost>(
            *main_context_menu_bridge);
        m_main_clipboard_host = create_app_clipboard_host(
            *main_clipboard_bridge);
        m_private_context_menu_host = std::make_unique<GtkContextMenuHost>(
            *private_context_menu_bridge);
        m_private_clipboard_host = create_app_clipboard_host(
            *private_clipboard_bridge);
    }

    GtkContextMenuHost* context_menu_host(PrivateMode private_mode) const
    {
        return private_mode == PrivateMode::yes
            ? m_private_context_menu_host.get()
            : m_main_context_menu_host.get();
    }

private:
    bridge::HostAttachment m_attachment;
    std::unique_ptr<GtkContextMenuHost> m_main_context_menu_host;
    std::unique_ptr<bridge::ClipboardHost> m_main_clipboard_host;
    std::unique_ptr<GtkContextMenuHost> m_private_context_menu_host;
    std::unique_ptr<bridge::ClipboardHost> m_private_clipboard_host;
};

} // namespace

std::shared_ptr<bridge::ProfileHost>
create_app_profile_host(CoreApi& core_api, ProfileId profile_id)
{
    return std::make_shared<GtkProfileHost>(core_api, profile_id);
}

GtkContextMenuHost*
app_context_menu_host(
    bridge::ProfileHost& profile_host, PrivateMode private_mode)
{
    return static_cast<GtkProfileHost&>(profile_host)
        .context_menu_host(private_mode);
}

#else

namespace {

class AppProfileHost final : public bridge::ProfileHost {
public:
    AppProfileHost(CoreApi& core_api, ProfileId profile_id)
        : m_attachment(core_api.attach_profile_host(profile_id, *this)) {}

private:
    bridge::HostAttachment m_attachment;
};

} // namespace

std::shared_ptr<bridge::ProfileHost>
create_app_profile_host(CoreApi& core_api, ProfileId profile_id)
{
    return std::make_shared<AppProfileHost>(core_api, profile_id);
}

GtkContextMenuHost*
app_context_menu_host(bridge::ProfileHost&, PrivateMode)
{
    return nullptr;
}

#endif

} // namespace orion
