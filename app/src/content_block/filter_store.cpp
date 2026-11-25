#define ENABLE_VERBOSE_LOGS 0

#include "filter_store.h"

#include <common/src/defer.h>
#include <core_module/core/core_api.h>
#include <core/downloader.h>
#include <common/src/string_util.h>
#include "app.h"

#include <algorithm>
#include <map>

namespace orion::contentblock {

static std::map<WebViewFactory*, std::vector<std::weak_ptr<orion::Downloader>>>
    filter_list_downloaders;
static std::map<WebViewFactory*, size_t> filter_list_sync_counts;
static orion::Observable<FilterListsSyncChange> global_filter_lists_sync_change;

static void
cancel_filter_list_downloaders(WebViewFactory* webview_factory)
{
    auto it = filter_list_downloaders.find(webview_factory);
    if (it == filter_list_downloaders.end()) return;

    for (const std::weak_ptr<orion::Downloader>& downloader_weak : it->second) {
        auto downloader = downloader_weak.lock();
        if (downloader) downloader->cancel();
    }
}

void
filter_list_download_started(
    WebViewFactory* webview_factory, const std::shared_ptr<orion::Downloader>& downloader)
{
    filter_list_downloaders[webview_factory].push_back(downloader);
    size_t& sync_count = filter_list_sync_counts[webview_factory];
    if (sync_count++ == 0) {
        global_filter_lists_sync_change.notify({
            .profile_id = webview_factory->profile_id(),
            .private_mode = webview_factory->private_mode(),
            .is_syncing = true,
        });
    }
    filter_store_callback_started(webview_factory);
}

void
filter_list_download_finished(
    WebViewFactory* webview_factory, const std::shared_ptr<orion::Downloader>& downloader)
{
    auto it = filter_list_downloaders.find(webview_factory);
    if (it != filter_list_downloaders.end()) {
        std::erase_if(
            it->second,
            [&downloader](const std::weak_ptr<orion::Downloader>& candidate) {
                auto candidate_strong = candidate.lock();
                return !candidate_strong || candidate_strong == downloader;
            });
        if (it->second.empty()) filter_list_downloaders.erase(it);
    }
}

void
filter_list_sync_finished(WebViewFactory* webview_factory)
{
    auto it = filter_list_sync_counts.find(webview_factory);
    if (it == filter_list_sync_counts.end() || it->second == 0) {
        orion::log_warning(
            "%s: no active filter list sync for profile_id=%" PRId64,
            __func__, webview_factory->profile_id().value);
        return;
    }

    FilterListsSyncChange change{
        .profile_id = webview_factory->profile_id(),
        .private_mode = webview_factory->private_mode(),
        .is_syncing = false,
    };
    filter_store_callback_finished(webview_factory);
    if (--it->second != 0) return;

    filter_list_sync_counts.erase(it);
    global_filter_lists_sync_change.notify(change);
}

bool
filter_lists_are_syncing(WebViewFactory* webview_factory)
{
    auto it = filter_list_sync_counts.find(webview_factory);
    return it != filter_list_sync_counts.end() && it->second != 0;
}

orion::Observable<FilterListsSyncChange>&
filter_lists_sync_change()
{
    return global_filter_lists_sync_change;
}

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT

WebKitUserContentFilterStore*
get_or_create_filter_store(WebViewFactory* webview_factory)
{
    if (webview_factory->filter_store()->webkit_store() == nullptr) {
        std::vector<std::string> path = WebExtensionWebViewFactory::get_profile_path_parts(
            webview_factory->profile_id(),
            orion::PrivateMode::no // share the same filter store for both main and private modes
        );
        path.push_back("filter_lists");

        WebKitUserContentFilterStore* store = webkit_user_content_filter_store_new(
            orion::str::join_strings(path, G_DIR_SEPARATOR_S).c_str()
        );
        webview_factory->filter_store()->set_webkit_store((OpaqueWebKitStore*)store);
        return store;
    }

    return (WebKitUserContentFilterStore*)webview_factory->filter_store()->webkit_store();
}

#endif // defined(ENABLE_WEBKIT) && ENABLE_WEBKIT

std::shared_ptr<orion::StopCancellable>
get_filter_store_cancellable(WebViewFactory* webview_factory)
{
    if (!webview_factory->filter_store()->stop_cancellable()) {
        webview_factory->filter_store()->set_stop_cancellable(orion::StopCancellable::create());
    }
    return webview_factory->filter_store()->stop_cancellable();
}

// Must be called when setting up new async WebKitUserContentFilterStore operation
void
filter_store_callback_started(WebViewFactory* webview_factory)
{
    webview_factory->filter_store()->add_pending_ops();

    orion::log_verbose(
        "filter_store_callback_started: pending_ops incremented for profile_id=%" PRId64 " pending_ops=%d",
        webview_factory->profile_id().value,
        webview_factory->filter_store()->pending_ops()
    );
}

static void
call_and_clear_all_cancelled_cbs(WebViewFactory* webview_factory)
{
    if (webview_factory->filter_store()->cancelled_cbs().empty()) {
        orion::log_verbose(
            "call_and_clear_all_cancelled_cbs: pending_ops is zero but zero cancelled_cbs are set for profile_id=%" PRId64,
            webview_factory->profile_id().value
        );
    } else {
        // Call all registered cancelled callbacks now that the pending ops have reached zero
        orion::log_verbose(
            "call_and_clear_all_cancelled_cbs: pending_ops is zero and cancelled_cb is set, calling %zu cancelled_cbs for profile_id=%" PRId64,
            webview_factory->filter_store()->cancelled_cbs().size(),
            webview_factory->profile_id().value
        );

        for (const auto& cb : webview_factory->filter_store()->cancelled_cbs()) {
            // But never call the callback synchronously
            global_core_api->async([cb=std::move(cb),profile_id=webview_factory->profile_id()]() mutable {
                cb();
            });
        }

        webview_factory->filter_store()->clear_cancelled_cbs();
    }
}

// Must be called when any WebKitUserContentFilterStore's GAsyncReadyCallback operation (load, save etc.) is finished.
// Otherwise, the app might hang on quit.
void
filter_store_callback_finished(WebViewFactory* webview_factory)
{
    if (webview_factory->filter_store()->pending_ops() <= 0) {
        orion::log_warning(
            "filter_store_callback_finished: pending_ops is already zero for profile_id=%" PRId64,
            webview_factory->profile_id().value
        );
        return;
    }
    webview_factory->filter_store()->remove_pending_ops();

    orion::log_verbose(
        "filter_store_callback_finished: pending_ops decremented for profile_id=%" PRId64 " pending_ops=%d",
        webview_factory->profile_id().value,
        webview_factory->filter_store()->pending_ops()
    );

    if (webview_factory->filter_store()->pending_ops() <= 0) {
        call_and_clear_all_cancelled_cbs(webview_factory);
    }
}

// Must be called only when no GAsyncReadyCallback (of each individual store) is pending because we must not
// unref the store if these callbacks (save, load etc.) are still pending
void
free_content_filter_store(WebViewFactory* webview_factory)
{
    orion::log_verbose(
        "free_content_filter_store: freeing content filter store for profile_id=%lld webview_factory=%p",
        webview_factory->profile_id().value, webview_factory
    );

    const auto& filter_store = webview_factory->filter_store();
    filter_store->set_stop_cancellable(nullptr);

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    if(auto* store = filter_store->webkit_store()) {
        g_object_unref(store);
        filter_store->set_webkit_store(nullptr);
    }
#endif // defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
}

// Ignores on_cancelled_cb if it is nullptr. Note that multiple calls to this function overrides the previous
// callback that may never be called if any operations are still pending.
void
cancel_all_content_filter_store_ops(WebViewFactory* webview_factory, std::function<void()> on_cancelled_cb)
{
    const auto& filter_store = webview_factory->filter_store();
    
    if (filter_store->is_cancelling()) {
        orion::log_verbose(
            "cancel_all_content_filter_store_ops: already cancelling for profile_id=%lld",
            webview_factory->profile_id().value
        );
    } else {
        orion::log_verbose(
            "cancel_all_content_filter_store_ops: cancelling all pending operations for profile_id=%lld pending_ops=%d",
            webview_factory->profile_id().value, filter_store->pending_ops()
        );
        filter_store->set_is_cancelling(true);
    }

    // Ensures that the cancellable is created in webkit-less build
    [[maybe_unused]] auto cancellable = get_filter_store_cancellable(webview_factory);

    if (filter_store->stop_cancellable()) {
        if (on_cancelled_cb) {
            orion::log_verbose(
                "cancel_all_content_filter_store_ops: registering cancelled_cb for profile_id=%lld",
                webview_factory->profile_id().value
            );
            filter_store->add_cancelled_cb(std::move(on_cancelled_cb));
        }
        filter_store->stop_cancellable()->cancel();
    }
    cancel_filter_list_downloaders(webview_factory);

    if (filter_store->pending_ops() <= 0) {
        call_and_clear_all_cancelled_cbs(webview_factory);
    }
}

} // namespace orion::contentblock
