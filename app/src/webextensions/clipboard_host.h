#pragma once

#include <core/webkit_bridge/API/bridge.h>

#include <memory>

namespace orion {

std::unique_ptr<bridge::ClipboardHost> create_app_clipboard_host(
    bridge::ClipboardBridge& bridge);

} // namespace orion
