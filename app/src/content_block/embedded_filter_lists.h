#pragma once

#include <memory>
#include <string>
#include <vector>

#include <core_module/core/dto/profile_dto.h>
#include <core_module/core/filter_list_metadata.h>

#include "webextensions/webview_factory_host.h"
#include "content_block/filter_store.h"

namespace orion::contentblock {

using FilterListMetadata = orion::filter_lists::Metadata;

// Reconciles local filter state. URL-backed lists are also downloaded when
// download_remote_filter_lists is true (for example, after saving Manage Filter Lists).
void setup_filter_lists(
    WebViewFactory* webview_factory, bool download_remote_filter_lists = false);
// Downloads URL-backed lists on an explicit request. force_update bypasses version
// checks, while unchanged content is still not recompiled.
void setup_filter_lists_from_urls(
    WebViewFactory* webview_factory,
    const std::vector<orion::FilterListDto>& filter_lists,
    bool force_update = false);
void free_embedded_filter_lists(WebViewFactory* webview_factory);
void remove_filter_list_from_webkit_store(
    WebViewFactory* webview_factory, const std::string& filter_id, orion::Fn cb);
const std::vector<FilterListMetadata>& get_metadata_list();

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
void reload_webview_filters_using_setting_value_with_cb(
    WebViewFactory* webview_factory, WebKitWebView* webview, bool enable_content_blockers, std::function<void()> cb
);
void disable_webview_filters(WebKitWebView* webview);
void reload_block_web_fonts_webview_filters_using_website_setting(
    WebViewFactory* webview_factory, WebKitWebView* webview
);
#endif

} // namespace orion::contentblock
