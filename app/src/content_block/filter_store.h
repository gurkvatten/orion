#pragma once

#include <memory>

#include "webextensions/webview_factory_host.h"
#include "util/stop_cancellable.h"

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
#include <webkit/webkit.h>
#endif

namespace orion {
class Downloader;
}

namespace orion::contentblock {

using WebViewFactory = WebExtensions::WebExtensionWebViewFactory;

struct FilterListsSyncChange {
    orion::ProfileId profile_id{};
    orion::PrivateMode private_mode{};
    bool is_syncing{};
};

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT

WebKitUserContentFilterStore* get_or_create_filter_store(WebViewFactory* webview_factory);

std::shared_ptr<orion::StopCancellable> get_filter_store_cancellable(WebViewFactory* webview_factory);

#endif // defined(ENABLE_WEBKIT) && ENABLE_WEBKIT

// Must be called when setting up new async WebKitUserContentFilterStore operation
void filter_store_callback_started(WebViewFactory* webview_factory);

// Must be called when any WebKitUserContentFilterStore's GAsyncReadyCallback operation (load, save etc.) is finished.
// Otherwise, the app might hang on quit.
void filter_store_callback_finished(WebViewFactory* webview_factory);

void filter_list_download_started(
    WebViewFactory* webview_factory, const std::shared_ptr<orion::Downloader>& downloader);
void filter_list_download_finished(
    WebViewFactory* webview_factory, const std::shared_ptr<orion::Downloader>& downloader);
void filter_list_sync_finished(WebViewFactory* webview_factory);
bool filter_lists_are_syncing(WebViewFactory* webview_factory);
orion::Observable<FilterListsSyncChange>& filter_lists_sync_change();

// Must be called only when no GAsyncReadyCallback (of each individual store) is pending because we must not
// unref the store if these callbacks (save, load etc.) are still pending
void free_content_filter_store(WebViewFactory* webview_factory);

// Ignores on_cancelled_cb if it is nullptr. Note that multiple calls to this function overrides the previous
// callback that may never be called if any operations are still pending.
void cancel_all_content_filter_store_ops(WebViewFactory* webview_factory, std::function<void()> on_cancelled_cb);

} // namespace orion::contentblock
