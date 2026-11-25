#pragma once

#include <common/src/error.h>
#include <memory>
#include <functional>
#include <chrono>
#include <type_traits>
#include <common/src/coalesce.h>
#include <core/dto/bookmark_dto.h>
#include <core/dto/bookmark_folder_dto.h>
#include <core/dto/history_entry_dto.h>
#include <core/dto/profile_dto.h>
#include <core/dto/reading_list_entry_dto.h>
#include <core/profiles_db/download_cleanup_service.h>
#include <core/dto/window_dto.h>
#include <core/dto/window_tab_checkpoint.h>
#include <core/dto/tab_dto.h>
#include <common/src/observable.h>
#include <core/dto/settings_dto.h>
#include <core/settings_types.h>
#include <core/dto/extension_dto.h>
#include <core/dto/tab_back_forward_list_dto.h>
#include <core/dto/snapshot_dto.h>
#include <core/dto/recently_closed_tab_dto.h>
#include <core/dto/undo_redo_tab_dto.h>
#include <core/webextensions/internal_store.h>
#include <core/webextensions/repository.h>
#include <core/webextensions/runtime.h>
#include <core/dto/password_item_dto.h>
#include <core/dto/extension_storage_area_dto.h>
#include <common/src/dto/totp.h>
#include <core/dto/download_dto.h>
#include <core/dto/search_engine_dto.h>
#include <core/dto/filter_list_dto.h>
#include <core/sync/sync_service.h>
#include <core/private_mode.h>
#include <core/dto/favicon_dto.h>
#include <core/dto/overflow_menu_item_dto.h>
#include <core/dto/toolbar_item_dto.h>
#include <core/dto/programmable_button_dto.h>
#include <core/webkit_bridge/API/bridge.h>
#include <core/notification_provider.h>

// New coroutine style (see coroutine.h for details):
#define AWAIT(function, ...) co_await async(invoke(global_core_api,&orion::CoreApi::function),##__VA_ARGS__)
#define API_AWAIT(core_api, function, ...) co_await async(invoke(core_api,&orion::CoreApi::function),##__VA_ARGS__)

namespace orion {

extern std::atomic<int> global_dispatch_arg_count;
extern std::atomic<int> global_core_api_count;
extern std::atomic<int> global_run_loop_count;

class Downloader;
class DownloaderFactory;
class PasswordStorage;
class NotificationProvider;
class CoreImpl;
class ProfilesDb;
class RunLoop;

template <typename T>
struct GetSettingReply {
    std::shared_ptr<settings::ISetting> setting{};
    T default_value{};
    T value{};
    Error err{};
};

template <typename T>
struct SetSettingReply {
    std::shared_ptr<settings::ISetting> setting{};
    T default_value{};
    T old_value{};
    T new_value{};
    Error err{};
};

struct CreateGetSettingQueryForWebsiteBoolArg {
    ProfileId profile_id{};
    std::string key_name{};
    bool default_value{};
    std::string website{};
};

struct CreateSetSettingQueryForWebsiteBoolArg {
    ProfileId profile_id{};
    std::string key_name{};
    bool value{};
    bool default_value{};
    std::string website{};
};

struct CreateSetSettingQueryForWebsiteIntegerArg {
    ProfileId profile_id{};
    std::string key_name{};
    int64_t value{};
    int64_t default_value{};
    std::string website{};
};

struct WindowRecord {
    UiWindowId ui_window_id;
    WindowId window_id;
    WindowId owner_window_id; // the window that owns (created) this UI window
};

using IterationCb = bool (*)(void* user_data);

template<typename T>
using Cb = std::function<void(T)>;

class HistoryCleanupService;
class FaviconCleanupService;
class FilterListUpdateService;
class CoreApi : public std::enable_shared_from_this<CoreApi> {
public:
    static std::shared_ptr<CoreApi> create(const char* app_id); // app_id can be defined in core CMakeLists.txt instead

    // profiles_db_path: optional explicit path for the profiles database
    // file; empty keeps the default <data dir>/profiles.db. Used by hosts
    // that maintain one database per profile (Windows).
    Error init(const std::string& profiles_db_path = {});
    void set_debug_mode(bool enabled) { m_debug_mode = enabled; }
    bool debug_mode() const { return m_debug_mode; }
    void set_password_storage(PasswordStorage* storage);
    PasswordStorage& password_storage() const;
    // Host-provided desktop notification presenter (notification_provider.h);
    // a no-op provider is used until one is set. Hosts report interaction
    // with shown notifications through notification_event().
    void set_notification_provider(NotificationProvider* provider);
    NotificationProvider& notification_provider() const;
    Observable<NotificationEvent>& notification_event() const { return *m_notification_event; }
    void set_downloader_factory(DownloaderFactory* factory);
    std::shared_ptr<Downloader> create_downloader();
    void set_filter_list_auto_update_handler(Cb<std::vector<FilterListDto>> handler);

    ~CoreApi();
    
    // The return value of iteration callback is ignored at the moment
    void set_iteration(IterationCb cb, void* user_data, std::chrono::milliseconds period);
    IterationCb get_iteration() const;
    void* get_iteration_user_data() const;
    bool run_iteration();

    // Blocking: runs the loop on the calling thread until shutdown.
    void start_run_loop();

    // Non-blocking alternative to start_run_loop() for environments where
    // a UI framework owns the thread (e.g. WinUI 3). Call once to start
    // background services, then drive the loop by calling tls_run_loop()->pump()
    // from a periodic timer on the same thread until pump() returns false.
    void prepare_run_loop();

    // The callback is called when the run loop successfully stops.
    // Do not call any CoreApi methods in the callback itself and
    // do not call CoreApi methods after the callback is called.
    Error request_stop_run_loop(std::function<void()> cb);

    //-------------------------------------------------------------------------
    // Profiles
    //-------------------------------------------------------------------------

    void get_profile_ids(Cb<GetProfileIdsReply> cb);
    void get_default_profile_id(Cb<GetDefaultProfileIdReply> cb);
    void get_internal_profile_id(Cb<GetInternalProfileIdReply> cb);
    void get_profiles(const std::vector<ProfileId>& ids, Cb<GetProfilesReply> cb);
    void get_all_profiles(Cb<GetProfilesReply> cb);

    // sets seq_num of the profile so that it is the last item
    void create_profile(CreateProfileQuery query, Cb<CreateProfileReply> cb);

    void update_profile(UpdateProfileQuery query, Cb<Error> cb);
    void move_profile(MoveProfileQuery query, Cb<Error> cb);
    void set_default_profile(ProfileId profile_id, Cb<Error> cb);

    // profiles with is_internal set to true are skipped (not deleted)
    void delete_profiles(DeleteProfilesQuery query, Cb<Error> cb);

    void delete_test_profiles(Cb<Error> cb);

    void delete_setting_for_website(DeleteSettingForWebsiteQuery query, Cb<Error> cb);
    
    // Delete all settings rows for a profile
    void delete_settings(ProfileId profile_id, Cb<Error> cb);

    Observable<ProfilesChange>& profiles_change() const { return *m_profiles_change; }
    void set_selected_window_id(SetSelectedWindowIdQuery query, Cb<Error> cb);

    // Returns window id of the most recently active window for the given profile.
    // If that window does not exist anymore, the window_id is set to the persisted
    // value but exists is set to false. If no windows exist for the profile or no
    // window is active, returns window_id 0 and exists is set to false.
    void get_selected_window_id(ProfileId profile_id, Cb<GetSelectedWindowIdReply> cb);
    void get_shared_pinned_tabs_window_id(ProfileId profile_id, Cb<GetSharedPinnedTabsWindowIdReply> cb);

    //-------------------------------------------------------------------------
    // Bookmark folders
    //-------------------------------------------------------------------------

    void get_bookmark_folder_ids(ProfileId profile_id, Cb<GetBookmarkFolderIdsReply> cb);
    void get_bookmark_folders(const std::vector<BookmarkFolderId>& ids, Cb<GetBookmarkFoldersReply> cb);
    void get_bookmark_folders_by_query(ProfileId profile_id, Cb<GetBookmarkFoldersReply> cb);
    void get_bookmark_folder_id_by_type(
        ProfileId profile_id, BookmarkFolderType type, Cb<GetBookmarkFolderIdByTypeReply> cb
    );

    // Returns ErrorCode::circular_reference if creating the folder would create a circular reference
    // inside the folder tree.
    void create_bookmark_folder(CreateBookmarkFolderQuery query, Cb<CreateBookmarkFolderReply> cb);

    // Folders with is_internal set to true are skipped (not updated).
    // Returns ErrorCode::circular_reference if updating the folder would create a circular reference
    // inside the folder tree.
    void update_bookmark_folder(UpdateBookmarkFolderQuery query, Cb<Error> cb);

    // folders with is_internal set to true are skipped (not deleted)
    void delete_bookmark_folders(const std::vector<BookmarkFolderId>& ids, Cb<Error> cb);

    Observable<BookmarkFoldersChange>& bookmark_folders_change() const { return *m_bookmark_folders_change; }

    // If both target_folder_id and target_bookmark_id are 0, the bookmarks are copied
    // to the root folder. If both are set, ErrorCode::argument error is returned.
    // If target_bookmark_id is set, the copied bookmarks and folders are appended
    // to the folder that contains the target bookmark.
    void copy_bookmarks(CopyBookmarksQuery query, Cb<CopyBookmarksReply> cb);

    // If folder_id is nullptr, then root level folders and bookmarks will be processed recursively
    Error get_bookmark_item_recursive(
        ProfileId profile_id,
        const BookmarkFolderId* folder_id,
        BookmarkItemDto* folder_out) const;

    Error create_bookmark_child_items_recursive(
        ProfileId profile_id,
        BookmarkFolderId parent_folder_id,
        BookmarkItemDto root_item,
        BookmarkFoldersChange* folders_change,
        BookmarksChange* bookmarks_change);
    
    void export_bookmarks_to_html(
        ProfileId profile_id,
        std::function<void(const std::string& html, Error err)> reply);

    void import_bookmarks_from_html(
        ProfileId profile_id,
        const std::string& html,
        const std::string& folder_name_prefix,
        std::function<void(int num_imported, Error err)> reply);

    //-------------------------------------------------------------------------
    // Bookmarks
    //-------------------------------------------------------------------------

    void get_bookmark_ids(GetBookmarkIdsQuery, Cb<GetBookmarkIdsReply> cb);

    // Returns at most 10 bookmark ids that were added most recently (descending).
    void get_recent_bookmark_ids(GetRecentBookmarkIdsQuery query, Cb<GetRecentBookmarkIdsReply> cb);

    void get_bookmarks(GetBookmarksQuery query, Cb<GetBookmarksReply> cb);
    void get_bookmarks_by_query(GetBookmarksByQuery query, Cb<GetBookmarksReply> cb);
    void create_bookmark(CreateBookmarkQuery query, Cb<CreateBookmarkReply> cb);
    void update_bookmark(UpdateBookmarkQuery query, Cb<Error> cb);
    void delete_bookmarks(const std::vector<BookmarkId>& ids, Cb<Error> cb);
    void move_bookmark(MoveBookmarkQuery query, Cb<Error> cb);
    void move_bookmark_folder(MoveBookmarkFolderQuery query, Cb<Error> cb);
    Observable<BookmarksChange>& bookmarks_change() const { return *m_bookmarks_change; }
    void is_current_tab_bookmarked(IsCurrentTabBookmarkedQuery query, Cb<IsCurrentTabBookmarkedReply> cb);

    //-------------------------------------------------------------------------
    // History entries
    //-------------------------------------------------------------------------

    // ordered by visited_at descending
    void get_history_entry_ids(GetHistoryEntryIdsQuery query, Cb<GetHistoryEntryIdsReply> cb);

    // Each start of a day is represented as 00:00:00 local system time of the particular day
    // that is converted to UnixTimeUtc64. Days are ordered by descending start time.
    void get_nonempty_history_days(GetNonemptyHistoryDaysQuery query, Cb<GetNonemptyHistoryDaysReply> cb);

    void get_history_entries(const std::vector<HistoryEntryId>& ids, Cb<GetHistoryEntriesReply> cb);

    // Note: visited_at fields in the reply are in local system time, not UTC.
    void get_history_entries(ProfileHistoryQuery query, Cb<GetHistoryEntriesReply> cb);

    void get_history_entries_by_query(GetHistoryEntryIdsQuery query, Cb<GetHistoryEntriesReply> cb);
    void create_history_entry(CreateHistoryEntryQuery query, Cb<CreateHistoryEntryReply> cb);
    void delete_history_entries(const std::vector<HistoryEntryId>& ids, Cb<Error> cb);
    void clear_history_entries(ProfileHistoryQuery query, Cb<Error> cb);
    Observable<HistoryEntriesChange>& history_entries_change() const { return *m_history_entries_change; }
    Observable<HistoryEntryDaysChange>& history_entry_days_change() const { return *m_history_entry_days_change; }

    //-------------------------------------------------------------------------
    // Reading list entries
    //-------------------------------------------------------------------------

    // ordered by added_on descending
    void get_reading_list_entry_ids(GetReadingListEntryIdsQuery query, Cb<GetReadingListEntryIdsReply> cb);

    // ordered by added_on descending
    void get_reading_list_entries(const std::vector<ReadingListEntryId>& ids, Cb<GetReadingListEntriesReply> cb);

    void get_reading_list_entries_by_query(GetReadingListEntryIdsQuery query, Cb<GetReadingListEntriesReply> cb);

    // If an entry with the same address already exists, it is updated (instead of creating a new entry) and
    // it is set to unread state. If an entry with the same address exists in unread state, nothing happens.
    void create_reading_list_entry(CreateReadingListEntryQuery query, Cb<CreateReadingListEntryReply> cb);

    void update_reading_list_entry(UpdateReadingListEntryQuery query, Cb<Error> cb);
    void delete_reading_list_entries(const std::vector<ReadingListEntryId>& ids, Cb<Error> cb);
    Observable<ReadingListEntriesChange>& reading_list_entries_change() const { return *m_reading_list_entries_change; }

    //-------------------------------------------------------------------------
    // Windows
    //-------------------------------------------------------------------------

    // ordered by profile_id ascending, then by seq_num ascending
    void get_window_ids(GetWindowIdsQuery query, Cb<GetWindowIdsReply> cb);

    // ordered by profile_id ascending, then by seq_num ascending
    void get_windows(const std::vector<WindowId>& ids, Cb<GetWindowsReply> cb);

    void get_windows_by_query(ProfileId profile_id, Cb<GetWindowsReply> cb);

    void create_window(CreateWindowQuery query, Cb<CreateWindowReply> cb);
    WindowId allocate_window_id();
    void db_create_window(WindowId window_id, CreateWindowQuery query, Cb<CreateWindowReply> cb);
    void db_apply_window_tab_checkpoint(WindowTabCheckpoint checkpoint, Cb<Error> cb);
    void update_window(UpdateWindowQuery query, Cb<Error> cb);
    void move_window(MoveWindowQuery query, Cb<Error> cb);
    void delete_windows(const std::vector<WindowId>& ids, Cb<Error> cb);
    Observable<WindowsChange>& windows_change() const { return *m_windows_change; }
    Observable<UiWindowsChange>& ui_windows_change() const { return *m_ui_windows_change; }

    // If the window's is_tab_group property is true, then this method deletes the window.
    // Otherwise (if it is a saved window), it ensures that at least one tab (Start Page)
    // is present in the window, and sets has_ui_window property of the window to false.
    // Does nothing if the window does not exist.
    // This should always be called with an owning window_id (the window that owns a UI window).
    void close_window(WindowId window_id, Cb<Error> cb);

    // Increases the internal count of UI windows. Returns new value of the count.
    int64_t ref_ui_window();

    // Decreases the internal count of UI windows. Returns new value of the count.
    // If the ref count is already zero, it is not decreased further and a warning is logged.
    int64_t unref_ui_window();

    int64_t get_ui_window_ref_count();

    // If the UI window id is already registered, a warning is logged and the id is not added again.
    void register_ui_window(UiWindowId id);

    // If the UI window id is not already registered, a warning is logged.
    void unregister_ui_window(UiWindowId id);

    void set_window_id_for_ui_window(UiWindowId ui_window_id, WindowId window_id);

    // Returns vector because there can be multiple UI windows for the same window_id.
    std::vector<UiWindowId> get_ui_windows_by_window_id(WindowId window_id) const;
    std::vector<UiWindowId> get_ui_window_ids() const;
    
    // Owner of the UI window id is basically the window (identified by window_id) that
    // creates the UI window (opened it in separate window).
    void set_owner_for_ui_window(UiWindowId ui_window_id, WindowId owner_window_id);

    WindowId get_owner_window_id_for_ui_window(UiWindowId ui_window_id) const;

    // Returns a vector but there should probably be at most one UI window owned by window_id.
    std::vector<UiWindowId> get_ui_window_ids_owned_by_window_id(WindowId owner_window_id) const;

    std::vector<UiWindowId> get_registered_ui_window_ids() const;
    void duplicate_window(DuplicateWindowQuery query, Cb<DuplicateWindowReply> cb);

    //-------------------------------------------------------------------------
    // Tabs
    //-------------------------------------------------------------------------

    // ordered by window_id, seq_num
    void get_tab_ids(GetTabIdsQuery query, Cb<GetTabIdsReply> cb);

    // Returns other tab ids in the same window as the given exclude_tab_id
    void get_other_tab_ids(GetOtherTabIdsQuery query, Cb<GetOtherTabIdsReply> cb);
    void get_descendant_tab_ids(TabId tab_id, Cb<GetDescendantTabIdsReply> cb);

    // ordered by window_id, seq_num
    void get_tabs(const std::vector<TabId>& ids, Cb<GetTabsReply> cb);
    
    void get_tabs_by_query(GetTabIdsQuery query, Cb<GetTabsReply> cb);
    void create_tab(CreateTabQuery query, Cb<CreateTabReply> cb);
    void delete_tabs(const std::vector<TabId>& ids, Cb<Error> cb);
    TabId allocate_tab_id();
    void db_create_tab(TabId tab_id, CreateTabQuery query, Cb<CreateTabReply> cb);
    void db_delete_tabs(const std::vector<TabId>& ids, Cb<Error> cb);
    void update_tab(UpdateTabQuery query, Cb<Error> cb);
    void update_tabs(const std::vector<UpdateTabQuery>& queries, Cb<Error> cb);
    void move_tab(MoveTabQuery query, Cb<Error> cb);
    Observable<TabsChange>& tabs_change() const { return *m_tabs_change; }
    void duplicate_tab(DuplicateTabQuery query, Cb<DuplicateTabReply> cb);
    void suspend_tabs(const std::vector<TabId>& ids, Cb<Error> cb);
    void can_move_tab(CanMoveTabQuery query, Cb<CanMoveTabReply> cb);

    //-------------------------------------------------------------------------
    // Tab back-forward list entries
    //-------------------------------------------------------------------------

    void get_tab_back_forward_list_entries(TabId tab_id, Cb<GetTabBackForwardListReply> cb);

    // an attempt to create an entry with duplicate webkit_index for the same tab_id will fail
    void create_tab_back_forward_list_entry(
        CreateTabBackForwardListEntryQuery query, Cb<CreateTabBackForwardListEntryReply> cb);

    // deletes entry with given start_webkit_index and all entries with higher start_webkit_index in the same tab_id
    void delete_tab_back_forward_list_entries(DeleteTabBackForwardListEntriesQuery query, Cb<Error> cb);

    Observable<TabBackForwardListEntriesChange>&
    tab_back_forward_list_entries_change() const
    {
        return *m_tab_back_forward_list_entries_change;
    }

    //-------------------------------------------------------------------------
    // Recently Closed Tabs
    //-------------------------------------------------------------------------

    // returns the tab ids ordered by closed_at descending
    void get_recently_closed_tab_ids(GetRecentlyClosedTabIdsQuery query, Cb<GetRecentlyClosedTabIdsReply> cb);

    // returns the tab ids ordered by closed_at descending
    void get_recently_closed_tabs(const std::vector<RecentlyClosedTabId>& ids, Cb<GetRecentlyClosedTabsReply> cb);

    void create_recently_closed_tab(CreateRecentlyClosedTabQuery query, Cb<CreateRecentlyClosedTabReply> cb);

    // does not report changes of other tabs (through recently_closed_tabs_change()) that had their window_id reset
    void delete_recently_closed_tabs(DeleteRecentlyClosedTabsQuery query, Cb<Error> cb);

    // not fully implemented - do not use (should not be needed at the moment)
    Observable<RecentlyClosedTabsChange>&
    recently_closed_tabs_change() const
    {
        return *m_recently_closed_tabs_change;
    }

    //-------------------------------------------------------------------------
    // Recently Closed Windows
    //-------------------------------------------------------------------------

    void get_recently_closed_window_ids(ProfileId profile_id, Cb<GetRecentlyClosedWindowIdsReply> cb);
    void get_recently_closed_windows(
        const std::vector<RecentlyClosedWindowId>& ids, Cb<GetRecentlyClosedWindowsReply> cb
    );
    void get_recently_closed_windows_by_query(ProfileId profile_id, Cb<GetRecentlyClosedWindowsReply> cb);

    // does not report changes of tabs (through recently_closed_tabs_change()) that are cascade deleted with the windows
    void delete_recently_closed_windows(const std::vector<RecentlyClosedWindowId>& ids, Cb<Error> cb);

    //-------------------------------------------------------------------------
    // Undo Redo Tab Steps
    //-------------------------------------------------------------------------

    void is_undo_redo_tab_step_available(IsUndoRedoTabStepAvailableQuery query, Cb<IsUndoRedoTabStepAvailableReply> cb);
    void create_undo_redo_tab_step(CreateUndoRedoTabStepQuery query, Cb<CreateUndoRedoTabStepReply> cb);
    void change_undo_redo_tab_step(ChangeUndoRedoTabStepQuery query, Cb<ChangeUndoRedoTabStepReply> cb);

    //-------------------------------------------------------------------------
    // Settings
    //-------------------------------------------------------------------------

    // Example usage (see also core_api_test.cpp):
    //     global_core_api->set_setting(ProfileId(1), settings::general::homepage, "https://example.com",
    //         [](SetSettingReply<std::string> reply) {
    //             VERIFY_LOG(reply.err);
    //             ...
    //         }
    //     );
    // See settings.h for all available settings keys.
    template <typename T>
    void
    set_setting(
        ProfileId profile_id, const settings::Setting<T>& setting, const T::value_type& value,
        Cb<SetSettingReply<typename T::value_type>> cb)
    {
        SetSettingQuery query{};
        query.profile_id = profile_id;
        query.key = setting.key();
        query.realm = setting.realm();
        query.key_name = setting.key_name();
        query.type = setting.type();
        query.value = settings::marshal_value(value);
        query.default_value = settings::marshal_value(setting.default_value());

        auto settings_change = std::make_shared<SettingsChange>();
        auto single_change = std::make_shared<SingleSettingAnyChange>();

        auto lambda = [guard=shared_from_this(), this, cb=std::move(cb), setting, settings_change, single_change]
            (SetSettingReply<std::string> reply)
        {
            SetSettingReply<typename T::value_type> typed_reply{};
            typed_reply.err = reply.err;

            if (reply.err) {
                cb(std::move(typed_reply));
                return;
            }

            typed_reply.default_value = setting.default_value();

            // fill old value
            typed_reply.err = settings::unmarshal_value<typename T::value_type>(reply.old_value, &typed_reply.old_value);
            if (typed_reply.err) {
                cb(std::move(typed_reply));
                return;
            }

            // fill new value
            typed_reply.err = settings::unmarshal_value<typename T::value_type>(reply.new_value, &typed_reply.new_value);
            if (typed_reply.err) {
                cb(std::move(typed_reply));
                return;
            }

            typed_reply.setting = setting.to_isetting();
            cb(std::move(typed_reply));
            // We cannot call notify calls here directly because that would expose
            // dispatch calls from observables breaking the compilation.
            notify_settings_change_internal(*settings_change, *single_change);
        };
        set_setting_internal(query, std::move(lambda), settings_change, single_change);
    }

    // See also set_setting
    template <typename T>
    void
    set_common_setting(
        const settings::Setting<T>& setting,
        const T::value_type& value,
        Cb<SetSettingReply<typename T::value_type>> cb)
    {
        SetCommonSettingQuery query{
            .key = setting.key(),
            .realm = setting.realm(),
            .key_name = setting.key_name(),
            .type = setting.type(),
            .value = settings::marshal_value(value),
            .default_value = settings::marshal_value(setting.default_value())
        };

        auto settings_change = std::make_shared<CommonSettingsChange>();
        auto single_change = std::make_shared<SingleCommonSettingAnyChange>();

        auto lambda = [guard=shared_from_this(),this,cb=std::move(cb),setting,settings_change,single_change]
            (SetSettingReply<std::string> reply)
        {
            SetSettingReply<typename T::value_type> typed_reply{};
            typed_reply.err = reply.err;

            if (reply.err) {
                cb(std::move(typed_reply));
                return;
            }

            typed_reply.default_value = setting.default_value();

            // fill old value
            typed_reply.err = settings::unmarshal_value<typename T::value_type>(reply.old_value, &typed_reply.old_value);
            if (typed_reply.err) {
                cb(std::move(typed_reply));
                return;
            }

            // fill new value
            typed_reply.err = settings::unmarshal_value<typename T::value_type>(reply.new_value, &typed_reply.new_value);
            if (typed_reply.err) {
                cb(std::move(typed_reply));
                return;
            }

            typed_reply.setting = setting.to_isetting();
            cb(std::move(typed_reply));
            // We cannot call notify calls here directly because that would expose
            // dispatch calls from observables breaking the compilation.
            notify_common_settings_change_internal(*settings_change, *single_change);
        };
        set_common_setting_internal(query, std::move(lambda), settings_change, single_change);
    }

    // Sets the setting to its default value. See also set_setting().
    template <typename T>
    void
    reset_setting(
        ProfileId profile_id,
        const settings::Setting<T>& setting,
        Cb<SetSettingReply<typename T::value_type>> cb)
    {
        set_setting(profile_id, setting, setting.default_value(), std::move(cb));
    }

    // Sets the setting to its default value. See also set_common_setting().
    template <typename T>
    void
    reset_common_setting(
        const settings::Setting<T>& setting,
        Cb<SetSettingReply<typename T::value_type>> cb)
    {
        set_common_setting(setting, setting.default_value(), std::move(cb));
    }

    template <typename T>
    void
    toggle_setting(
        ProfileId profile_id, const settings::Setting<T>& setting, Cb<SetSettingReply<typename T::value_type>> cb)
    {
        static_assert(
            std::is_same<typename T::value_type, bool>::value, "toggle_setting can only be used with boolean settings"
        );

        SetSettingQuery query{};
        query.profile_id = profile_id;
        query.key = setting.key();
        query.realm = setting.realm();
        query.key_name = setting.key_name();
        query.type = setting.type();
        // query.value field is ignored when toggling
        query.default_value = settings::marshal_value(setting.default_value());

        auto settings_change = std::make_shared<SettingsChange>();
        auto single_change = std::make_shared<SingleSettingAnyChange>();

        auto lambda = [=,guard=shared_from_this(),this,cb=std::move(cb)] (SetSettingReply<std::string> reply) {
            SetSettingReply<typename T::value_type> typed_reply{};
            typed_reply.err = reply.err;

            if (reply.err) {
                cb(std::move(typed_reply));
                return;
            }

            typed_reply.default_value = setting.default_value();

            // fill old value
            typed_reply.err = settings::unmarshal_value<typename T::value_type>(reply.old_value, &typed_reply.old_value);
            if (typed_reply.err) {
                cb(std::move(typed_reply));
                return;
            }

            // fill new value
            typed_reply.err = settings::unmarshal_value<typename T::value_type>(reply.new_value, &typed_reply.new_value);
            if (typed_reply.err) {
                cb(std::move(typed_reply));
                return;
            }

            typed_reply.setting = setting.to_isetting();
            cb(std::move(typed_reply));
            // We cannot call notify calls here directly because that would expose
            // dispatch calls from observables breaking the compilation.
            notify_settings_change_internal(*settings_change, *single_change);
        };
        toggle_setting_internal(query, std::move(lambda), settings_change, single_change);
    }

    // Non-template set_setting for use from C API (e.g. Windows P/Invoke).
    // Calls set_setting_internal and notifies observers on success.
    void set_setting_raw(const SetSettingQuery& query, Cb<SetSettingReply<std::string>> cb);
    void get_setting_raw(const GetSettingQuery& query, Cb<GetSettingReply<std::string>> cb);
    void get_setting_or_default_raw(const GetSettingOrDefaultQuery& query, Cb<GetSettingReply<std::string>> cb);
    void get_settings_by_key_prefix(ProfileId profile_id, const std::string& key_prefix, Cb<GetSettingsByKeyPrefixReply> cb);
    void get_setting_for_website_or_default(
        const GetSettingForWebsiteOrDefaultQuery& query, Cb<GetSettingForWebsiteOrDefaultReply> cb
    );
    void increment_and_clamp_setting(
        const IncrementAndClampSettingQuery& query, Cb<SetSettingReply<int64_t>> cb
    );
    void append_to_string_setting(
        const AppendToStringSettingQuery& query, Cb<SetSettingReply<std::string>> cb
    );
    void remove_all_occurrences_of_string_from_setting(
        const RemoveAllOccurrencesOfStringFromSettingQuery& query, Cb<SetSettingReply<std::string>> cb
    );
    void get_all_values_for_website_setting(
        const GetAllValuesForWebsiteSettingQuery& query, Cb<GetAllValuesForWebsiteSettingReply> cb
    );

    // Example usage (see also core_api_test.cpp):
    //     global_core_api->set_setting(ProfileId(1), settings::general::homepage,
    //         [](GetSettingReply<std::string> reply) {
    //             VERIFY_LOG(reply.err);
    //             reply.value // contains std::string value of the setting
    //             ...
    //         }
    //     );
    // See settings.h for all available settings keys.
    template <typename T>
    void
    get_setting(
        ProfileId profile_id,
        const settings::Setting<T>& setting,
        Cb<GetSettingReply<typename T::value_type>> cb)
    {
        GetSettingQuery query{};
        query.profile_id = profile_id;
        query.key = setting.key();
        query.realm = setting.realm();
        query.key_name = setting.key_name();
        query.type = setting.type();
        query.default_value = settings::marshal_value(setting.default_value());

        auto lambda = [cb=std::move(cb), setting](GetSettingReply<std::string> reply) {
            GetSettingReply<typename T::value_type> typed_reply{};
            typed_reply.err = reply.err;
            typed_reply.setting = setting.to_isetting();
            typed_reply.default_value = setting.default_value();

            if (reply.err) {
                cb(std::move(typed_reply));
                return;
            }

            // Convert the string value to the appropriate type
            typename T::value_type value{};
            Error err = settings::unmarshal_value(reply.value, &value);
            if (err) {
                typed_reply.err = err;
            } else {
                typed_reply.value = value;
            }

            cb(std::move(typed_reply));
        };

        get_setting_internal(query, std::move(lambda));
    }

    template <typename T, typename DefaultT>
    void
    get_setting_or_default(
        ProfileId profile_id,
        const settings::Setting<T>& setting,
        const settings::Setting<DefaultT>& default_setting,
        Cb<GetSettingReply<typename T::value_type>> cb)
    {
        static_assert(
            std::is_same_v<typename T::value_type, typename DefaultT::value_type>,
            "setting and default_setting must have the same value type"
        );

        GetSettingOrDefaultQuery query{
            .profile_id = profile_id,
            .key = setting.key(),
            .realm = setting.realm(),
            .key_name = setting.key_name(),
            .type = setting.type(),
            .default_setting_key = default_setting.key(),
            .default_setting_realm = default_setting.realm(),
            .default_setting_key_name = default_setting.key_name(),
            .default_setting_type = default_setting.type(),
            .default_value = settings::marshal_value(default_setting.default_value()),
        };

        auto lambda = [cb=std::move(cb), setting, default_setting](GetSettingReply<std::string> reply) {
            GetSettingReply<typename T::value_type> typed_reply{
                .setting = setting.to_isetting(),
                .default_value = default_setting.default_value(),
                .err = reply.err,
            };

            if (reply.err) {
                cb(std::move(typed_reply));
                return;
            }

            typename T::value_type default_value{};
            Error err = settings::unmarshal_value(reply.default_value, &default_value);
            if (err) {
                typed_reply.err = err;
                cb(std::move(typed_reply));
                return;
            }
            typed_reply.default_value = default_value;

            typename T::value_type value{};
            err = settings::unmarshal_value(reply.value, &value);
            if (err) {
                typed_reply.err = err;
            } else {
                typed_reply.value = value;
            }

            cb(std::move(typed_reply));
        };

        get_setting_or_default_raw(query, std::move(lambda));
    }

    template <typename T, typename DefaultT>
    void
    set_setting_or_default(
        ProfileId profile_id,
        const settings::Setting<T>& setting,
        const settings::Setting<DefaultT>& default_setting,
        const typename T::value_type& value,
        Cb<SetSettingReply<typename T::value_type>> cb)
    {
        static_assert(
            std::is_same_v<typename T::value_type, typename DefaultT::value_type>,
            "setting and default_setting must have the same value type"
        );

        SetSettingOrDefaultQuery query{
            .profile_id = profile_id,
            .key = setting.key(),
            .realm = setting.realm(),
            .key_name = setting.key_name(),
            .type = setting.type(),
            .value = settings::marshal_value(value),
            .default_setting_key = default_setting.key(),
            .default_setting_realm = default_setting.realm(),
            .default_setting_key_name = default_setting.key_name(),
            .default_setting_type = default_setting.type(),
            .default_value = settings::marshal_value(default_setting.default_value()),
        };

        auto settings_change = std::make_shared<SettingsChange>();
        auto single_change = std::make_shared<SingleSettingAnyChange>();

        auto lambda = [guard=shared_from_this(), this, cb=std::move(cb), setting, default_setting, settings_change, single_change]
            (SetSettingReply<std::string> reply)
        {
            SetSettingReply<typename T::value_type> typed_reply{};
            typed_reply.err = reply.err;

            if (reply.err) {
                cb(std::move(typed_reply));
                return;
            }

            // fill default value (resolved from default_setting in the database)
            typed_reply.err = settings::unmarshal_value<typename T::value_type>(reply.default_value, &typed_reply.default_value);
            if (typed_reply.err) {
                cb(std::move(typed_reply));
                return;
            }

            // fill old value
            typed_reply.err = settings::unmarshal_value<typename T::value_type>(reply.old_value, &typed_reply.old_value);
            if (typed_reply.err) {
                cb(std::move(typed_reply));
                return;
            }

            // fill new value
            typed_reply.err = settings::unmarshal_value<typename T::value_type>(reply.new_value, &typed_reply.new_value);
            if (typed_reply.err) {
                cb(std::move(typed_reply));
                return;
            }

            typed_reply.setting = setting.to_isetting();
            cb(std::move(typed_reply));
            // We cannot call notify calls here directly because that would expose
            // dispatch calls from observables breaking the compilation.
            notify_settings_change_internal(*settings_change, *single_change);
        };
        set_setting_or_default_internal(query, std::move(lambda), settings_change, single_change);
    }

    // See also get_setting
    template <typename T>
    void
    get_common_setting(
        const settings::Setting<T>& setting,
        Cb<GetSettingReply<typename T::value_type>> cb)
    {
        GetCommonSettingQuery query{
            .key = setting.key(),
            .realm = setting.realm(),
            .key_name = setting.key_name(),
            .type = setting.type(),
            .default_value = settings::marshal_value(setting.default_value())
        };

        auto lambda = [cb=std::move(cb), setting](GetSettingReply<std::string> reply) {
            GetSettingReply<typename T::value_type> typed_reply{
                .setting = setting.to_isetting(),
                .default_value = setting.default_value(),
                .err = reply.err,
            };

            if (reply.err) {
                cb(std::move(typed_reply));
                return;
            }

            // Convert the string value to the appropriate type
            typename T::value_type value{};
            Error err = settings::unmarshal_value(reply.value, &value);
            if (err) {
                typed_reply.err = err;
            } else {
                typed_reply.value = value;
            }

            cb(std::move(typed_reply));
        };

        get_common_setting_internal(query, std::move(lambda));
    }

    // Remove the observer afterwards like this:
    //     global_core_api->single_setting_change().remove_observer(observer_id);
    template <typename T>
    ObserverId
    add_single_setting_change_observer(
        const settings::Setting<T>& setting,
        const std::function<void(const SingleSettingChange<typename T::value_type>&)>& cb)
    {
        auto lambda = [cb=std::move(cb)](SingleSettingAnyChange any_change){
            SingleSettingChange<typename T::value_type> change{};
            change.profile_id = any_change.profile_id.value;
            change.key = any_change.key;
            change.realm = any_change.realm;
            change.key_name = any_change.key_name;
            change.type = any_change.type;

            typename T::value_type new_value{};
            VERIFY_LOG(settings::unmarshal_value(any_change.new_value, &new_value));
            change.new_value = new_value;

            typename T::value_type old_value{};
            VERIFY_LOG(settings::unmarshal_value(any_change.old_value, &old_value));
            change.old_value = old_value;

            typename T::value_type default_value{};
            VERIFY_LOG(settings::unmarshal_value(any_change.default_value, &default_value));
            change.default_value = default_value;

            cb(change);
        };
        return add_single_setting_change_observer_internal(setting.key(), std::move(lambda));
    }

    Observable<SettingsChange>& settings_change() const { return *m_settings_change; }
    Observable<SingleSettingAnyChange>& single_setting_change() const { return *m_single_setting_change; }

    std::string get_website_setting_realm() const;
    std::string get_website_setting_key(const std::string& website, const std::string& key_name) const;
    GetSettingQuery create_get_setting_query_for_website_bool(CreateGetSettingQueryForWebsiteBoolArg arg);
    SetSettingQuery create_set_setting_query_for_website_bool(CreateSetSettingQueryForWebsiteBoolArg arg);
    SetSettingQuery create_set_setting_query_for_website_integer(CreateSetSettingQueryForWebsiteIntegerArg arg);
    void reset_website_settings(ResetWebsiteSettings query, Cb<Error> cb);

    //-------------------------------------------------------------------------
    // Extensions
    //-------------------------------------------------------------------------

    WebExtensionsInternalStore& internal_store() { return *m_webextensions.internal_store.get(); }
    WebExtensionsRepository& repository() { return *m_webextensions.repository.get(); }
    [[nodiscard]] bridge::HostAttachment attach_profile_host(
        ProfileId profile_id, bridge::ProfileHost& host);
    std::weak_ptr<bridge::ProfileBridge> get_profile_bridge(
        ProfileId profile_id) const;
    std::weak_ptr<bridge::ProfileWorld> get_profile_world(
        ProfileId profile_id, PrivateMode private_mode) const;

    void install_extension(InstallExtensionQuery query, Cb<InstallExtensionReply> cb);
    void get_installed_extension_ids(GetInstalledExtensionIdsQuery query, Cb<GetAllInstalledExtensionIdsReply> cb);
    void get_installed_extensions(GetInstalledExtensionsQuery query, Cb<GetInstalledExtensionsReply> cb);
    void get_installed_extensions_by_query(GetInstalledExtensionIdsQuery query, Cb<GetInstalledExtensionsReply> cb);
    std::string get_extension_fetch_path(const WebExtensions::ExtensionInfo& extension);
    void update_extension(UpdateExtensionQuery query, Cb<UpdateExtensionReply> cb);
    void uninstall_extension(UninstallExtensionQuery query, Cb<UninstallExtensionReply> cb);
    Observable<ExtensionsChange>& extensions_change() const { return *m_extensions_change; }
    std::string app_id() const { return m_app_id; }

    //-------------------------------------------------------------------------
    // Snapshots
    //-------------------------------------------------------------------------

    void create_snapshot(ProfileId profile_id, Cb<CreateSnapshotReply> cb);

    // Returns default-constructed SnapshotDto if no snapshot exists for the profile.
    void get_last_snapshot(ProfileId profile_id, Cb<GetLastSnapshotReply> cb);

    void delete_snapshot(ProfileId profile_id, Cb<DeleteSnapshotReply> cb);

    // Restores previously created snapshot of tab group windows and tabs. On success,
    // before the snapshot is restored, a new snapshot is automatically created.
    void restore_snapshot(RestoreSnapshotQuery, Cb<RestoreSnapshotReply> cb);

    //-------------------------------------------------------------------------
    // Password Items
    //-------------------------------------------------------------------------

    void create_password_item(CreatePasswordItemQuery query, Cb<CreatePasswordItemReply> cb);
    // Creates a special password item that marks the website (URL) as "never save password" for the given profile
    void create_never_save_password_item(CreateNeverSavePasswordItemQuery query, Cb<CreateNeverSavePasswordItemReply> cb);
    void get_password_item_ids(GetPasswordItemIdsQuery query, Cb<GetPasswordItemIdsReply> cb);
    void get_password_items(const std::vector<PasswordItemId>& ids, Cb<GetPasswordItemsReply> cb);
    void get_password_items_by_query(GetPasswordItemIdsQuery query, Cb<GetPasswordItemsReply> cb);
    void get_never_save_password_item(GetNeverSavePasswordItemQuery query, Cb<GetNeverSavePasswordItemReply> cb);
    void update_password_item(UpdatePasswordItemQuery query, Cb<Error> cb);
    void delete_password_items(const std::vector<PasswordItemId>& ids, Cb<Error> cb);
    Observable<PasswordItemsChange>& password_items_change() const { return *m_password_items_change; }
    void get_password_items_csv_export(const std::vector<PasswordItemId>& ids, Cb<GetPasswordItemsCsvExportReply> cb);
    void import_password_items_from_csv(ImportPasswordItemsFromCsvQuery query, Cb<ImportPasswordItemsFromCsvReply> cb);
    // Time-Based One-Time Password (TOTP) generation
    Error generate_totp_verification_code(GenerateTotpVerificationCodeQuery query, std::string* out_code);

    //-------------------------------------------------------------------------
    // Extension Storage Areas
    //-------------------------------------------------------------------------

    void extension_storage_areas_get(ExtensionStorageAreas::GetQuery query, Cb<ExtensionStorageAreas::GetReply> cb);
    void extension_storage_areas_get_all(
        ExtensionStorageAreas::GetAllQuery query, Cb<ExtensionStorageAreas::GetAllReply> cb
    );
    void extension_storage_areas_get_keys(
        ExtensionStorageAreas::GetKeysQuery query, Cb<ExtensionStorageAreas::GetKeysReply> cb
    );
    void extension_storage_areas_get_bytes_in_use(
        ExtensionStorageAreas::GetBytesInUseQuery query,
        Cb<ExtensionStorageAreas::GetBytesInUseReply> cb
    );
    void extension_storage_areas_set(
        ExtensionStorageAreas::SetQuery query,
        Cb<ExtensionStorageAreas::MutationReply> cb
    );
    void extension_storage_areas_remove(
        ExtensionStorageAreas::RemoveQuery query,
        Cb<ExtensionStorageAreas::MutationReply> cb
    );
    void extension_storage_areas_clear(
        ExtensionStorageAreas::ClearQuery query,
        Cb<ExtensionStorageAreas::MutationReply> cb
    );
    void extension_session_storage_clear(ExtensionId extension_id, Cb<Error> cb);
    void extension_storage_access_level_get(
        ExtensionStorageAccessLevels::GetQuery query,
        Cb<ExtensionStorageAccessLevels::GetReply> cb
    );
    void extension_storage_access_level_set(
        ExtensionStorageAccessLevels::SetQuery query,
        Cb<Error> cb
    );

    //-------------------------------------------------------------------------
    // Downloads
    //-------------------------------------------------------------------------

    void create_download(CreateDownloadQuery query, Cb<CreateDownloadReply> cb);
    // For each query, if update_operation_state is true and operation_state is DownloadOperationState::finished and
    // settings::general::remove_download_items_mode is set to RemoveDownloadItemsMode::upon_successful_download,
    // then the download will be removed from the database during the update operation.
    void update_downloads(const std::vector<UpdateDownloadQuery>& queries, Cb<Error> cb);
    void get_download_ids(GetDownloadIdsQuery query, Cb<GetDownloadIdsReply> cb);
    void get_downloads(const std::vector<DownloadId>& ids, Cb<GetDownloadsReply> cb);
    void get_downloads_by_query(GetDownloadIdsQuery query, Cb<GetDownloadsReply> cb);
    void delete_downloads(const DeleteDownloadsQuery& query, Cb<DeleteDownloadsReply> cb);
    Observable<DownloadsChange>& downloads_change() const { return *m_downloads_change; }

    //-------------------------------------------------------------------------
    // Search Engines
    //-------------------------------------------------------------------------

    void get_search_engine_ids(GetSearchEngineIdsQuery query, Cb<GetSearchEngineIdsReply> cb);
    void get_search_engines(const std::vector<SearchEngineId>& ids, Cb<GetSearchEnginesReply> cb);
    void get_search_engines_by_query(GetSearchEngineIdsQuery query, Cb<GetSearchEnginesReply> cb);
    void create_search_engine(CreateSearchEngineQuery query, Cb<CreateSearchEngineReply> cb);
    void update_search_engine(UpdateSearchEngineQuery query, Cb<Error> cb);
    void delete_search_engines(const std::vector<SearchEngineId>& ids, Cb<Error> cb);
    Observable<SearchEnginesChange>& search_engines_change() const { return *m_search_engines_change; }
    // Tries to find matches in both search_engines and bookmarks tables
    void match_search_engines(MatchSearchEnginesQuery query, Cb<MatchSearchEnginesReply> cb);

    //-------------------------------------------------------------------------
    // Filter Lists
    //-------------------------------------------------------------------------

    void create_or_update_filter_list(CreateFilterListQuery query, Cb<CreateFilterListReply> cb);
    void get_filter_lists(GetFilterListsQuery query, Cb<GetFilterListsReply> cb);
    void delete_filter_lists(const std::vector<FilterListId>& ids, Cb<Error> cb);
    void parse_filter_list(LoadFilterListQuery query, Cb<LoadFilterListReply> cb);
    Observable<FilterListsChange>& filter_lists_change() const { return *m_filter_lists_change; }
    
    //-------------------------------------------------------------------------
    // Favicons
    //-------------------------------------------------------------------------

    void get_favicon(const std::string& page_url, Cb<GetFaviconReply> cb);
    void set_favicon(SetFaviconQuery query, Cb<Error> cb);

    //-------------------------------------------------------------------------
    // Overflow Menu Items
    //-------------------------------------------------------------------------

    void get_overflow_menu_items(GetOverflowMenuItemsQuery query, Cb<GetOverflowMenuItemsReply> cb);
    void create_overflow_menu_item(CreateOverflowMenuItemQuery query, Cb<CreateOverflowMenuItemReply> cb);
    void move_overflow_menu_item(MoveOverflowMenuItemQuery query, Cb<Error> cb);
    void delete_overflow_menu_items(const std::vector<OverflowMenuItemId>& ids, Cb<Error> cb);
    Observable<OverflowMenuItemsChange>& overflow_menu_items_change() const { return *m_overflow_menu_items_change; }

    //-------------------------------------------------------------------------
    // Toolbar Items
    //-------------------------------------------------------------------------

    void get_toolbar_items(GetToolbarItemsQuery query, Cb<GetToolbarItemsReply> cb);
    void get_default_toolbar_items(GetDefaultToolbarItemsQuery query, Cb<GetDefaultToolbarItemsReply> cb);
    void get_available_toolbar_items(GetAvailableToolbarItemsQuery query, Cb<GetAvailableToolbarItemsReply> cb);
    void create_toolbar_item(CreateToolbarItemQuery query, Cb<CreateToolbarItemReply> cb);
    void move_toolbar_item(MoveToolbarItemQuery query, Cb<Error> cb);
    void update_toolbar_item(UpdateToolbarItemQuery query, Cb<Error> cb);
    void delete_toolbar_items(const std::vector<ToolbarItemId>& ids, Cb<Error> cb);
    void reset_toolbar_items(ResetToolbarItemsQuery query, Cb<Error> cb);
    Observable<ToolbarItemsChange>& toolbar_items_change() const { return *m_toolbar_items_change; }

    //-------------------------------------------------------------------------
    // Programmable Buttons
    //-------------------------------------------------------------------------

    void get_programmable_buttons(GetProgrammableButtonsQuery query, Cb<GetProgrammableButtonsReply> cb);
    // Creates a new toolbar item that is a programmable button with specified properties
    void create_programmable_button(CreateProgrammableButtonQuery query, Cb<Error> cb);
    void update_programmable_button(UpdateProgrammableButtonQuery query, Cb<Error> cb);
    Observable<ProgrammableButtonsChange>& programmable_buttons_change() const { return *m_programmable_buttons_change; }

    //-------------------------------------------------------------------------
    // Other
    //-------------------------------------------------------------------------

    std::shared_ptr<SyncService> sync_service() {
        return m_sync_service;
    }

    std::shared_ptr<SyncService> make_sync_service() {
        return SyncService::create(this);
    }

    // Dispatches the work to the main run loop thread.
    void async(std::function<void()> work);

    // Dispatches the work to the main run loop thread after a period of time.
    void async_after(std::chrono::milliseconds after, std::function<void()> work);

    // Dispatches the work to the disk thread.
    void async_disk(std::function<void()> work);

    // Dispatches all the work functions on the main thread and when all of them are done, asynchronously
    // dispatches done_cb on the main thread with the first encountered error or no error if all work
    // functions succeeded.
    void when_all(std::vector<Cb<Cb<Error>>> fns, Cb<Error> done_cb);

private:
    CoreApi(const char* app_id); // private constructor to enforce use of create()

    void set_setting_internal(
        const SetSettingQuery& query,
        Cb<SetSettingReply<std::string>> cb,
        std::shared_ptr<SettingsChange> settings_change,
        std::shared_ptr<SingleSettingAnyChange> single_change
    );
    void set_common_setting_internal(
        const SetCommonSettingQuery& query,
        Cb<SetSettingReply<std::string>> cb,
        std::shared_ptr<CommonSettingsChange> settings_change,
        std::shared_ptr<SingleCommonSettingAnyChange> single_change
    );
    void toggle_setting_internal(
        const SetSettingQuery& query,
        Cb<SetSettingReply<std::string>> cb,
        std::shared_ptr<SettingsChange> settings_change,
        std::shared_ptr<SingleSettingAnyChange> single_change
    );
    void set_setting_or_default_internal(
        const SetSettingOrDefaultQuery& query,
        Cb<SetSettingReply<std::string>> cb,
        std::shared_ptr<SettingsChange> settings_change,
        std::shared_ptr<SingleSettingAnyChange> single_change
    );
    void get_setting_internal(const GetSettingQuery& query, Cb<GetSettingReply<std::string>> cb);
    void get_common_setting_internal(const GetCommonSettingQuery& query, Cb<GetSettingReply<std::string>> cb);
    ObserverId add_single_setting_change_observer_internal(const std::string& key, Cb<SingleSettingAnyChange> cb);
    void notify_settings_change_internal(
        const SettingsChange& settings_change,
        const SingleSettingAnyChange& single_change
    );
    void notify_common_settings_change_internal(
        const CommonSettingsChange& settings_change,
        const SingleCommonSettingAnyChange& single_change
    );
    void debug_log_window_records() const;
    Error validate_window_change(WindowsChange change);
    std::shared_ptr<bridge::ProfileBridge>
    get_or_create_profile_bridge(ProfileId profile_id);
    void remove_profile_bridge(bridge::ProfileBridge&);
    Error init_filter_lists();

private:
    std::string m_app_id;
    bool m_debug_mode = false;
    PasswordStorage* m_password_storage = nullptr;
    DownloaderFactory* m_downloader_factory = nullptr;
    NotificationProvider* m_notification_provider = nullptr;
    std::shared_ptr<ProfilesDb> m_profiles_db;
    std::shared_ptr<SyncService> m_sync_service;
    //this could've been unique_ptr if we didn't safeguard asynch callbacks with shared ptr ref guard
    std::shared_ptr<HistoryCleanupService> m_history_cleanup;
    std::shared_ptr<FaviconCleanupService> m_favicon_cleanup;
    std::shared_ptr<DownloadCleanupService> m_download_cleanup;
    std::shared_ptr<FilterListUpdateService> m_filter_list_update;
    struct {
        std::shared_ptr<WebExtensionsInternalStore> internal_store;
        std::shared_ptr<WebExtensionsRepository> repository;
        std::map<ProfileId, std::shared_ptr<bridge::ProfileBridge>> profile_bridges;
    } m_webextensions;
    std::shared_ptr<RunLoop> m_run_loop;
    int64_t m_ui_window_ref_count = 0;
    std::vector<WindowRecord> m_window_records;

    friend class bridge::ProfileBridge;

private:
    const std::unique_ptr<Observable<BookmarksChange>> m_bookmarks_change;
    const std::unique_ptr<Observable<BookmarkFoldersChange>> m_bookmark_folders_change;
    const std::unique_ptr<Observable<ProfilesChange>> m_profiles_change;
    const std::unique_ptr<Observable<HistoryEntriesChange>> m_history_entries_change;
    const std::unique_ptr<Observable<HistoryEntryDaysChange>> m_history_entry_days_change;
    const std::unique_ptr<Observable<ReadingListEntriesChange>> m_reading_list_entries_change;
    const std::unique_ptr<Observable<WindowsChange>> m_windows_change;
    const std::unique_ptr<Observable<TabsChange>> m_tabs_change;
    const std::unique_ptr<Observable<SettingsChange>> m_settings_change;
    const std::unique_ptr<Observable<CommonSettingsChange>> m_common_settings_change;
    const std::unique_ptr<Observable<SingleSettingAnyChange>> m_single_setting_change;
    const std::unique_ptr<Observable<SingleCommonSettingAnyChange>> m_single_common_setting_change;
    const std::unique_ptr<Observable<ExtensionsChange>> m_extensions_change;
    const std::unique_ptr<Observable<TabBackForwardListEntriesChange>> m_tab_back_forward_list_entries_change;
    const std::unique_ptr<Observable<UiWindowsChange>> m_ui_windows_change;
    const std::unique_ptr<Observable<RecentlyClosedTabsChange>> m_recently_closed_tabs_change;
    const std::unique_ptr<Observable<PasswordItemsChange>> m_password_items_change;
    const std::unique_ptr<Observable<DownloadsChange>> m_downloads_change;
    const std::unique_ptr<Observable<SearchEnginesChange>> m_search_engines_change;
    const std::unique_ptr<Observable<FilterListsChange>> m_filter_lists_change;
    const std::unique_ptr<Observable<OverflowMenuItemsChange>> m_overflow_menu_items_change;
    const std::unique_ptr<Observable<ToolbarItemsChange>> m_toolbar_items_change;
    const std::unique_ptr<Observable<ProgrammableButtonsChange>> m_programmable_buttons_change;
    const std::unique_ptr<Observable<NotificationEvent>> m_notification_event;
};

} // namespace orion
