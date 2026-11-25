#include <common/src/coroutine.h>
#include <common/src/time_util.h>
#include <common/src/uuid.h>
#include <core/sync/sync_service.h>
#include <core/core_api.h>
#include <src/sync_api.h>
#include <rfl/json.hpp>

#include <condition_variable>
#include <algorithm>
#include <map>
#include <atomic>
#include <optional>

using namespace orion::sync;

namespace orion {

class SyncServiceImpl
    : public SyncService
    , public std::enable_shared_from_this<SyncServiceImpl>
{
public:
    SyncServiceImpl(CoreApi* api) : m_api(api) { }
    virtual OperationHandle export_to(ProfileId profile_id, const std::string& directory, ProgressHandler progress_handler, ExportToCompletionHandler completion_handler) override;
    virtual OperationHandle import_from(ProfileId profile_id, const std::string& source, ProgressHandler progress_handler, ImportFromCompletionHandler completion_handler) override;
private:
    CoreApi* m_api = nullptr;
};

struct ChannelExec {
    std::deque<std::function<void()>> queue;
    std::mutex mux;
    std::condition_variable cv;
};

struct SyncOperationState {
    std::atomic<bool> cancel_requested = false;
    std::mutex active_channel_m;
    SyncId active_sync_id = Failure;
};

static void
request_cancel(const std::shared_ptr<SyncOperationState>& operation_state)
{
    if (!operation_state) {
        return;
    }

    operation_state->cancel_requested.store(true, std::memory_order_relaxed);

    SyncId active_sync_id = Failure;
    {
        std::lock_guard lk(operation_state->active_channel_m);
        active_sync_id = operation_state->active_sync_id;
    }

    if (active_sync_id != Failure) {
        SyncApi::instance()->cancel(active_sync_id);
    }
}

class SyncServiceHandleImpl : public SyncService::Handle {
public:
    explicit SyncServiceHandleImpl(std::weak_ptr<SyncOperationState> operation_state)
        : m_operation_state(std::move(operation_state))
    {
    }

    void
    cancel() override
    {
        request_cancel(m_operation_state.lock());
    }

private:
    std::weak_ptr<SyncOperationState> m_operation_state;
};

struct ExportToPrivateData {
    orion::ProfileId profile_id;
    std::string directory;
    SyncService::ProgressHandler progress_handler;
    SyncService::ExportToCompletionHandler completion_handler;
    std::shared_ptr<SyncApi> sync_api;
    CoreApi* api;
    std::unique_ptr<ChannelExec> exec;
    std::shared_ptr<SyncOperationState> operation_state;
    size_t export_data_idx;
    std::deque<std::shared_ptr<Payload>> export_data;
};

struct SyncWindowItem {
    DTO_MEMBER(id, orion::WindowId, orion::WindowId(0));
    DTO_MEMBER(name, std::string);
    DTO_MEMBER(icon_name, std::string);
    DTO_MEMBER(keyboard_shortcut, std::string);
    DTO_MEMBER(is_suspended, bool, false);
    DTO_MEMBER(seq_num, double, 0.0);
    DTO_MEMBER(is_tab_group, bool, false);
    DTO_MEMBER(selected_tab_id, orion::TabId, orion::TabId(0));
    DTO_MEMBER(has_ui_window, bool, false);
    DTO_MEMBER(is_focus_mode, bool, false);
    DTO_MEMBER(is_profile_selected, bool, false);
};

struct ImportFromPrivateData {
    orion::ProfileId profile_id;
    std::string source;
    SyncService::ProgressHandler progress_handler;
    SyncService::ImportFromCompletionHandler completion_handler;
    std::shared_ptr<SyncApi> sync_api;
    CoreApi* api;
    std::unique_ptr<ChannelExec> exec;
    std::shared_ptr<SyncOperationState> operation_state;

    std::vector<orion::ReadingListEntryDto> reading_list_entries;
    std::vector<orion::BookmarkFolderDto> bookmark_folders;
    std::vector<orion::BookmarkDto> bookmarks;
    std::vector<SyncWindowItem> windows;
    std::vector<orion::TabDto> tabs;
    std::map<orion::BookmarkFolderId, orion::BookmarkFolderId> imported_to_created_bookmark_folder_ids;
    std::map<orion::WindowId, orion::WindowId> imported_to_created_window_ids;
    std::map<orion::TabId, orion::TabId> imported_to_created_tab_ids;
    orion::WindowId target_shared_pinned_tabs_window_id = orion::WindowId(0);
};

static Error
make_cancelled_error()
{
    return MAKE_ERROR(ErrorCode::cancelled, "Operation cancelled by user");
}

static bool
is_cancel_requested(const std::shared_ptr<SyncOperationState>& operation_state)
{
    return operation_state && operation_state->cancel_requested.load(std::memory_order_relaxed);
}

static Error
check_cancel_requested(const std::shared_ptr<SyncOperationState>& operation_state)
{
    if (is_cancel_requested(operation_state)) {
        return make_cancelled_error();
    }
    return Error::ok();
}

static void
channel_exec_push(ChannelExec& exec, std::function<void()> fn)
{
    std::unique_lock lk{exec.mux};
    exec.queue.push_back(fn);
    exec.cv.notify_all();
}

static void
channel_exec_iterate(ChannelExec& exec)
{
    std::unique_lock lk{exec.mux};
    if(exec.queue.empty()) {
        exec.cv.wait_for(lk, 10ms);
    }
    decltype(exec.queue) queue;
    queue.swap(exec.queue);
    for(auto fn: queue) {
        fn();
    }
}

static std::shared_ptr<Payload>
sync_export_one_item(std::shared_ptr<ExportToPrivateData> private_data)
{
    // sync thread, sequenced after export_prepare
    if (is_cancel_requested(private_data->operation_state)) {
        return nullptr;
    }
    if(private_data->export_data_idx >= private_data->export_data.size()) {
        return nullptr;
    }
    return private_data->export_data[private_data->export_data_idx++];
}

template<typename T, typename U>
std::shared_ptr<Payload>
export_prepare_one_item(const U& item)
{
    return std::make_shared<T>(generate_uuid_v4(), tm::today_local(), 140, rfl::json::write(item), 1);
}

static async_task<orion::Error>
export_prepare_reading_lists(std::shared_ptr<ExportToPrivateData> private_data)
{
    // main thread
    if (Error cancel_err = check_cancel_requested(private_data->operation_state); cancel_err) {
        co_return cancel_err;
    }
    GetReadingListEntryIdsQuery query;
    query.profile_id = private_data->profile_id;
    auto reply = co_await async(
        invoke(private_data->api, &CoreApi::get_reading_list_entry_ids), query
    );
    if (reply.err) {
        co_return WRAP_ERROR(std::move(reply.err));
    }
    size_t reading_list_idx = 0;
    const size_t reading_lists_cnt = reply.ids.size();
    for(auto id : reply.ids) {
        if (Error cancel_err = check_cancel_requested(private_data->operation_state); cancel_err) {
            co_return cancel_err;
        }
        auto reply = co_await async(
            invoke(private_data->api, &CoreApi::get_reading_list_entries), std::vector<ReadingListEntryId>{id}
        );
        if (reply.err) {
            co_return WRAP_ERROR(std::move(reply.err));
        }
        for(const auto& item : reply.entries) {
            auto json = export_prepare_one_item<ReadingListPayload>(item);
            private_data->export_data.push_back(json);
        }
        private_data->progress_handler(++reading_list_idx, reading_lists_cnt, "Reading Lists");
    }
    co_return Error::ok();
}

static async_task<orion::Error>
export_prepare_bookmark_folders(std::shared_ptr<ExportToPrivateData> private_data)
{
    // main thread
    if (Error cancel_err = check_cancel_requested(private_data->operation_state); cancel_err) {
        co_return cancel_err;
    }
    auto reply = co_await async(
        invoke(private_data->api, &CoreApi::get_bookmark_folder_ids), private_data->profile_id
    );
    if (reply.err) {
        co_return WRAP_ERROR(std::move(reply.err));
    }
    size_t bookmark_folders_idx = 0;
    const size_t bookmark_folders_cnt = reply.ids.size();
    for(auto id : reply.ids) {
        if (Error cancel_err = check_cancel_requested(private_data->operation_state); cancel_err) {
            co_return cancel_err;
        }
        auto reply = co_await async(
            invoke(private_data->api, &CoreApi::get_bookmark_folders), std::vector<BookmarkFolderId>{id}
        );
        if (reply.err) {
            co_return WRAP_ERROR(std::move(reply.err));
        }
        for(const auto& item : reply.folders) {
            auto json = export_prepare_one_item<BookmarkFolderPayload>(item);
            private_data->export_data.push_back(json);
        }
        private_data->progress_handler(++bookmark_folders_idx, bookmark_folders_cnt, "Bookmark Folders");
    }
    co_return Error::ok();
}

static async_task<orion::Error>
export_prepare_bookmarks(std::shared_ptr<ExportToPrivateData> private_data)
{
    // main thread
    if (Error cancel_err = check_cancel_requested(private_data->operation_state); cancel_err) {
        co_return cancel_err;
    }
    GetBookmarkIdsQuery query;
    query.profile_id = private_data->profile_id;
    auto reply = co_await async(
        invoke(private_data->api, &CoreApi::get_bookmark_ids), query
    );
    if (reply.err) {
        co_return WRAP_ERROR(std::move(reply.err));
    }
    size_t bookmark_folders_idx = 0;
    const size_t bookmark_folders_cnt = reply.ids.size();
    for(auto id : reply.ids) {
        if (Error cancel_err = check_cancel_requested(private_data->operation_state); cancel_err) {
            co_return cancel_err;
        }
        GetBookmarksQuery query;
        query.ids = {id};
        auto reply = co_await async(
            invoke(private_data->api, &CoreApi::get_bookmarks), query
        );
        if (reply.err) {
            co_return WRAP_ERROR(std::move(reply.err));
        }
        for(const auto& item : reply.bookmarks) {
            auto json = export_prepare_one_item<BookmarkPayload>(item);
            private_data->export_data.push_back(json);
        }
        private_data->progress_handler(++bookmark_folders_idx, bookmark_folders_cnt, "Bookmarks");
    }
    co_return Error::ok();
}

static async_task<orion::Error>
export_prepare_windows(std::shared_ptr<ExportToPrivateData> private_data)
{
    if (Error cancel_err = check_cancel_requested(private_data->operation_state); cancel_err) {
        co_return cancel_err;
    }
    orion::WindowId selected_window_id = orion::WindowId(0);
    {
        auto selected_reply = co_await async(
            invoke(private_data->api, &CoreApi::get_selected_window_id), private_data->profile_id
        );
        if (selected_reply.err) {
            co_return WRAP_ERROR(std::move(selected_reply.err));
        }
        if (selected_reply.window_exists) {
            selected_window_id = selected_reply.window_id;
        }
    }

    GetWindowIdsQuery query{
        .profile_id = private_data->profile_id,
        .include_private_windows = false,
    };
    auto reply = co_await async(
        invoke(private_data->api, &CoreApi::get_window_ids), query
    );
    if (reply.err) {
        co_return WRAP_ERROR(std::move(reply.err));
    }

    size_t idx = 0;
    const size_t total = reply.ids.size();
    for (const auto& id : reply.ids) {
        if (Error cancel_err = check_cancel_requested(private_data->operation_state); cancel_err) {
            co_return cancel_err;
        }
        auto window_reply = co_await async(
            invoke(private_data->api, &CoreApi::get_windows), std::vector<WindowId>{id}
        );
        if (window_reply.err) {
            co_return WRAP_ERROR(std::move(window_reply.err));
        }
        for (const auto& item : window_reply.windows) {
            SyncWindowItem sync_item{
                .id = item.id,
                .name = item.name,
                .icon_name = item.icon_name,
                .keyboard_shortcut = item.keyboard_shortcut,
                .is_suspended = item.is_suspended,
                .seq_num = item.seq_num,
                .is_tab_group = item.is_tab_group,
                .selected_tab_id = item.selected_tab_id,
                .has_ui_window = item.has_ui_window,
                .is_focus_mode = item.is_focus_mode,
                .is_profile_selected = (item.id == selected_window_id),
            };
            auto json = export_prepare_one_item<WindowPayload>(sync_item);
            private_data->export_data.push_back(json);
        }
        private_data->progress_handler(++idx, total, "Windows");
    }
    co_return Error::ok();
}

static async_task<orion::Error>
export_prepare_tabs(std::shared_ptr<ExportToPrivateData> private_data)
{
    if (Error cancel_err = check_cancel_requested(private_data->operation_state); cancel_err) {
        co_return cancel_err;
    }
    std::vector<WindowId> window_ids;
    {
        GetWindowIdsQuery query{
            .profile_id = private_data->profile_id,
            .include_private_windows = false,
        };
        auto reply = co_await async(
            invoke(private_data->api, &CoreApi::get_window_ids), query
        );
        if (reply.err) {
            co_return WRAP_ERROR(std::move(reply.err));
        }
        window_ids = std::move(reply.ids);
    }

    {
        auto shared_reply = co_await async(
            invoke(private_data->api, &CoreApi::get_shared_pinned_tabs_window_id), private_data->profile_id
        );
        if (shared_reply.err) {
            co_return WRAP_ERROR(std::move(shared_reply.err));
        }
        if (shared_reply.window_id.value != 0) {
            bool has_shared_id = std::find(window_ids.begin(), window_ids.end(), shared_reply.window_id) != window_ids.end();
            if (!has_shared_id) {
                window_ids.push_back(shared_reply.window_id);
            }
        }
    }

    for (const auto& window_id : window_ids) {
        if (Error cancel_err = check_cancel_requested(private_data->operation_state); cancel_err) {
            co_return cancel_err;
        }
        auto ids_reply = co_await async(
            invoke(private_data->api, &CoreApi::get_tab_ids), GetTabIdsQuery{.window_id = window_id}
        );
        if (ids_reply.err) {
            co_return WRAP_ERROR(std::move(ids_reply.err));
        }

        size_t idx = 0;
        const size_t total = ids_reply.ids.size();
        for (const auto& tab_id : ids_reply.ids) {
            if (Error cancel_err = check_cancel_requested(private_data->operation_state); cancel_err) {
                co_return cancel_err;
            }
            auto tabs_reply = co_await async(
                invoke(private_data->api, &CoreApi::get_tabs), std::vector<TabId>{tab_id}
            );
            if (tabs_reply.err) {
                co_return WRAP_ERROR(std::move(tabs_reply.err));
            }

            for (auto item : tabs_reply.tabs) {
                // Session and back-forward state are intentionally not exported.
                item.session_state.clear();
                item.back_forward_list_index = 0;
                item.webkit_back_forward_list_index = 0;
                item.merged_back_forward_list_length = 0;
                item.snapshot_image.clear();
                item.session_state.clear();

                auto json = export_prepare_one_item<TabPayload>(item);
                private_data->export_data.push_back(json);
            }
            
            private_data->progress_handler(++idx, total, "Tabs");
        }
    }
    co_return Error::ok();
}

static void
export_done(std::shared_ptr<ExportToPrivateData> private_data, Error err)
{
    // main thread
    private_data->completion_handler(err);
}

static void
export_progressed(std::shared_ptr<ExportToPrivateData> private_data, size_t idx)
{
    // main thread
    private_data->progress_handler(idx, private_data->export_data.size(), "Exporting ...");
}

static Error
export_to_blocking(std::shared_ptr<ExportToPrivateData> private_data)
{
    if (Error cancel_err = check_cancel_requested(private_data->operation_state); cancel_err) {
        return cancel_err;
    }

    Uri uri = Uri::parse_strict(("file://" + private_data->directory).c_str());
    auto export_one_item_cb = [private_data]() {
        std::shared_ptr<Payload> item = sync_export_one_item(private_data);
        if(item) {
            channel_exec_push(*private_data->exec, [private_data,idx=private_data->export_data_idx](){
                dispatch(orion::on_main_thread(), [private_data,idx](){
                    export_progressed(private_data, idx);
                });
            });
        }
        return item;
    };
    std::optional<Error> err;
    auto export_done_cb = [private_data,&err](SyncId sync_id, Error e) {
        channel_exec_push(*private_data->exec, [e,&err](){
            err.emplace(e);
        });
        if (e && sync_id != Failure) {
            private_data->sync_api->cancel(sync_id);
        }
        return true;
    };
    SyncApi::ChannelHandle channel_handle = private_data->sync_api->sync(Collection<void>(), std::move(uri), export_one_item_cb, nullptr, export_done_cb);
    {
        std::lock_guard lk(private_data->operation_state->active_channel_m);
        private_data->operation_state->active_sync_id = channel_handle.id();
    }
    bool cancel_dispatched = false;
    while(!err.has_value()) {
        if (!cancel_dispatched && is_cancel_requested(private_data->operation_state)) {
            cancel_dispatched = true;
            channel_handle.cancel();
        }
        channel_exec_iterate(*private_data->exec);
    }
    {
        std::lock_guard lk(private_data->operation_state->active_channel_m);
        private_data->operation_state->active_sync_id = Failure;
    }
    if (is_cancel_requested(private_data->operation_state)) {
        return make_cancelled_error();
    }
    if (*err) {
        return WRAP_ERROR(std::move(*err));
    }
    return Error::ok();
}

static detached_task
export_to_task(std::shared_ptr<ExportToPrivateData> private_data)
{
    typedef async_task<orion::Error> (*ExportTask)(std::shared_ptr<ExportToPrivateData> private_data);
    ExportTask export_tasks[] = {
        export_prepare_reading_lists,
        export_prepare_bookmark_folders,
        export_prepare_bookmarks,
        export_prepare_windows,
        export_prepare_tabs,
    };
    Error err = Error::ok();
    for(auto task : export_tasks) {
        err = check_cancel_requested(private_data->operation_state);
        if (err) {
            break;
        }
        err = co_await task(private_data);
        if(err) {
            break;
        }
        auto export_to_task_cb = async([](std::shared_ptr<ExportToPrivateData> private_data, std::function<void(orion::Error)> cb) -> void {
            dispatch(orion::on_disk_thread(), [private_data,cb]() {
                Error err = export_to_blocking(private_data);
                dispatch(orion::on_main_thread(), [cb,err]() {
                    cb(err);
                });
            });
        }, private_data);
        err = co_await export_to_task_cb;
        if(err) {
            break;
        }
    }
    if (!err) {
        err = check_cancel_requested(private_data->operation_state);
    }
    export_done(private_data, err);
    /*
    dispatch(orion::on_main_thread(), [private_data,err](){
        export_done(private_data, err);
    });
    */
    co_return;
}

SyncService::OperationHandle
SyncServiceImpl::export_to(ProfileId profile_id, const std::string& directory, ProgressHandler progress_handler, ExportToCompletionHandler completion_handler)
{
    auto operation_state = std::make_shared<SyncOperationState>();

    auto private_data = std::make_shared<ExportToPrivateData>(ExportToPrivateData{
        .profile_id = profile_id,
        .directory = directory,
        .progress_handler = progress_handler,
        .completion_handler = completion_handler,
        .sync_api = SyncApi::instance(),
        .api = m_api,
        .exec = std::make_unique<ChannelExec>(),
        .operation_state = operation_state,
        .export_data_idx = 0,
        .export_data = {},
    });
    export_to_task(private_data);
    return std::make_shared<SyncServiceHandleImpl>(operation_state);
}

template<typename T>
static Error
import_payload_to_item(const std::shared_ptr<Payload>& payload, T* out)
{
    if (!payload || !out) {
        RAISE(ErrorCode::argument, "import_payload_to_item: invalid payload or output pointer");
    }

    try {
        *out = rfl::json::read<T>(payload->bsdo_object().payload).value();
    } catch (const std::exception& ex) {
        RAISE(ErrorCode::invalid_format, "Failed to parse imported payload JSON: %s", ex.what());
    }
    SUCCEED();
}

template<typename PayloadType, typename ConsumerFn>
static Error
import_from_collection_blocking(std::shared_ptr<ImportFromPrivateData> private_data, const std::string& description, ConsumerFn consumer_fn)
{
    if (Error cancel_err = check_cancel_requested(private_data->operation_state); cancel_err) {
        return cancel_err;
    }

    Uri uri = Uri::parse_strict(("file://" + private_data->source).c_str());
    std::optional<Error> err;
    size_t imported_count = 0;

    auto import_item_cb = [private_data,description,&consumer_fn,&imported_count](std::shared_ptr<Payload> payload) -> Error {
        if (Error cancel_err = check_cancel_requested(private_data->operation_state); cancel_err) {
            return cancel_err;
        }
        Error consumer_err = consumer_fn(payload);
        if (consumer_err) {
            return WRAP_ERROR(std::move(consumer_err));
        }
        ++imported_count;
        channel_exec_push(*private_data->exec, [private_data,description,imported_count](){
            dispatch(orion::on_main_thread(), [private_data,description,imported_count](){
                private_data->progress_handler(imported_count, imported_count, "Importing ...");
            });
        });
        return Error::ok();
    };

    auto import_done_cb = [private_data,&err](SyncId sync_id, Error e) {
        channel_exec_push(*private_data->exec, [e,&err](){
            err.emplace(e);
        });
        if (e && sync_id != Failure) {
            private_data->sync_api->cancel(sync_id);
        }
        return true;
    };

    SyncApi::ChannelHandle channel_handle = private_data->sync_api->sync(
        Collection<PayloadType>(), std::move(uri), nullptr, import_item_cb, import_done_cb
    );
    {
        std::lock_guard lk(private_data->operation_state->active_channel_m);
        private_data->operation_state->active_sync_id = channel_handle.id();
    }
    bool cancel_dispatched = false;
    while(!err.has_value()) {
        if (!cancel_dispatched && is_cancel_requested(private_data->operation_state)) {
            cancel_dispatched = true;
            channel_handle.cancel();
        }
        channel_exec_iterate(*private_data->exec);
    }
    {
        std::lock_guard lk(private_data->operation_state->active_channel_m);
        private_data->operation_state->active_sync_id = Failure;
    }
    if (is_cancel_requested(private_data->operation_state)) {
        return make_cancelled_error();
    }
    if (*err) {
        return WRAP_ERROR(std::move(*err));
    }
    return Error::ok();
}

static Error
import_reading_lists_blocking(std::shared_ptr<ImportFromPrivateData> private_data)
{
    if (Error cancel_err = check_cancel_requested(private_data->operation_state); cancel_err) {
        return cancel_err;
    }
    Error err = import_from_collection_blocking<ReadingListPayload>(
        private_data,
        "Reading Lists",
        [private_data](const std::shared_ptr<Payload>& payload) -> Error {
            if (!payload || payload->empty()) {
                return Error::ok();
            }
            orion::ReadingListEntryDto item;
            Error err = import_payload_to_item(payload, &item);
            if (err) return WRAP_ERROR(std::move(err));
            private_data->reading_list_entries.push_back(item);
            return Error::ok();
        }
    );
    if (err) {
        return WRAP_ERROR(std::move(err));
    }
    return Error::ok();
}

static Error
import_bookmark_folders_blocking(std::shared_ptr<ImportFromPrivateData> private_data)
{
    if (Error cancel_err = check_cancel_requested(private_data->operation_state); cancel_err) {
        return cancel_err;
    }
    Error err = import_from_collection_blocking<BookmarkFolderPayload>(
        private_data,
        "Bookmark Folders",
        [private_data](const std::shared_ptr<Payload>& payload) -> Error {
            if (!payload || payload->empty()) {
                return Error::ok();
            }
            orion::BookmarkFolderDto item;
            Error err = import_payload_to_item(payload, &item);
            if (err) return WRAP_ERROR(std::move(err));
            private_data->bookmark_folders.push_back(item);
            return Error::ok();
        }
    );
    if (err) {
        return WRAP_ERROR(std::move(err));
    }
    return Error::ok();
}

static Error
import_bookmarks_blocking(std::shared_ptr<ImportFromPrivateData> private_data)
{
    if (Error cancel_err = check_cancel_requested(private_data->operation_state); cancel_err) {
        return cancel_err;
    }
    Error err = import_from_collection_blocking<BookmarkPayload>(
        private_data,
        "Bookmarks",
        [private_data](const std::shared_ptr<Payload>& payload) -> Error {
            if (!payload || payload->empty()) {
                return Error::ok();
            }
            orion::BookmarkDto item;
            Error err = import_payload_to_item(payload, &item);
            if (err) return WRAP_ERROR(std::move(err));
            private_data->bookmarks.push_back(item);
            return Error::ok();
        }
    );
    if (err) {
        return WRAP_ERROR(std::move(err));
    }
    return Error::ok();
}

static Error
import_windows_blocking(std::shared_ptr<ImportFromPrivateData> private_data)
{
    if (Error cancel_err = check_cancel_requested(private_data->operation_state); cancel_err) {
        return cancel_err;
    }
    Error err = import_from_collection_blocking<WindowPayload>(
        private_data,
        "Windows",
        [private_data](const std::shared_ptr<Payload>& payload) -> Error {
            if (!payload || payload->empty()) {
                return Error::ok();
            }
            SyncWindowItem item;
            Error err = import_payload_to_item(payload, &item);
            if (err) return WRAP_ERROR(std::move(err));
            private_data->windows.push_back(item);
            return Error::ok();
        }
    );
    if (err) {
        return WRAP_ERROR(std::move(err));
    }
    return Error::ok();
}

static Error
import_tabs_blocking(std::shared_ptr<ImportFromPrivateData> private_data)
{
    if (Error cancel_err = check_cancel_requested(private_data->operation_state); cancel_err) {
        return cancel_err;
    }
    Error err = import_from_collection_blocking<TabPayload>(
        private_data,
        "Tabs",
        [private_data](const std::shared_ptr<Payload>& payload) -> Error {
            if (!payload || payload->empty()) {
                return Error::ok();
            }
            orion::TabDto item;
            Error err = import_payload_to_item(payload, &item);
            if (err) return WRAP_ERROR(std::move(err));
            private_data->tabs.push_back(item);
            return Error::ok();
        }
    );
    if (err) {
        return WRAP_ERROR(std::move(err));
    }
    return Error::ok();
}

static async_task<orion::Error>
import_create_windows(std::shared_ptr<ImportFromPrivateData> private_data)
{
    if (Error cancel_err = check_cancel_requested(private_data->operation_state); cancel_err) {
        co_return cancel_err;
    }
    auto shared_reply = co_await async(
        invoke(private_data->api, &CoreApi::get_shared_pinned_tabs_window_id), private_data->profile_id
    );
    if (shared_reply.err) {
        co_return WRAP_ERROR(std::move(shared_reply.err));
    }
    private_data->target_shared_pinned_tabs_window_id = shared_reply.window_id;

    size_t idx = 0;
    const size_t total = private_data->windows.size();
    for (const auto& item : private_data->windows) {
        if (Error cancel_err = check_cancel_requested(private_data->operation_state); cancel_err) {
            co_return cancel_err;
        }
        orion::CreateWindowQuery query{
            .profile_id = private_data->profile_id,
            .name = item.name,
            .icon_name = item.icon_name,
            .keyboard_shortcut = item.keyboard_shortcut,
            .is_suspended = item.is_suspended,
            .seq_num = item.seq_num,
            .is_tab_group = item.is_tab_group,
            .selected_tab_id = orion::TabId(0), // selected tab is restored after tabs are created
            .has_ui_window = item.has_ui_window,
            .undo_redo_tab_step_seq_num = 0,
            .is_focus_mode = item.is_focus_mode,
            .is_private = false, // private window should never be synced nor imported
        };
        auto reply = co_await async(invoke(private_data->api, &CoreApi::create_window), query);
        if (reply.err) {
            co_return WRAP_ERROR(std::move(reply.err));
        }
        private_data->imported_to_created_window_ids[item.id] = reply.id;
        private_data->progress_handler(++idx, total, "Windows");
    }

    co_return Error::ok();
}

static async_task<orion::Error>
import_create_tabs(std::shared_ptr<ImportFromPrivateData> private_data)
{
    if (Error cancel_err = check_cancel_requested(private_data->operation_state); cancel_err) {
        co_return cancel_err;
    }
    if (private_data->target_shared_pinned_tabs_window_id.value == 0) {
        auto shared_reply = co_await async(
            invoke(private_data->api, &CoreApi::get_shared_pinned_tabs_window_id), private_data->profile_id
        );
        if (shared_reply.err) {
            co_return WRAP_ERROR(std::move(shared_reply.err));
        }
        private_data->target_shared_pinned_tabs_window_id = shared_reply.window_id;
    }

    struct PendingParentMapping {
        orion::TabId source_child_id;
        orion::TabId source_parent_id;
    };
    std::vector<PendingParentMapping> pending_parent_mappings;

    size_t idx = 0;
    const size_t total = private_data->tabs.size();
    for (const auto& item : private_data->tabs) {
        if (Error cancel_err = check_cancel_requested(private_data->operation_state); cancel_err) {
            co_return cancel_err;
        }
        orion::WindowId target_window_id = private_data->target_shared_pinned_tabs_window_id;
        auto window_it = private_data->imported_to_created_window_ids.find(item.window_id);
        if (window_it != private_data->imported_to_created_window_ids.end()) {
            target_window_id = window_it->second;
        }
        if (target_window_id.value == 0) {
            co_return MAKE_ERROR(ErrorCode::invalid_state, "Could not resolve target window for imported tab");
        }

        orion::CreateTabQuery query{
            .window_id = target_window_id,
            .name = item.name,
            .icon_name = item.icon_name,
            .is_pinned = item.is_pinned,
            .is_suspended = item.is_suspended,
            .seq_num = item.seq_num,
            .uri = item.uri,
            .session_state = {},
            .parent_tab_id = orion::TabId(0),
            .is_collapsed = item.is_collapsed,
            // back-forward/session state is intentionally not imported
            .back_forward_list_index = 0,
            .webkit_back_forward_list_index = 0,
            .merged_back_forward_list_length = 0,
            .snapshot_image = item.snapshot_image,
        };
        auto reply = co_await async(invoke(private_data->api, &CoreApi::create_tab), query);
        if (reply.err) {
            co_return WRAP_ERROR(std::move(reply.err));
        }

        private_data->imported_to_created_tab_ids[item.id] = reply.id;
        if (item.parent_tab_id.value != 0) {
            pending_parent_mappings.push_back(PendingParentMapping{
                .source_child_id = item.id,
                .source_parent_id = item.parent_tab_id,
            });
        }

        private_data->progress_handler(++idx, total, "Tabs");
    }

    for (const auto& parent_mapping : pending_parent_mappings) {
        if (Error cancel_err = check_cancel_requested(private_data->operation_state); cancel_err) {
            co_return cancel_err;
        }
        auto child_it = private_data->imported_to_created_tab_ids.find(parent_mapping.source_child_id);
        auto parent_it = private_data->imported_to_created_tab_ids.find(parent_mapping.source_parent_id);
        if (
            child_it == private_data->imported_to_created_tab_ids.end() ||
            parent_it == private_data->imported_to_created_tab_ids.end()
        ) {
            co_return MAKE_ERROR(ErrorCode::invalid_state, "Could not resolve imported tab parent mapping");
        }

        orion::UpdateTabQuery query{
            .id = child_it->second,
            .parent_tab_id = parent_it->second,
            .update_parent_tab_id = true,
        };
        auto reply = co_await async(invoke(private_data->api, &CoreApi::update_tab), query);
        if (reply) {
            co_return WRAP_ERROR(std::move(reply));
        }
    }

    for (const auto& item : private_data->windows) {
        if (Error cancel_err = check_cancel_requested(private_data->operation_state); cancel_err) {
            co_return cancel_err;
        }
        if (item.selected_tab_id.value == 0) {
            continue;
        }

        auto window_it = private_data->imported_to_created_window_ids.find(item.id);
        auto tab_it = private_data->imported_to_created_tab_ids.find(item.selected_tab_id);
        if (
            window_it == private_data->imported_to_created_window_ids.end() ||
            tab_it == private_data->imported_to_created_tab_ids.end()
        ) {
            continue;
        }

        orion::UpdateWindowQuery query{
            .id = window_it->second,
            .selected_tab_id = tab_it->second,
            .update_selected_tab_id = true,
        };
        auto reply = co_await async(invoke(private_data->api, &CoreApi::update_window), query);
        if (reply) {
            co_return WRAP_ERROR(std::move(reply));
        }
    }

    orion::WindowId selected_window_id = orion::WindowId(0);
    for (const auto& item : private_data->windows) {
        if (!item.is_profile_selected) {
            continue;
        }
        auto it = private_data->imported_to_created_window_ids.find(item.id);
        if (it != private_data->imported_to_created_window_ids.end()) {
            selected_window_id = it->second;
            break;
        }
    }
    if (selected_window_id.value != 0) {
        orion::SetSelectedWindowIdQuery query{
            .profile_id = private_data->profile_id,
            .window_id = selected_window_id,
        };
        auto reply = co_await async(invoke(private_data->api, &CoreApi::set_selected_window_id), query);
        if (reply) {
            co_return WRAP_ERROR(std::move(reply));
        }
    }

    co_return Error::ok();
}

static async_task<orion::Error>
import_create_reading_lists(std::shared_ptr<ImportFromPrivateData> private_data)
{
    if (Error cancel_err = check_cancel_requested(private_data->operation_state); cancel_err) {
        co_return cancel_err;
    }
    size_t idx = 0;
    const size_t total = private_data->reading_list_entries.size();
    for (const auto& item : private_data->reading_list_entries) {
        if (Error cancel_err = check_cancel_requested(private_data->operation_state); cancel_err) {
            co_return cancel_err;
        }
        orion::CreateReadingListEntryQuery query{
            .profile_id = private_data->profile_id,
            .added_on = item.added_on,
            .icon_name = item.icon_name,
            .title = item.title,
            .address = item.address,
            .referrer = item.referrer,
            .description = item.description,
        };
        auto reply = co_await async(invoke(private_data->api, &CoreApi::create_reading_list_entry), query);
        if (reply.err) {
            co_return WRAP_ERROR(std::move(reply.err));
        }
        private_data->progress_handler(++idx, total, "Reading Lists");
    }
    co_return Error::ok();
}

static async_task<orion::Error>
import_create_bookmark_folders(std::shared_ptr<ImportFromPrivateData> private_data)
{
    if (Error cancel_err = check_cancel_requested(private_data->operation_state); cancel_err) {
        co_return cancel_err;
    }
    std::vector<orion::BookmarkFolderDto> pending = private_data->bookmark_folders;
    size_t idx = 0;
    const size_t total = pending.size();

    // Map imported internal folders (Favorites, Bookmarks Bar, ...) to existing
    // internal folders in the target profile, instead of creating duplicates.
    for (auto it = pending.begin(); it != pending.end();) {
        if (Error cancel_err = check_cancel_requested(private_data->operation_state); cancel_err) {
            co_return cancel_err;
        }
        if (!it->is_internal || it->type == orion::BookmarkFolderType::normal) {
            ++it;
            continue;
        }

        auto reply = co_await async(
            invoke(private_data->api, &CoreApi::get_bookmark_folder_id_by_type),
            private_data->profile_id,
            it->type
        );
        if (reply.err) {
            co_return WRAP_ERROR(std::move(reply.err));
        }

        private_data->imported_to_created_bookmark_folder_ids[it->id] = reply.folder_id;
        it = pending.erase(it);
        ++idx;
        private_data->progress_handler(idx, total, "Bookmark Folders");
    }

    while (!pending.empty()) {
        if (Error cancel_err = check_cancel_requested(private_data->operation_state); cancel_err) {
            co_return cancel_err;
        }
        bool made_progress = false;
        for (auto it = pending.begin(); it != pending.end();) {
            if (Error cancel_err = check_cancel_requested(private_data->operation_state); cancel_err) {
                co_return cancel_err;
            }
            orion::BookmarkFolderId parent_folder_id = orion::BookmarkFolderId(0);
            if (it->parent_folder_id.value != 0) {
                auto parent = private_data->imported_to_created_bookmark_folder_ids.find(it->parent_folder_id);
                if (parent == private_data->imported_to_created_bookmark_folder_ids.end()) {
                    ++it;
                    continue;
                }
                parent_folder_id = parent->second;
            }

            orion::CreateBookmarkFolderQuery query{
                .profile_id = private_data->profile_id,
                .name = it->name,
                .parent_folder_id = parent_folder_id,
                .seq_num = it->seq_num,
            };
            auto reply = co_await async(invoke(private_data->api, &CoreApi::create_bookmark_folder), query);
            if (reply.err) {
                co_return WRAP_ERROR(std::move(reply.err));
            }

            private_data->imported_to_created_bookmark_folder_ids[it->id] = reply.folder_id;
            it = pending.erase(it);
            ++idx;
            made_progress = true;
            private_data->progress_handler(idx, total, "Bookmark Folders");
        }

        if (!made_progress) {
            co_return MAKE_ERROR(ErrorCode::invalid_state, "Could not resolve imported bookmark folder hierarchy");
        }
    }
    co_return Error::ok();
}

static async_task<orion::Error>
import_create_bookmarks(std::shared_ptr<ImportFromPrivateData> private_data)
{
    if (Error cancel_err = check_cancel_requested(private_data->operation_state); cancel_err) {
        co_return cancel_err;
    }
    size_t idx = 0;
    const size_t total = private_data->bookmarks.size();
    for (const auto& item : private_data->bookmarks) {
        if (Error cancel_err = check_cancel_requested(private_data->operation_state); cancel_err) {
            co_return cancel_err;
        }
        orion::BookmarkFolderId parent_folder_id = orion::BookmarkFolderId(0);
        if (item.parent_folder_id.value != 0) {
            auto parent = private_data->imported_to_created_bookmark_folder_ids.find(item.parent_folder_id);
            if (parent != private_data->imported_to_created_bookmark_folder_ids.end()) {
                parent_folder_id = parent->second;
            }
        }

        orion::CreateBookmarkQuery query{
            .profile_id = private_data->profile_id,
            .name = item.name,
            .address = item.address,
            .invocation_keyword = item.invocation_keyword,
            .parent_folder_id = parent_folder_id,
            .seq_num = item.seq_num,
        };
        auto reply = co_await async(invoke(private_data->api, &CoreApi::create_bookmark), query);
        if (reply.err) {
            co_return WRAP_ERROR(std::move(reply.err));
        }
        private_data->progress_handler(++idx, total, "Bookmarks");
    }
    co_return Error::ok();
}

static void
import_done(std::shared_ptr<ImportFromPrivateData> private_data, Error err)
{
    private_data->completion_handler(err);
}

static detached_task
import_from_task(std::shared_ptr<ImportFromPrivateData> private_data)
{
    typedef Error (*ImportBlockingTask)(std::shared_ptr<ImportFromPrivateData> private_data);
    ImportBlockingTask import_tasks[] = {
        import_reading_lists_blocking,
        import_bookmark_folders_blocking,
        import_bookmarks_blocking,
        import_windows_blocking,
        import_tabs_blocking,
    };

    Error err = Error::ok();
    for (auto task : import_tasks) {
        err = check_cancel_requested(private_data->operation_state);
        if (err) {
            break;
        }
        auto import_from_task_cb = async([](std::shared_ptr<ImportFromPrivateData> private_data, ImportBlockingTask task, std::function<void(orion::Error)> cb) -> void {
            dispatch(orion::on_disk_thread(), [private_data,task,cb]() {
                Error task_err = task(private_data);
                dispatch(orion::on_main_thread(), [cb,task_err]() {
                    cb(task_err);
                });
            });
        }, private_data, task);
        err = co_await import_from_task_cb;
        if (err) {
            break;
        }
    }

    typedef async_task<orion::Error> (*ImportCreateTask)(std::shared_ptr<ImportFromPrivateData> private_data);
    ImportCreateTask import_create_tasks[] = {
        import_create_windows,
        import_create_tabs,
        import_create_reading_lists,
        import_create_bookmark_folders,
        import_create_bookmarks,
    };

    for (auto task : import_create_tasks) {
        err = check_cancel_requested(private_data->operation_state);
        if(err) {
            break;
        }
        err = co_await task(private_data);
        if(err) {
            break;
        }
    }

    import_done(private_data, err);
}

SyncService::OperationHandle
SyncServiceImpl::import_from(ProfileId profile_id, const std::string& source, ProgressHandler progress_handler, ImportFromCompletionHandler completion_handler)
{
    auto operation_state = std::make_shared<SyncOperationState>();

    auto private_data = std::make_shared<ImportFromPrivateData>(ImportFromPrivateData{
        .profile_id = profile_id,
        .source = source,
        .progress_handler = progress_handler,
        .completion_handler = completion_handler,
        .sync_api = SyncApi::instance(),
        .api = m_api,
        .exec = std::make_unique<ChannelExec>(),
        .operation_state = operation_state,
        .reading_list_entries = {},
        .bookmark_folders = {},
        .bookmarks = {},
        .windows = {},
        .tabs = {},
        .imported_to_created_bookmark_folder_ids = {},
        .imported_to_created_window_ids = {},
        .imported_to_created_tab_ids = {},
        .target_shared_pinned_tabs_window_id = orion::WindowId(0),
    });
    import_from_task(private_data);
    return std::make_shared<SyncServiceHandleImpl>(operation_state);
}

std::shared_ptr<SyncService> SyncService::create(orion::CoreApi* api)
{
    return std::make_shared<SyncServiceImpl>(api);
}

}
