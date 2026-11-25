#define ENABLE_VERBOSE_LOGS 0
#define PRINT_JSON_FILTERS 0

// TODO: rename this file to filter_lists.cpp or something similar

#include "embedded_filter_lists.h"

#include <core_module/core/core_api.h>
#include <common/src/crypto.h>
#include <common/src/log.h>
#include <common/src/defer.h>
#include <common/src/string_util.h>
#include <common/src/time_util.h>
#include <common/src/uuid.h>
#include <core/downloader.h>

#include <filesystem>

#include "util/ref_util.h"
#include "util/file_util.h"
#include "content_block/filter_store.h"
#include "webextensions/webview_factory_host.h"
#include "app.h"
#include <core_module/core/settings.h>
#include "window/orion_webview_tabview_widgets/orion_webkit_webview.h"
#include "window/orion_webview_tabview_widgets/orion_webview_tab_content.h"

namespace orion::contentblock {

static const std::vector<FilterListMetadata> global_metadatas = orion::filter_lists::get_metadata_list();

const std::vector<FilterListMetadata>&
get_metadata_list()
{
    return global_metadatas;
}

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT

namespace {
    static std::shared_ptr<WebViewFactory>
    web_view_factory_for(orion::bridge::ProfileWorld& profile_world)
    {
        return orion_app_get_web_view_factory(profile_world).lock();
    }

    struct FilterStoreIdsReply {
        orion::Cb<std::vector<std::string>> cb{};
        std::weak_ptr<orion::bridge::ProfileWorld> profile_world{};
    };

    struct FilterStoreFiltersReply {
        orion::Cb<std::vector<WebKitUserContentFilter*>> cb{};
        std::weak_ptr<orion::bridge::ProfileWorld> profile_world{};
        size_t callback_counter{};
        size_t expected_filter_count{};
        std::vector<WebKitUserContentFilter*> filters{};
    };
}

static void setup_filter_lists_impl(
    WebViewFactory* webview_factory,
    const std::vector<FilterListMetadata>& metadatas,
    std::function<void()> completion = {},
    bool force_update = false,
    bool include_custom_filter_lists = false);

enum class FilterType : int64_t {
    normal = 0,
    block_web_fonts = 1,
};

namespace {
    struct FilterDetail {
        FilterType filter_type{};
        std::string website{}; // relevant for block_web_fonts filter type, empty for normal filter type
    };
}

static std::string
content_filter_block_web_fonts_id_for_website(const std::string& website)
{
    return std::format("settings::website::specific::{}::block_web_fonts", website);
}

static const std::regex&
content_filter_block_web_fonts_regex()
{
    // For example: settings::website::specific::WEBSITE::block_web_fonts
    static std::regex re(R"(^settings::website::specific::(.+)::block_web_fonts$)");
    return re;
}

static bool
filter_id_is_block_web_fonts_filter(const std::string& filter_id)
{
    return std::regex_match(filter_id, content_filter_block_web_fonts_regex());
}

static FilterDetail
content_filter_get_detail(WebKitUserContentFilter* filter)
{
    const char* id = webkit_user_content_filter_get_identifier(filter);
    if (!id) return FilterDetail{.filter_type = FilterType::normal};
    std::string name(id);
    std::smatch match;
    if (std::regex_match(name, match, content_filter_block_web_fonts_regex())) {
        if (match.size() == 2) {
            return FilterDetail{
                .filter_type = FilterType::block_web_fonts,
                .website = match[1].str(),
            };
        }
    }
    return FilterDetail{.filter_type = FilterType::normal};
}

// Returns 0 tab ID if the webview is not associated with any tab
static orion::TabId
tab_id_for_webview(WebKitWebView* webview)
{
    OrionWebkitWebview* orion_webview = ORION_WEBKIT_WEBVIEW(webview);
    OrionWebviewTabContent* content = ORION_WEBVIEW_TAB_CONTENT(
        gtk_widget_get_ancestor(GTK_WIDGET(orion_webview), ORION_WEBVIEW_TAB_CONTENT_TYPE));
    orion::TabId tab_id{};
    if (content) {
        tab_id = orion_webview_tab_content_get_id(content);
    }
    return tab_id;
}

static void
fetch_filter_store_ids_cb(GObject* source_object, GAsyncResult* result, gpointer user_data)
{
    FilterStoreIdsReply* reply_data = static_cast<FilterStoreIdsReply*>(user_data);
    defer([reply_data]{ delete reply_data; });

    auto profile_world = reply_data->profile_world.lock();
    if (!profile_world) return;
    auto factory_owner = web_view_factory_for(*profile_world);
    auto* webview_factory = factory_owner.get();
    if(!webview_factory) return;
    defer([webview_factory] {
        filter_store_callback_finished(webview_factory);
    });

    WebKitUserContentFilterStore* store = WEBKIT_USER_CONTENT_FILTER_STORE(source_object);
    char** identifiers = webkit_user_content_filter_store_fetch_identifiers_finish(store, result);
    if (!identifiers) {
        orion::log_error("filter_lists: fetch_filter_store_ids_cb: failed to fetch filter store identifiers");
        reply_data->cb({});
        return;
    }
    defer([identifiers]{ g_strfreev(identifiers); });

    std::vector<std::string> ids{};
    for (char** p = identifiers; *p != NULL; ++p) {
        ids.push_back(*p);
    }

    orion::log_verbose(
        "filter_lists: fetch_filter_store_ids_cb: fetched filter store identifiers successfully: count=%zu",
        ids.size());

    reply_data->cb(ids);
}

static void
fetch_saved_filter_ids(WebViewFactory* webview_factory, orion::Cb<std::vector<std::string>> reply)
{
    WebKitUserContentFilterStore* store = get_or_create_filter_store(webview_factory);
    std::shared_ptr<orion::StopCancellable> stop_cancellable = get_filter_store_cancellable(webview_factory);
    GCancellable* cancellable = stop_cancellable->get();
    FilterStoreIdsReply* reply_data = new FilterStoreIdsReply{
        .cb = reply,
        .profile_world = webview_factory->parent_world(),
    };

    filter_store_callback_started(webview_factory);

    webkit_user_content_filter_store_fetch_identifiers(store, cancellable, fetch_filter_store_ids_cb, reply_data);
}

static void
fetch_saved_filters_cb(GObject* source_object, GAsyncResult* result, gpointer user_data)
{
    FilterStoreFiltersReply* reply_data = static_cast<FilterStoreFiltersReply*>(user_data);
    reply_data->callback_counter++;

    defer([reply_data]{
        if (reply_data->callback_counter == reply_data->expected_filter_count) {
            delete reply_data;
        }
    });

    auto profile_world = reply_data->profile_world.lock();
    if (!profile_world) return;
    auto factory_owner = web_view_factory_for(*profile_world);
    auto* webview_factory = factory_owner.get();
    if(!webview_factory) return;
    defer([webview_factory] {
        filter_store_callback_finished(webview_factory);
    });

    WebKitUserContentFilterStore* store = WEBKIT_USER_CONTENT_FILTER_STORE(source_object);
    GError* error = NULL;
    WebKitUserContentFilter* content_filter = webkit_user_content_filter_store_load_finish(store, result, &error);
    if (!content_filter) {
        defer([error]{ g_error_free(error); });
        orion::log_error("filter_lists: fetch_saved_filters_cb: failed to load filter store: %s", error->message);
        return;
    }

    reply_data->filters.push_back(content_filter);
    if (reply_data->callback_counter == reply_data->expected_filter_count) {
        reply_data->cb(reply_data->filters);
    }
}

// The caller of the method should unref each filter in reply callback (webkit_user_content_filter_unref)
static void
fetch_saved_filters(WebViewFactory* webview_factory, orion::Cb<std::vector<WebKitUserContentFilter*>> reply)
{
    auto profile_world = webview_factory->parent_world();
    fetch_saved_filter_ids(webview_factory, [profile_world,reply](std::vector<std::string> filter_ids){
        auto world = profile_world.lock();
        if (!world) return;
        auto factory_owner = web_view_factory_for(*world);
        auto* webview_factory = factory_owner.get();
        if(!webview_factory) return;
        FilterStoreFiltersReply* reply_data = new FilterStoreFiltersReply{
            .cb = reply,
            .profile_world = profile_world,
            .expected_filter_count = filter_ids.size(),
        };

        for (const std::string& filter_id : filter_ids) {
            WebKitUserContentFilterStore* store = get_or_create_filter_store(webview_factory);
            std::shared_ptr<orion::StopCancellable> stop_cancellable = get_filter_store_cancellable(webview_factory);
            GCancellable* cancellable = stop_cancellable->get();

            filter_store_callback_started(webview_factory);

            // Load must be called with the same reply_data object for all filters
            webkit_user_content_filter_store_load(
                store,
                filter_id.c_str(),
                cancellable,
                fetch_saved_filters_cb,
                reply_data
            );
        }
    });
}

namespace {
    struct RemoveFilterData {
        std::weak_ptr<orion::bridge::ProfileWorld> profile_world{};
        std::string filter_id{};
        orion::Fn done_cb{};
    };
};

static void
remove_filter_from_webviews(WebViewFactory* webview_factory, const std::string& filter_id)
{
    for (const orion::gtk::WeakRefWrapper& webview_weak : webview_factory->web_views()) {
        auto webview_strong = webview_weak.lock();
        if (!webview_strong) continue;

        WebKitUserContentManager* manager = webkit_web_view_get_user_content_manager(
            WEBKIT_WEB_VIEW(webview_strong.get()));
        webkit_user_content_manager_remove_filter_by_id(manager, filter_id.c_str());
    }
}

static void
remove_filter_cb(GObject* source_object, GAsyncResult* result, gpointer user_data)
{
    RemoveFilterData* data = static_cast<RemoveFilterData*>(user_data);
    defer([data]{ delete data; });

    orion::log_verbose("filter_lists: remove_filter_cb: filter_id='%s'", data->filter_id.c_str());

    auto profile_world = data->profile_world.lock();
    if (!profile_world) return;
    auto factory_owner = web_view_factory_for(*profile_world);
    auto* webview_factory = factory_owner.get();
    if(!webview_factory) return;
    defer([webview_factory]{
        filter_store_callback_finished(webview_factory);
    });

    GError* error = NULL;
    WebKitUserContentFilterStore* store = WEBKIT_USER_CONTENT_FILTER_STORE(source_object);
    gboolean success = webkit_user_content_filter_store_remove_finish(store, result, &error);
    if (!success) {
        defer([error]{ g_error_free(error); });
        orion::log_error("filter_lists: remove_filter_cb: failed to remove filter from store: %s", error->message);
        if (data->done_cb) data->done_cb();
        return;
    }

    // A web view may have loaded the saved filter while the asynchronous store
    // removal was in flight. Detach it again after the store no longer exposes it.
    remove_filter_from_webviews(webview_factory, data->filter_id);
    orion::log_debug(
        "filter_lists: remove_filter_cb: filter removed from webkit store: name='%s'",
        data->filter_id.c_str());
    if (data->done_cb) data->done_cb();
}

static void
remove_filter_from_filter_store_in_factory(WebViewFactory* webview_factory, const char* filter_id, orion::Fn cb)
{
    orion::log_verbose("filter_lists: remove_filter_from_filter_store: filter_id='%s'", filter_id);
    remove_filter_from_webviews(webview_factory, filter_id);

    WebKitUserContentFilterStore* store = get_or_create_filter_store(webview_factory);
    std::shared_ptr<orion::StopCancellable> stop_cancellable = get_filter_store_cancellable(webview_factory);
    GCancellable* cancellable = stop_cancellable->get();

    RemoveFilterData* data = new RemoveFilterData{
        .profile_world = webview_factory->parent_world(),
        .filter_id = filter_id,
        .done_cb = cb,
    };

    filter_store_callback_started(webview_factory);
    webkit_user_content_filter_store_remove(store, filter_id, cancellable, remove_filter_cb, data);
}

void
remove_filter_list_from_webkit_store(
    WebViewFactory* webview_factory, const std::string& filter_id, orion::Fn cb)
{
    if (webview_factory->filter_store()->is_cancelling()) {
        orion::log_debug(
            "filter_lists: remove_filter_list_from_webkit_store: filter store is cancelling, skipping removal");
        if (cb) cb();
        return;
    }

    remove_filter_from_filter_store_in_factory(webview_factory, filter_id.c_str(), cb);
}

// If website is specified, only return webviews that are currently displaying that website
static std::vector<orion::gtk::StrongRefWrapper>
get_strong_webviews_in_profile(orion::ProfileId profile_id, std::optional<std::string> website)
{
    std::vector<std::weak_ptr<orion::bridge::ProfileWorld>> profile_worlds{
        global_core_api->get_profile_world(profile_id, orion::PrivateMode::no),
        global_core_api->get_profile_world(profile_id, orion::PrivateMode::yes),
    };
    std::vector<orion::gtk::StrongRefWrapper> alive_webviews;

    for (const auto& profile_world_weak : profile_worlds) {
        auto profile_world = profile_world_weak.lock();
        if (!profile_world) continue;
        auto factory = web_view_factory_for(*profile_world.get());
        if (!factory) continue;

        for (const orion::gtk::WeakRefWrapper& webview_weak : factory->web_views()) {
            auto webview_strong = webview_weak.lock();
            if (!webview_strong) continue;
            WebKitWebView* webview = WEBKIT_WEB_VIEW(webview_strong.get());

            // Filter by website (conditionally)
            if (website.has_value()) {
                if (!ORION_IS_WEBKIT_WEBVIEW(webview)) continue;
                OrionWebkitWebview* orion_webview = ORION_WEBKIT_WEBVIEW(webview);
                auto current_website = orion_webkit_webview_get_current_website(orion_webview);
                if (current_website != website.value()) continue;
            }

            orion::gtk::StrongRefWrapper strong_ref(webview);
            alive_webviews.push_back(strong_ref);
        }
    }
    return alive_webviews;
}

static void
reload_content_filter_in_webview(WebKitWebView* webview, WebKitUserContentFilter* content_filter)
{
    WebKitUserContentManager* manager = webkit_web_view_get_user_content_manager(webview);
    const char* filter_id = webkit_user_content_filter_get_identifier(content_filter);
    webkit_user_content_manager_remove_filter_by_id(manager, filter_id);
    webkit_user_content_manager_add_filter(manager, content_filter);
}

// Reloads the filter in all webviews of both normal and private windows
static void
reload_content_filter_in_profile(orion::ProfileId profile_id, WebKitUserContentFilter* content_filter)
{
    auto strong_webviews = get_strong_webviews_in_profile(profile_id, {});
    for (const auto& strong_ref : strong_webviews) {
        WebKitWebView* webview = WEBKIT_WEB_VIEW(strong_ref.get());
        reload_content_filter_in_webview(webview, content_filter);
    }
}

static void
reload_website_webviews_in_profile(orion::ProfileId profile_id, const std::string& website)
{
    auto strong_webviews = get_strong_webviews_in_profile(profile_id, website);
    orion::log_verbose(
        "filter_lists: reload_website_webviews_in_profile: website=%s webviews=%zu",
        website.c_str(), strong_webviews.size());
    for (const auto& strong_ref : strong_webviews) {
        OrionWebkitWebview* webview = ORION_WEBKIT_WEBVIEW(strong_ref.get());
        webkit_web_view_reload(WEBKIT_WEB_VIEW(webview));
    }
}

// Not needed at the moment, use remove_filter_from_filter_stores_in_profile instead (the filter id already
// implies the website)
static void
remove_filter_from_website_webviews_in_profile(
    const std::string& filter_id,
    orion::ProfileId profile_id,
    const std::string& website)
{
    auto strong_webviews = get_strong_webviews_in_profile(profile_id, website);
    orion::log_verbose(
        "filter_lists: remove_filter_from_website_webviews_in_profile: filter_id=%s website=%s webviews=%zu",
        filter_id.c_str(), website.c_str(), strong_webviews.size());
    for (const auto& strong_ref : strong_webviews) {
        OrionWebkitWebview* webview = ORION_WEBKIT_WEBVIEW(strong_ref.get());
        WebKitUserContentManager* manager = webkit_web_view_get_user_content_manager(WEBKIT_WEB_VIEW(webview));
        webkit_user_content_manager_remove_filter_by_id(manager, filter_id.c_str());
    }
}

static void
remove_filter_from_filter_stores_in_profile(
    const std::string& filter_id,
    orion::ProfileId profile_id,
    orion::Fn cb)
{
    orion::log_verbose(
        "filter_lists: remove_filter_from_filter_stores_in_profile: filter_id=%s profile_id=%lld",
        filter_id.c_str(), profile_id.value);
    std::vector<std::weak_ptr<orion::bridge::ProfileWorld>> profile_worlds{
        global_core_api->get_profile_world(profile_id, orion::PrivateMode::no),
        global_core_api->get_profile_world(profile_id, orion::PrivateMode::yes),
    };
    std::shared_ptr<int> remaining = std::make_shared<int>(profile_worlds.size());
    for (const auto& weak_world : profile_worlds) {
        auto world = weak_world.lock();
        if (!world) {
            if (--(*remaining) == 0 && cb) cb();
            continue;
        }
        auto factory = web_view_factory_for(*world);
        if (!factory) {
            if (--(*remaining) == 0 && cb) cb();
            continue;
        }
        // Is factory lifetime guaranteed here?
        remove_filter_from_filter_store_in_factory(factory.get(), filter_id.c_str(), [remaining,cb]{
            if (--(*remaining) == 0 && cb) cb();
        });
    }
}

namespace {
    struct SetupFilterListsState {
        size_t remaining{};
        std::function<void()> completion{};

        void
        finish_one()
        {
            if (remaining == 0 || --remaining != 0) return;
            if (completion) completion();
        }

        void
        finish_all()
        {
            if (remaining == 0) return;
            remaining = 0;
            if (completion) completion();
        }
    };

    struct FilterStoreSaveData {
        std::string version;
        std::chrono::steady_clock::time_point started_at;
        std::weak_ptr<orion::bridge::ProfileWorld> profile_world{};
        bool is_website_specific{};
        int64_t position{};
        std::string url;
        std::string display_name;
        std::string description;
        bool is_enabled{};
        int64_t active_rules_count{};
        orion::FilterListType filter_type{};
        std::string custom_filter_data;
        std::string content_hash;
        std::function<void()> completion{};
    };
}

// Called when WebKit filter store save operation is completed
static void
filter_store_save_ready_cb(GObject* source_object, GAsyncResult* result, gpointer data)
{
    FilterStoreSaveData* save_data = static_cast<FilterStoreSaveData*>(data);
    std::chrono::steady_clock::time_point started_at = save_data->started_at;
    std::function<void()> completion = std::move(save_data->completion);
    bool completion_handed_off = false;
    defer([&completion,&completion_handed_off] {
        if (!completion_handed_off && completion) completion();
    });
    defer([save_data]{ delete save_data; });

    auto profile_world = save_data->profile_world.lock();
    if (!profile_world) return;
    auto factory_owner = web_view_factory_for(*profile_world);
    auto* webview_factory = factory_owner.get();
    if(!webview_factory) return;
    auto profile_id = profile_world->profile_id();

    defer([webview_factory]{
        // This must be called when leaving this callback
        filter_store_callback_finished(webview_factory);
    });

    WebKitUserContentFilterStore* store = WEBKIT_USER_CONTENT_FILTER_STORE(source_object);    
    GError* error = NULL;
    WebKitUserContentFilter* content_filter = webkit_user_content_filter_store_save_finish(store, result, &error);
    if (!content_filter) {
        defer([error]{ g_error_free(error); });
        if (g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
            // This error is expected if the operation was cancelled
            orion::log_info("filter_lists: filter_store_save_ready_cb: filter store save operation was cancelled");
            return;
        }
        
        orion::log_error("filter_lists: filter_store_save_ready_cb: failed to save filter store: %s", error->message);
        return;
    }

    const char* id = webkit_user_content_filter_get_identifier(content_filter);
    if (!id) {
        orion::log_error("filter_lists: filter_store_save_ready_cb: saved filter has no identifier");
        return;
    }

    std::string name(id);

    orion::CreateFilterListQuery query{
        .profile_id = profile_id,
        .name = name,
        .version = save_data->version,
        .position = save_data->position,
        .url = save_data->url,
        .display_name = save_data->display_name,
        .description = save_data->description,
        .is_enabled = save_data->is_enabled,
        .updated_at = orion::tm::unix_time_utc64_now(),
        .active_rules_count = save_data->active_rules_count,
        .filter_type = save_data->filter_type,
        .custom_filter_data = save_data->custom_filter_data,
        .content_hash = save_data->content_hash,
        // These fields describe the successfully compiled and saved filter.
        .update_is_enabled = true,
        .update_updated_at = true,
        .update_active_rules_count = true,
        .update_version = true,
        .update_content_hash = true,
    };

    bool is_website_specific = save_data->is_website_specific;

    webkit_user_content_filter_ref(content_filter);

    std::weak_ptr<orion::bridge::ProfileWorld> weak_profile_world = profile_world;
    global_core_api->create_or_update_filter_list(query,
    [weak_profile_world,name,content_filter,started_at,query,is_website_specific,profile_id,
     completion=std::move(completion)](orion::CreateFilterListReply reply) mutable {
        defer([content_filter]{ webkit_user_content_filter_unref(content_filter); });
        defer([&completion] {
            if (completion) completion();
        });
        auto profile_world = weak_profile_world.lock();
        if (!profile_world) return;
        VERIFY_LOG(reply.err);
        auto factory_owner = web_view_factory_for(*profile_world);
        auto* webview_factory = factory_owner.get();
        if(!webview_factory) return;

        size_t reloaded_webviews = get_strong_webviews_in_profile(profile_id, {}).size();

        orion::log_by_level(
            is_website_specific ? orion::LogLevel::verbose : orion::LogLevel::debug,
            "filter_lists: filter_store_save_ready_cb: filter list saved, reloading filter in existing webviews: name='%s' took=%lldms reloaded_webviews=%zu",
            name.c_str(),
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started_at).count(),
            reloaded_webviews);
        
        // We want to reload the filter in all webviews associated with this profile (in both normal
        // and private webview factories)
        reload_content_filter_in_profile(profile_id, content_filter);
    });
    completion_handed_off = true;
}

static std::string
normalize_filter_list_version(std::string s)
{
    orion::str::trim(&s);
    orion::str::to_lower(&s);

    // Version format examples:
    //   2026.0310.1015.27
    //   2026.0310.1032.15
    //   202603101504
    //   23 December 2025
    //   202603101443
    //   202603101100
    //   26 December 2025
    //   20260730
    std::smatch match{};
    std::regex re(R"((\d+) ([a-zA-Z]+) (\d{4}))");
    if (!std::regex_match(s, match, re)) {
        return s;
    }
    std::string day = match[1].str();
    
    if (day.length() == 1) {
        day = "0" + day;
    }

    std::string year = match[3].str();
    std::string month_name = match[2].str();
    std::string month = "";

    if (month_name == "january" || month_name == "jan") {
        month = "01";
    } else if (month_name == "february" || month_name == "feb") {
        month = "02";
    } else if (month_name == "march" || month_name == "mar") {
        month = "03";
    } else if (month_name == "april" || month_name == "apr") {
        month = "04";
    } else if (month_name == "may") {
        month = "05";
    } else if (month_name == "june" || month_name == "jun") {
        month = "06";
    } else if (month_name == "july" || month_name == "jul") {
        month = "07";
    } else if (month_name == "august" || month_name == "aug") {
        month = "08";
    } else if (month_name == "september" || month_name == "sep" || month_name == "sept") {
        month = "09";
    } else if (month_name == "october" || month_name == "oct") {
        month = "10";
    } else if (month_name == "november" || month_name == "nov") {
        month = "11";
    } else if (month_name == "december" || month_name == "dec") {
        month = "12";
    } else {
        return s;
    }
    return year + "-" + month + "-" + day;
}

static bool
filter_list_should_update(
    const std::string& version_a,
    const std::string& version_b,
    const std::string& content_hash_a,
    const std::string& content_hash_b)
{
    // Some filter lists do not provide a version. In that case, differing content
    // means the installed filter list needs to be updated.
    if (version_a.empty() || version_b.empty()) {
        return content_hash_a != content_hash_b;
    }

    std::string va = normalize_filter_list_version(version_a);
    std::string vb = normalize_filter_list_version(version_b);
    if (va == vb) {
        return content_hash_a != content_hash_b;
    }

    const auto version_to_unix = [](const std::string& version) -> int64_t {
        std::smatch match{};

        if (std::regex_match(version, match, std::regex(R"((\d{4})-(\d{2})-(\d{2}))"))) {
            const std::chrono::year_month_day date{
                std::chrono::year{std::stoi(match[1].str())},
                std::chrono::month{static_cast<unsigned int>(std::stoul(match[2].str()))},
                std::chrono::day{static_cast<unsigned int>(std::stoul(match[3].str()))},
            };
            if (!date.ok()) return 0;

            return std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::sys_days{date}.time_since_epoch()).count();
        }

        if (std::regex_match(version, std::regex(R"(\d{10})"))) {
            return std::stoll(version);
        }

        return 0;
    };

    // A versionless downloaded list uses its download time as the version. Convert
    // known date versions too so they can be compared with that Unix timestamp.
    int64_t unix_a = version_to_unix(va);
    int64_t unix_b = version_to_unix(vb);
    if (unix_a != 0 && unix_b != 0) {
        return unix_a < unix_b;
    }

    return va < vb;
}

static void
remove_unknown_or_disabled_webkit_filters(
    WebViewFactory* webview_factory,
    std::vector<std::string> saved_ids,
    std::vector<std::string> known_filter_ids)
{
    std::set<std::string> unknown_ids(saved_ids.begin(), saved_ids.end());

    // Erase all known filter ids from the set
    for (const std::string& id : known_filter_ids) {
        unknown_ids.erase(id);
    }

    // Erase all block-web-fonts filters
    for (const std::string& id : saved_ids) {
        if (filter_id_is_block_web_fonts_filter(id)) {
            unknown_ids.erase(id);
        }
    }

    if (unknown_ids.empty()) {
        return;
    }

    orion::log_info(
        "filter_lists: remove_unknown_or_disabled_webkit_filters: removing disabled/unknown filters: count=%zu",
        unknown_ids.size());

    // The remaining filter ids are unknown
    for (const std::string& id : unknown_ids) {
        remove_filter_from_filter_store_in_factory(webview_factory, id.c_str(), nullptr);
    }
}

void
free_embedded_filter_lists(WebViewFactory* webview_factory)
{
    orion::log_verbose(
        "filter_lists: free_embedded_filter_lists: freeing embedded filter lists: profile_id=%lld webview_factory=%p",
        webview_factory->profile_id().value, webview_factory
    );

    orion::contentblock::free_content_filter_store(webview_factory);
    
    orion::ObserverId observer = webview_factory->web_views_observer();
    webview_factory->web_views_change().clear_observer(&observer);
    webview_factory->set_web_views_observer(observer);
}

void
disable_webview_filters(WebKitWebView* webview)
{
    WebKitUserContentManager* manager = webkit_web_view_get_user_content_manager(webview);
    webkit_user_content_manager_remove_all_filters(manager);
}

void
reload_webview_filters_using_setting_value_with_cb(
    WebViewFactory* webview_factory,
    WebKitWebView* webview,
    bool enable_content_blockers,
    std::function<void()> cb)
{
    orion::TabId tab_id = tab_id_for_webview(webview);
    orion::log_verbose(
        "filter_lists: reload_webview_filters_using_setting_value_with_cb: tab_id=%lld",
        tab_id.value);

    WebKitUserContentManager* manager = webkit_web_view_get_user_content_manager(webview);

    orion::gtk::WeakRefWrapper manager_weak(manager);

    fetch_saved_filters(webview_factory,
    [manager_weak,enable_content_blockers,cb = std::move(cb)](std::vector<WebKitUserContentFilter*> filters){
        auto manager_strong = manager_weak.lock();
        if (!manager_strong) return;
        WebKitUserContentManager* manager = WEBKIT_USER_CONTENT_MANAGER(manager_strong.get());

        for (WebKitUserContentFilter* filter : filters) {
            // Unreffing the filter unconditionally at the end of the scope seems to work fine
            defer([filter]{ webkit_user_content_filter_unref(filter); });
            if (enable_content_blockers) {
                webkit_user_content_manager_add_filter(manager, filter);
            } else {
                // Do not remove website-specific filters
                FilterDetail detail = content_filter_get_detail(filter);
                if (detail.filter_type == FilterType::normal) {
                    webkit_user_content_manager_remove_filter(manager, filter);
                }
            }
        }
        if(cb)
        {
            cb();
        }
    });
}

static void
reload_webview_filters_using_website_setting(WebViewFactory* webview_factory, WebKitWebView* webview)
{
    orion::TabId tab_id = tab_id_for_webview(webview);
    orion::log_verbose("filter_lists: reload_webview_filters_using_website_setting: tab_id=%lld", tab_id.value);

    auto profile_world = webview_factory->parent_world();
    // Note that orion_webview_tab_content_get_uri may still return stale data here
    orion::Uri current_uri = orion::Uri::parse_user_input(webkit_web_view_get_uri(webview));
    std::string website = current_uri.authority();
    orion::ProfileId profile_id = webview_factory->profile_id();
    orion::gtk::WeakRefWrapper webview_weak(webview);

    if (website.empty()) {
        // New Tab: always disable filters.
        // Each webview must start with filter disabled state because when we apply filters in on_webview_load_changed
        // during WEBKIT_LOAD_STARTED event, it works properly. But if we apply filters in a new webview by default
        // and then we try to disable filters in on_webview_load_changed during WEBKIT_LOAD_STARTED, the filters
        // would always apply for the website even when they should not.
        disable_webview_filters(webview);
        return;
    }

    orion::GetSettingForWebsiteOrDefaultQuery query{
        .profile_id = profile_id,
        .website = website,
        .key_name = orion::settings::website::specific::enable_content_blockers.key_name(),
    };

    global_core_api->get_setting_for_website_or_default(query,
    [website,webview_weak,profile_world](orion::GetSettingForWebsiteOrDefaultReply reply){
        auto world = profile_world.lock();
        if (!world) return;
        VERIFY_LOG(reply.err);
        bool enable_content_blockers{};
        VERIFY_LOG(orion::settings::unmarshal_value(reply.value, &enable_content_blockers));

        auto webview_strong = webview_weak.lock();
        if (!webview_strong) return;
        WebKitWebView* webview = WEBKIT_WEB_VIEW(webview_strong.get());

        auto factory_owner = web_view_factory_for(*world);

        auto* webview_factory = factory_owner.get();
        if(!webview_factory) return;
        reload_webview_filters_using_setting_value_with_cb(webview_factory, webview, enable_content_blockers, nullptr);
    });
}

void
reload_block_web_fonts_webview_filters_using_website_setting(WebViewFactory* webview_factory, WebKitWebView* webview)
{
    orion::TabId tab_id = tab_id_for_webview(webview);
    orion::log_verbose(
        "filter_lists: reload_block_web_fonts_webview_filters_using_website_setting: tab_id=%lld",
        tab_id.value);

    auto profile_world = webview_factory->parent_world();
    // Note that orion_webview_tab_content_get_uri may still return stale data here
    orion::Uri current_uri = orion::Uri::parse_user_input(webkit_web_view_get_uri(webview));
    std::string website = current_uri.authority();
    orion::ProfileId profile_id = webview_factory->profile_id();
    orion::gtk::WeakRefWrapper webview_weak(webview);

    if (website.empty()) {
        // New Tab: nothing to do
        return;
    }

    orion::GetSettingForWebsiteOrDefaultQuery query{
        .profile_id = profile_id,
        .website = website,
        .key_name = orion::settings::website::specific::block_web_fonts.key_name(),
    };

    global_core_api->get_setting_for_website_or_default(query,
    [website,webview_weak,profile_world](orion::GetSettingForWebsiteOrDefaultReply reply){
        auto world = profile_world.lock();
        if (!world) return;
        VERIFY_LOG(reply.err);

        bool block_web_fonts{};
        VERIFY_LOG(orion::settings::unmarshal_value(reply.value, &block_web_fonts));

        auto webview_strong = webview_weak.lock();
        if (!webview_strong) return;
        auto factory_owner = web_view_factory_for(*world);
        auto* webview_factory = factory_owner.get();
        if(!webview_factory) return;

        std::string filter_id = content_filter_block_web_fonts_id_for_website(website);

        if (block_web_fonts) {
            orion::log_verbose(
                "filter_lists: reload_block_web_fonts_webview_filters_using_website_setting: added block web fonts filter: website='%s'",
                website.c_str()
            );
            
            std::vector<FilterListMetadata> metadatas{
                {
                    .name = filter_id,
                    .json_data = R"(
                        [{"trigger":{"url-filter":".*","if-domain":[")"
                        + website
                        + R"("],"resource-type":["font"]},"action":{"type":"block"}}]
                    )",
                    .is_website_specific = true,
                    .is_enabled = true,
                    .filter_type = orion::FilterListType::block_web_fonts,
                },
            };
            setup_filter_lists_impl(webview_factory, metadatas, [world,website]{
                // On successful completion
                reload_website_webviews_in_profile(world->profile_id(), website);
            });
        } else {
            orion::GetFilterListsQuery query{
                .profile_id = webview_factory->profile_id(),
            };
            global_core_api->get_filter_lists(query,
            [website,filter_id,profile_world](orion::GetFilterListsReply reply){
                if (profile_world.expired()) return;

                VERIFY_LOG(reply.err);
                std::vector<orion::FilterListId> to_delete;
                for (const orion::FilterListDto& dto : reply.filter_lists) {
                    if (dto.name == filter_id) {
                        to_delete.push_back(dto.id);
                    }
                }

                global_core_api->delete_filter_lists(to_delete,
                [website,filter_id,profile_world](orion::Error err){
                    auto world = profile_world.lock();
                    if (!world) return;
                    VERIFY_LOG(err);

                    auto factory = web_view_factory_for(*world);
                    if (!factory) return;

                    fetch_saved_filter_ids(factory.get(), [world,website,filter_id](std::vector<std::string> saved_ids){
                        if (std::find(saved_ids.begin(), saved_ids.end(), filter_id) == saved_ids.end()) return;

                        // Asynchronously remove the filter from the underlying filter store in the profile
                        remove_filter_from_filter_stores_in_profile(filter_id, world->profile_id(),
                        [world,website]{
                            // On successful completion
                            reload_website_webviews_in_profile(world->profile_id(), website);
                        });
                    });
                });
            });
        }
    });
}

namespace {
struct ProcessMetadataQuery {
    FilterListMetadata metadata{};
    std::map<std::string, orion::FilterListDto> db_filter_lists{};
    std::shared_ptr<SetupFilterListsState> state{};
    bool force_update{};
    WebViewFactory* webview_factory{};
    std::vector<std::string> saved_ids{};
};
}

static void
filter_list_process_metadata(const ProcessMetadataQuery& query)
{
    auto started_at = std::chrono::steady_clock::now();
    const auto& metadata = query.metadata;
    const auto& db_filter_lists = query.db_filter_lists;
    const auto& webview_factory = query.webview_factory;
    auto profile_world = webview_factory->parent_world();

    auto db_filter_list = db_filter_lists.find(metadata.name);
    if (db_filter_list != db_filter_lists.end() && !db_filter_list->second.is_enabled) {
        orion::LogLevel level = metadata.filter_type == orion::FilterListType::custom
            ? orion::LogLevel::verbose
            : orion::LogLevel::debug;
        orion::log_by_level(
            level, 
            "filter_lists: filter list is disabled, skipping setup: name='%s'",
            metadata.name.c_str()
        );
        query.state->finish_one();
        return;
    }

    gchar* raw_data = NULL;

    if (!metadata.resource_path.empty()) {
        // Maybe dispatch this to disk thread?
        raw_data = orion::load_text_from_resource(metadata.resource_path.c_str());
        if (!raw_data) {
            orion::log_error(
                "filter_lists: failed to load filter list resource: path=%s",
                metadata.resource_path.c_str());
            query.state->finish_one();
            return;
        }
    }
    
    defer([raw_data]{ g_free(raw_data); });

    std::string data_to_parse = raw_data ? raw_data : metadata.raw_data;
    const std::string& content_to_hash = metadata.json_data.empty()
        ? data_to_parse
        : metadata.json_data;
    std::string content_hash = orion::crypto::sha256_str(content_to_hash);

    const bool is_saved = std::find(
        query.saved_ids.begin(), query.saved_ids.end(), metadata.name) != query.saved_ids.end();
    if (query.force_update
        && db_filter_list != db_filter_lists.end()
        && db_filter_list->second.content_hash == content_hash
        && is_saved) {
        const auto& db_list = db_filter_list->second;
        orion::UnixTimeUtc64 updated_at = orion::tm::unix_time_utc64_now();
        if (updated_at <= db_list.updated_at) {
            updated_at = orion::UnixTimeUtc64(db_list.updated_at.value + 1);
        }

        orion::CreateFilterListQuery update_query{
            .profile_id = db_list.profile_id,
            .name = db_list.name,
            .version = db_list.version,
            .position = db_list.position,
            .url = db_list.url,
            .display_name = db_list.display_name,
            .description = db_list.description,
            .is_enabled = db_list.is_enabled,
            .updated_at = updated_at,
            .active_rules_count = db_list.active_rules_count,
            .filter_type = db_list.filter_type,
            .custom_filter_data = db_list.custom_filter_data,
            .content_hash = db_list.content_hash,
            .update_is_enabled = false,
            .update_updated_at = true,
            .update_active_rules_count = false,
            .update_version = false,
            .update_content_hash = false,
        };

        orion::log_debug(
            "filter_lists: downloaded filter list content is unchanged, updating timestamp only: name='%s' updated_at=%lld content_hash='%s'",
            metadata.name.c_str(),
            updated_at.value,
            content_hash.c_str());
        global_core_api->create_or_update_filter_list(
            update_query,
            [state=query.state](orion::CreateFilterListReply reply) {
                VERIFY_LOG(reply.err);
                state->finish_one();
            });
        return;
    }

    auto filter_list = std::shared_ptr<orion::FilterList>(
        orion::filter_list_new(metadata.name.c_str()),
        orion::filter_list_free
    );

    // Get filter list version fast
    std::string content_version; // version that is parsed from the content of the filter list data
    VERIFY_LOG(orion::filter_list_parse_version(
        filter_list.get(), data_to_parse.c_str(), &content_version));
    if (content_version.empty()) {
        content_version = metadata.version;
    }
    if (content_version.empty()) {
        content_version = std::to_string(orion::tm::unix_time_utc64_now().value);
    }

    auto it = db_filter_lists.find(metadata.name);
    if (it != db_filter_lists.end()) {
        // Filter list is already in ProfilesDb - but might be missing in webkit store or might be older
        // than metadata version
        const auto& db_list = it->second;

        bool should_update = filter_list_should_update(
            db_list.version,
            content_version,
            db_list.content_hash,
            content_hash);
        if (!query.force_update && !should_update) {
            // Check webkit filter store
            if (std::find(query.saved_ids.begin(), query.saved_ids.end(), metadata.name) != query.saved_ids.end()) {
                orion::log_verbose(
                    "filter_lists: filter list already installed in same or newer version: name='%s' db_version='%s' content_version='%s'",
                    metadata.name.c_str(),
                    db_list.version.c_str(),
                    content_version.c_str()
                );
                query.state->finish_one();
                return;
            }
        } else if (should_update) {
            orion::log_verbose(
                "filter_lists: db filter list with older version than metadata version or different content hash, updating it: name='%s' db_version='%s' content_version='%s' db_hash='%s' content_hash='%s'",
                metadata.name.c_str(),
                db_list.version.c_str(),
                content_version.c_str(),
                db_list.content_hash.c_str(),
                content_hash.c_str()
            );
        } else {
            // Force update is enabled
            orion::log_debug(
                "filter_lists: force updating filter list: name='%s' db_version='%s' content_version='%s'",
                metadata.name.c_str(),
                db_list.version.c_str(),
                content_version.c_str()
            );
        }
    } else {
        orion::log_debug(
            "filter_lists: filter list does not exist in db, saving it: name='%s' content_version='%s'",
            metadata.name.c_str(),
            content_version.c_str()
        );
    }

    orion::log_by_level(
        metadata.is_website_specific ? orion::LogLevel::verbose : orion::LogLevel::debug,
        "filter_lists: setting up filter list: name='%s' is_website_specific=%d",
        metadata.name.c_str(),
        metadata.is_website_specific);

    std::shared_ptr<orion::StopCancellable> stop_cancellable
        = get_filter_store_cancellable(webview_factory);

    orion::LoadFilterListQuery load_query{
        .filter_list = filter_list,
        .raw_data = data_to_parse,
        .stop_token = stop_cancellable->get_stop_token(),
    };

    // Increment pending_ops before dispatching load and decrement it in the callback
    filter_store_callback_started(webview_factory);

    // Load must not be called on main thread, it is slow (and CPU-bound)
    global_core_api->parse_filter_list(load_query,
    // copy metadata to avoid overriding it in the loop before callback is called
    [metadata=metadata,content_version,content_hash,profile_world,started_at,state=query.state]
    (orion::LoadFilterListReply reply){
        auto world = profile_world.lock();
        if (!world) {
            state->finish_one();
            return;
        }
        auto factory_owner = web_view_factory_for(*world);
        auto* webview_factory = factory_owner.get();
        if(!webview_factory) {
            state->finish_one();
            return;
        }
        // Flag this callback as finished no matter what
        defer([webview_factory]{ filter_store_callback_finished(webview_factory); });

        if (reply.err.code() == orion::ErrorCode::cancelled) {
            orion::log_info("filter_lists: filter list parse was cancelled: name='%s'",
                metadata.name.c_str()
            );
            state->finish_one();
            return; // this is expected
        }
        VERIFY_LOG(reply.err);
        if (reply.err) {
            state->finish_one();
            return;
        }

        if (webview_factory->filter_store()->is_cancelling()) {
            orion::log_debug("parse_filter_list cb: filter store is cancelling, skipping setup: name='%s'",
                metadata.name.c_str()
            );
            state->finish_one();
            return;
        }

        std::string json;
        if (metadata.json_data.empty()) {
            // Can also take some time
            json = rfl::json::write(reply.items);
        } else {
            json = metadata.json_data;
        }

        auto load_took_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - started_at
        ).count();

        orion::log_by_level(
            metadata.is_website_specific ? orion::LogLevel::verbose : orion::LogLevel::debug,
            "filter_lists: filter list parsed: name='%s' parsed_rule_count=%d items=%zu json_size=%zu content_version='%s' private=%d took=%lldms",
            metadata.name.c_str(),
            reply.parsed_rule_count,
            reply.items.size(),
            json.size(),
            content_version.c_str(),
            webview_factory->private_mode() == orion::PrivateMode::yes,
            load_took_ms);

        // TEST override:
        // json = R"([
        //     {"action":{"type":"block"},"trigger":{"url-filter":"sdn.cz/d_.*jpeg"}},
        //     {"action":{"type":"block"},"trigger":{"url-filter":"sdn.cz/d_.*png"}}
        // ])";

    #if PRINT_JSON_FILTERS
        printf("%s\n", json.c_str());
    #endif

        WebKitUserContentFilterStore* store = get_or_create_filter_store(webview_factory);
        if (!store) {
            orion::log_error(
                "filter_lists: failed to get filter store for profile_id=%" PRId64,
                webview_factory->profile_id().value
            );
            state->finish_one();
            return;
        }

        GBytes* bytes = g_bytes_new(json.c_str(), json.size());
        defer([bytes]{ g_bytes_unref(bytes); });

        // Cache the parsed JSON to a file for later use to profile cache directory in the future if needed
        // // The same file is used for main and private worlds
        // write_parsed_json_file(webview_factory->profile_id(), metadata.name, bytes, cb);

        filter_store_callback_started(webview_factory);
        std::shared_ptr<orion::StopCancellable> stop_cancellable
            = get_filter_store_cancellable(webview_factory);
        GCancellable* cancellable = stop_cancellable->get();

        FilterStoreSaveData* save_data = new FilterStoreSaveData{
            .version = content_version,
            .started_at = std::chrono::steady_clock::now(), // measure webkit save separately
            .profile_world = profile_world,
            .is_website_specific = metadata.is_website_specific,
            .position = metadata.position,
            .url = metadata.url,
            .display_name = metadata.display_name,
            .description = metadata.description,
            .is_enabled = metadata.is_enabled,
            .active_rules_count = reply.parsed_rule_count,
            .filter_type = metadata.filter_type,
            .custom_filter_data = metadata.filter_type == orion::FilterListType::custom
                ? metadata.raw_data
                : "",
            .content_hash = content_hash,
            .completion = [state] { state->finish_one(); },
        };

        webkit_user_content_filter_store_save(
            store,
            metadata.name.c_str(),
            bytes,
            cancellable, // cancellable
            filter_store_save_ready_cb, // ready callback
            save_data // user_data
        );
    });
}

static void
setup_filter_lists_impl(
    WebViewFactory* webview_factory,
    const std::vector<FilterListMetadata>& metadatas,
    std::function<void()> completion,
    bool force_update,
    bool include_custom_filter_lists)
{
    auto profile_world = webview_factory->parent_world();
    auto state = std::make_shared<SetupFilterListsState>(SetupFilterListsState{
        .remaining = metadatas.size(),
        .completion = std::move(completion),
    });
    if (metadatas.empty()) {
        if (state->completion) state->completion();
        return;
    }

    if (webview_factory->filter_store()->is_cancelling()) {
        orion::log_debug("filter_lists: setup_filter_lists_impl: filter store is cancelling, skipping setup");
        state->finish_all();
        return;
    }

    // Setup observer of all web views only once
    if (webview_factory->web_views_observer() == orion::ObserverId{0}) {
        orion::ObserverId observer = webview_factory->web_views_change().add_observer(
        [profile_world](const WebExtensions::WebViewMapChange& change){
            auto world = profile_world.lock();
            if (!world) return;
            auto factory_owner = web_view_factory_for(*world);
            auto* webview_factory = factory_owner.get();
            if(!webview_factory) return;
            orion::log_verbose("filter_lists: web_views_change: created=%zu deleted=%zu",
                change.created.size(), change.deleted.size());

            for (const orion::gtk::WeakRefWrapper& webview_weak : change.created) {
                auto webview_strong = webview_weak.lock();
                if (!webview_strong) continue;
                WebKitWebView* webview = WEBKIT_WEB_VIEW(webview_strong.get());

                reload_webview_filters_using_website_setting(webview_factory, webview);
                reload_block_web_fonts_webview_filters_using_website_setting(webview_factory, webview);
            }
        });

        webview_factory->set_web_views_observer(observer);
    }

    orion::GetFilterListsQuery query{
        .profile_id = webview_factory->profile_id(),
    };

    global_core_api->get_filter_lists(query,
    [profile_world,metadatas,state,force_update,include_custom_filter_lists](orion::GetFilterListsReply reply){
        VERIFY_LOG(reply.err);
        if (reply.err) {
            state->finish_all();
            return;
        }
        auto world = profile_world.lock();
        if (!world) {
            state->finish_all();
            return;
        }
        auto factory_owner = web_view_factory_for(*world);
        auto* webview_factory = factory_owner.get();
        if(!webview_factory) {
            state->finish_all();
            return;
        }

        if (webview_factory->filter_store()->is_cancelling()) {
            orion::log_debug("filter_lists: get_filter_lists cb: filter store is cancelling, skipping setup");
            state->finish_all();
            return;
        }
    
        std::map<std::string, orion::FilterListDto> db_filter_lists{};
        for (const orion::FilterListDto& dto : reply.filter_lists) {
            db_filter_lists[dto.name] = dto;
        }

        std::vector<FilterListMetadata> filter_lists_to_process = metadatas;
        if (include_custom_filter_lists) {
            for (const orion::FilterListDto& dto : reply.filter_lists) {
                if (dto.filter_type != orion::FilterListType::custom) continue;

                filter_lists_to_process.push_back({
                    .name = dto.name,
                    .display_name = dto.display_name,
                    .raw_data = dto.custom_filter_data,
                    .url = dto.url,
                    .version = dto.version,
                    .is_data_embedded = true,
                    .description = dto.description,
                    .is_enabled = dto.is_enabled,
                    .position = dto.position,
                    .filter_type = dto.filter_type,
                });
                ++state->remaining;
            }
        }

        fetch_saved_filter_ids(webview_factory,
        [profile_world,db_filter_lists,filter_lists_to_process,state,force_update](std::vector<std::string> saved_ids){
            auto world = profile_world.lock();
            if (!world) {
                state->finish_all();
                return;
            }
            auto factory_owner = web_view_factory_for(*world);
            auto* webview_factory = factory_owner.get();
            if(!webview_factory) {
                state->finish_all();
                return;
            }
            if (webview_factory->filter_store()->is_cancelling()) {
                orion::log_debug("filter_lists: fetch_saved_filter_ids cb: filter store is cancelling, skipping setup");
                state->finish_all();
                return;
            }

            // Disabled lists remain in ProfilesDb so the user's choice is persistent, but they must
            // be treated as absent from WebKit.
            std::vector<std::string> known_enabled_db_filter_ids{};
            for (const auto& [name, dto] : db_filter_lists) {
                if (dto.is_enabled) {
                    known_enabled_db_filter_ids.push_back(name);
                }
            }
            remove_unknown_or_disabled_webkit_filters(webview_factory, saved_ids, known_enabled_db_filter_ids);

            for (const FilterListMetadata& metadata : filter_lists_to_process) {
                filter_list_process_metadata({
                    .metadata = metadata,
                    .db_filter_lists = db_filter_lists,
                    .state = state,
                    .force_update = force_update,
                    .webview_factory = webview_factory,
                    .saved_ids = saved_ids,
                });
            }
        });
    });
}

namespace {

struct DownloadFilterListData {
    std::weak_ptr<orion::bridge::ProfileWorld> profile_world{};
    FilterListMetadata metadata{};
    std::shared_ptr<orion::Downloader> downloader{};
    std::string download_path{};
    bool force_update{};
};

} // namespace

static void
download_filter_list_finished(DownloadFilterListData* data, orion::Error err)
{
    defer([data] { delete data; });
    defer([download_path=data->download_path] {
        std::error_code remove_error;
        std::filesystem::remove(download_path, remove_error);
    });
    auto world = data->profile_world.lock();
    if (!world) return;
    auto factory_owner = web_view_factory_for(*world);
    auto* webview_factory = factory_owner.get();
    if(!webview_factory) return;

    bool setup_started = false;
    defer([webview_factory,&setup_started] {
        if (!setup_started) filter_list_sync_finished(webview_factory);
    });
    defer([webview_factory,downloader=data->downloader] {
        filter_list_download_finished(webview_factory, downloader);
    });

    if (err) {
        orion::log_error(
            "filter_lists: download_filter_list_finished: failed to download filter list: name='%s' url='%s' error='%s'",
            data->metadata.name.c_str(), data->metadata.url.c_str(),
            err.to_string().c_str());
        return;
    }

    gchar* contents = nullptr;
    gsize contents_length{};
    GError* read_error = nullptr;
    gboolean read_success = g_file_get_contents(
        data->download_path.c_str(), &contents, &contents_length, &read_error);
    defer([contents] { g_free(contents); });
    if (!read_success) {
        orion::log_error(
            "filter_lists: download_filter_list_finished: failed to read downloaded filter list: name='%s' url='%s' error='%s'",
            data->metadata.name.c_str(), data->metadata.url.c_str(),
            read_error ? read_error->message : "unknown error");
        g_clear_error(&read_error);
        return;
    }
    if (contents_length == 0) {
        orion::log_error(
            "filter_lists: download_filter_list_finished: downloaded filter list is empty: name='%s' url='%s'",
            data->metadata.name.c_str(), data->metadata.url.c_str());
        return;
    }

    data->metadata.raw_data.assign(contents, contents_length);

    orion::log_verbose(
        "filter_lists: finished filter list download: name='%s' url='%s' contents_length=%zu",
        data->metadata.name.c_str(),
        data->metadata.url.c_str(),
        contents_length);

    std::vector<FilterListMetadata> metadatas;
    metadatas.push_back(data->metadata);
    setup_started = true;
    setup_filter_lists_impl(
        webview_factory,
        metadatas,
        [profile_world=data->profile_world] {
            auto world = profile_world.lock();
            if (!world) return;
            auto factory_owner = web_view_factory_for(*world);
            if (factory_owner) filter_list_sync_finished(factory_owner.get());
        },
        data->force_update);
}

void
setup_filter_lists_from_urls(
    WebViewFactory* webview_factory,
    const std::vector<orion::FilterListDto>& filter_lists,
    bool force_update)
{
    if (webview_factory->filter_store()->is_cancelling()) {
        orion::log_debug(
            "filter_lists: setup_filter_lists_from_urls: filter store is cancelling, skipping downloads");
        return;
    }

    for (const orion::FilterListDto& filter_list : filter_lists) {
        if (!filter_list.is_enabled || filter_list.url.empty()) continue;

        std::shared_ptr<orion::Downloader> downloader = global_core_api->create_downloader();
        if (!downloader) {
            orion::log_error(
                "filter_lists: setup_filter_lists_from_urls: failed to create downloader for filter list: name='%s' url='%s'",
                filter_list.name.c_str(), filter_list.url.c_str());
            continue;
        }

        std::string download_path = (
            std::filesystem::path(g_get_tmp_dir())
            / ("orion-filter-list-" + orion::generate_uuid_v4())
        ).string();
        auto* data = new DownloadFilterListData{
            .profile_world = webview_factory->parent_world(),
            .metadata = {
                .name = filter_list.name,
                .display_name = filter_list.display_name,
                .url = filter_list.url,
                .is_data_embedded = false,
                .description = filter_list.description,
                .is_enabled = true,
                .position = filter_list.position,
                .filter_type = filter_list.filter_type,
            },
            .downloader = downloader,
            .download_path = download_path,
            .force_update = force_update,
        };

        downloader->set_target_path(download_path);
        downloader->set_uri(filter_list.url);
        downloader->set_finished_cb([data](orion::Error err) {
            download_filter_list_finished(data, std::move(err));
        });
        filter_list_download_started(webview_factory, downloader);
        downloader->start_download();
        orion::log_verbose(
            "filter_lists: started downloading filter list: name='%s' url='%s'",
            filter_list.name.c_str(), filter_list.url.c_str());
    }
}

static void
setup_non_embedded_filter_lists(WebViewFactory* webview_factory)
{
    std::vector<FilterListMetadata> metadatas{};
    for (const FilterListMetadata& metadata : global_metadatas) {
        if (!metadata.is_data_embedded) {
            metadatas.push_back(metadata);
        }
    }

    for (const FilterListMetadata& metadata : metadatas) {
        orion::ProfileId profile_id = webview_factory->profile_id();
        bool is_private = webview_factory->private_mode() == orion::PrivateMode::yes;

        orion::CreateFilterListQuery query{
            .profile_id = profile_id,
            .name = metadata.name,
            .version = "",
            .position = metadata.position,
            .url = metadata.url,
            .display_name = metadata.display_name,
            .description = metadata.description,
            .is_enabled = metadata.is_enabled, // should always be false
            .updated_at = UnixTimeUtc64(0),
            .active_rules_count = 0,
            .filter_type = metadata.filter_type,
            .custom_filter_data = "",
            .content_hash = "",

            .update_is_enabled = false,
            .update_updated_at = false,
            .update_active_rules_count = false,
            .update_version = false,
            .update_content_hash = false,
        };
        global_core_api->create_or_update_filter_list(query, [](orion::CreateFilterListReply reply){
            VERIFY_LOG(reply.err);
        });
    }
}

static void
setup_enabled_remote_filter_lists(WebViewFactory* webview_factory)
{
    orion::GetFilterListsQuery query{
        .profile_id = webview_factory->profile_id(),
    };

    auto profile_world = webview_factory->parent_world();
    global_core_api->get_filter_lists(query, [profile_world](orion::GetFilterListsReply reply) {
        auto world = profile_world.lock();
        if (!world) return;
        auto factory_owner = web_view_factory_for(*world);
        auto* webview_factory = factory_owner.get();
        if(!webview_factory) return;
        VERIFY_LOG(reply.err);
        if (reply.err || webview_factory->filter_store()->is_cancelling()) return;

        std::vector<orion::FilterListDto> remote_filter_lists;
        for (const orion::FilterListDto& filter_list : reply.filter_lists) {
            if (!filter_list.is_enabled || filter_list.url.empty()) continue;

            bool is_remote = filter_list.filter_type == orion::FilterListType::user;
            if (!is_remote) {
                auto metadata = std::find_if(
                    global_metadatas.begin(),
                    global_metadatas.end(),
                    [&filter_list](const FilterListMetadata& candidate) {
                        return candidate.name == filter_list.name;
                    });
                is_remote = metadata != global_metadatas.end()
                    && !metadata->is_data_embedded;
            }
            if (is_remote) remote_filter_lists.push_back(filter_list);
        }
        setup_filter_lists_from_urls(webview_factory, remote_filter_lists);
    });
}

void
setup_filter_lists(WebViewFactory* webview_factory, bool download_remote_filter_lists)
{
    std::vector<FilterListMetadata> metadatas{};
    for (const FilterListMetadata& metadata : global_metadatas) {
        if (metadata.is_data_embedded) {
            metadatas.push_back(metadata);
        }
    }
    setup_filter_lists_impl(webview_factory, metadatas, {}, false, true);

    setup_non_embedded_filter_lists(webview_factory);
    if (download_remote_filter_lists) {
        setup_enabled_remote_filter_lists(webview_factory);
    }
}

#else // defined(ENABLE_WEBKIT) && ENABLE_WEBKIT

void
free_embedded_filter_lists(WebViewFactory* webview_factory)
{
}

void
setup_filter_lists(WebViewFactory* webview_factory, bool download_remote_filter_lists)
{
    (void)download_remote_filter_lists;
}

void
setup_filter_lists_from_urls(
    WebViewFactory* webview_factory,
    const std::vector<orion::FilterListDto>& filter_lists,
    bool force_update)
{
    (void)webview_factory;
    (void)filter_lists;
    (void)force_update;
}

void
remove_filter_list_from_webkit_store(WebViewFactory* webview_factory, const std::string& filter_id, orion::Fn cb)
{
}

#endif // defined(ENABLE_WEBKIT) && ENABLE_WEBKIT

} // namespace orion::contentblock
