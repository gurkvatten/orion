#pragma once

#include <core/webkit_bridge/API/bridge.h>

#include <memory>

namespace orion {

class CoreApi;
class GtkContextMenuHost;

std::shared_ptr<bridge::ProfileHost> create_app_profile_host(
    CoreApi& core_api, ProfileId profile_id);
GtkContextMenuHost* app_context_menu_host(
    bridge::ProfileHost& profile_host, PrivateMode private_mode);

} // namespace orion
