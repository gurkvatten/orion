#pragma once

#include <core/webkit_bridge/API/bridge.h>

#include <memory>

class OpaqueWebView;

namespace orion {

std::unique_ptr<bridge::WebViewHost> create_app_web_view_host(
    bridge::WebViewBridge& web_view_bridge, OpaqueWebView* web_view);
std::weak_ptr<bridge::WebViewBridge> app_web_view_bridge(
    OpaqueWebView* web_view);

} // namespace orion
