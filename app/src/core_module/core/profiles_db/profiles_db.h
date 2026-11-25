#pragma once

#include <common/src/error.h>
#include <core/db/db.h>
#include <core/dto/bookmark_dto.h>
#include <core/dto/history_entry_dto.h>
#include <core/dto/reading_list_entry_dto.h>
#include <core/dto/window_dto.h>
#include <core/dto/tab_dto.h>
#include <core/dto/window_tab_checkpoint.h>
#include <core/dto/profile_dto.h>
#include <core/dto/bookmark_folder_dto.h>
#include <core/dto/settings_dto.h>
#include <core/dto/tab_back_forward_list_dto.h>
#include <set>
#include <map>
#include <vector>
#include <atomic>
#include <core/dto/extension_dto.h>
#include <core/dto/snapshot_dto.h>
#include <core/dto/snapshot_back_forward_dto.h>
#include <core/dto/recently_closed_tab_dto.h>
#include <core/dto/recently_closed_window_dto.h>
#include <core/dto/undo_redo_tab_dto.h>
#include <core/dto/password_item_dto.h>
#include <core/dto/extension_storage_area_dto.h>
#include <common/src/dto/totp.h>
#include <core/dto/download_dto.h>
#include <core/dto/search_engine_dto.h>
#include <core/dto/filter_list_dto.h>
#include <core/dto/overflow_menu_item_dto.h>
#include <core/dto/favicon_dto.h>
#include <core/dto/toolbar_item_dto.h>
#include <core/dto/programmable_button_dto.h>

namespace orion {

struct BookmarkSeqNumExistsQuery;
struct BookmarkSeqNumExistsReply;
struct RefineBookmarkSeqNumQuery;
struct BookmarkHierarchy;
struct RefineTabSeqNumQuery;
struct RefineWindowSeqNumQuery;
class PasswordStorage;

struct BookmarkItemKey {
    BookmarkFolderId parent_folder_id{};
    double seq_num{};

    bool
    operator<(const BookmarkItemKey& other) const
    {
        return std::tie(parent_folder_id, seq_num) < std::tie(other.parent_folder_id, other.seq_num);
    }
};

using BookmarkItemId = std::variant<BookmarkFolderId, BookmarkId, nullptr_t>;

enum class TxMode {
    no_tx,  // do not use transaction
    use_tx, // begin and commit a new transaction
};

struct BookmarkItem {
    int64_t id = 0;
    BookmarkItemType type = BookmarkItemType::not_set;
    std::vector<BookmarkItemKey> children{}; // for folders, contains child bookmarks and folders
    BookmarkFolderId parent_folder_id{};
    double seq_num = 0.0;
};

class ProfilesDb : public std::enable_shared_from_this<ProfilesDb> {
public:
    static std::shared_ptr<ProfilesDb> create(const char* app_id, PasswordStorage* password_storage);

    void set_password_storage(PasswordStorage* password_storage);

    Error open(const std::string& path);
    Error migrate();
    Error fix_old_migrations(std::shared_ptr<Db> db);
    std::shared_ptr<Db> db() const { return m_db; }

    //-------------------------------------------------------------------------
    // Profiles
    //-------------------------------------------------------------------------

    Error get_profile_ids(std::vector<ProfileId>* ids);
    Error get_default_profile_id(ProfileId* profile_id);
    Error get_internal_profile_id(ProfileId* profile_id);
    Error get_profiles(const std::vector<ProfileId>& ids, std::vector<ProfileDto>* profiles);
    Error create_profile(
        CreateProfileQuery query, ProfileId* profile_id, BookmarkFoldersChange* change,
        ProfilesChange* profiles_change, OverflowMenuItemsChange* overflow_menu_items_change,
        ToolbarItemsChange* toolbar_items_change, ProgrammableButtonsChange* prog_buttons_change
    );
    Error update_profile(UpdateProfileQuery query, ProfilesChange* profiles_change);
    Error move_profile(MoveProfileQuery query, ProfilesChange* profiles_change);
    Error set_default_profile(ProfileId profile_id, ProfilesChange* profiles_change);
    Error delete_profiles(
        const std::vector<ProfileId>& ids, ProfilesChange* profiles_change, WindowsChange* windows_change,
        TabsChange* tabs_change
    );
    Error set_selected_window_id(SetSelectedWindowIdQuery query, ProfilesChange* profiles_change);
    Error get_selected_window_id(ProfileId profile_id, WindowId* window_id, bool* exists, bool* is_private);
    Error reset_selected_window_ids(const std::vector<WindowId>& window_ids, ProfilesChange* profiles_change);
    Error window_id_get_profile_id(const WindowId window_id, ProfileId* profile_id);
    Error profile_id_get_shared_pinned_tabs_window_id(const ProfileId profile_id, WindowId* window_id);

    //-------------------------------------------------------------------------
    // Bookmark folders
    //-------------------------------------------------------------------------

    Error get_bookmark_folder_ids(ProfileId profile_id, std::vector<BookmarkFolderId>* ids);
    Error get_bookmark_folders(const std::vector<BookmarkFolderId>& ids, std::vector<BookmarkFolderDto>* folders);
    Error get_bookmark_folder_id_by_type(ProfileId profile_id, BookmarkFolderType type, BookmarkFolderId* folder_id);
    Error create_bookmark_folder(CreateBookmarkFolderQuery query, BookmarkFolderId* folder_id, BookmarkFoldersChange* change);
    Error update_bookmark_folder(UpdateBookmarkFolderQuery query, BookmarkFoldersChange* change);
    Error delete_bookmark_folders(
        const std::vector<BookmarkFolderId>& ids, BookmarkFoldersChange* change, BookmarksChange* bookmarks_change
    );
    Error copy_bookmarks(
        CopyBookmarksQuery query, CopyBookmarksReply* reply, BookmarkFoldersChange* folder_change,
        BookmarksChange* bookmark_change
    );

    // If folder_id is nullptr, then root level folders and bookmarks will be returned
    Error get_bookmark_folder_children(
        ProfileId profile_id, const BookmarkFolderId* folder_id, std::vector<BookmarkItem>* children);

    // If parent_folder_id is nullptr, then only root level folders are taken into account
    Error find_unique_bookmark_folder_name(
        ProfileId profile_id,
        const std::string& folder_name_prefix,
        const BookmarkFolderId* parent_folder_id,
        std::string* out_unique_name);

    //-------------------------------------------------------------------------
    // Bookmarks
    //-------------------------------------------------------------------------

    Error get_bookmark_ids(GetBookmarkIdsQuery query, std::vector<BookmarkId>* ids);
    Error get_recent_bookmark_ids(GetRecentBookmarkIdsQuery query, std::vector<BookmarkId>* ids);
    Error get_bookmarks(GetBookmarksQuery query, std::vector<BookmarkDto>* bookmarks);
    Error create_bookmark(CreateBookmarkQuery query, BookmarkId* bookmark_id, BookmarksChange* change);
    Error update_bookmark(UpdateBookmarkQuery query, BookmarksChange* change);
    Error delete_bookmarks(const std::vector<BookmarkId>& ids, BookmarksChange* change);
    Error move_bookmark(MoveBookmarkQuery query, BookmarksChange* change);
    Error move_bookmark_folder(MoveBookmarkFolderQuery query, BookmarkFoldersChange* change);
    Error normalize_bookmark_seq_num(
        ProfileId profile_id, BookmarkFolderId parent_folder_id, BookmarksChange* bookmarks_change,
        BookmarkFoldersChange* folders_change
    );
    Error is_current_tab_bookmarked(IsCurrentTabBookmarkedQuery query, IsCurrentTabBookmarkedReply* out_reply);

    //-------------------------------------------------------------------------
    // History entries
    //-------------------------------------------------------------------------

    Error get_history_entry_ids(GetHistoryEntryIdsQuery query, std::vector<HistoryEntryId>* ids);
    Error get_history_entries(const std::vector<HistoryEntryId>& ids, std::vector<HistoryEntryDto>* entries);
    Error get_nonempty_history_days(GetNonemptyHistoryDaysQuery query, std::vector<HistoryEntryDay>* days);
    Error get_history_entries(const ProfileHistoryQuery& query, std::vector<HistoryEntryDto>* entries);
    Error create_history_entry(
        CreateHistoryEntryQuery query, HistoryEntryId* entry_id, HistoryEntriesChange* entries_change,
        HistoryEntryDaysChange* days_change
    );
    Error delete_history_entries(
        const std::vector<HistoryEntryId>& ids, HistoryEntriesChange* entries_change,
        HistoryEntryDaysChange* days_change
    );
    Error clear_history_entries(const ProfileHistoryQuery& query, HistoryEntryDaysChange* days_change);
    
    //-------------------------------------------------------------------------
    // Reading list entries
    //-------------------------------------------------------------------------

    Error get_reading_list_entry_ids(GetReadingListEntryIdsQuery query, std::vector<ReadingListEntryId>* ids);
    Error get_reading_list_entries(const std::vector<ReadingListEntryId>& ids, std::vector<ReadingListEntryDto>* entries);
    Error create_reading_list_entry(CreateReadingListEntryQuery query, ReadingListEntryId* entry_id, ReadingListEntriesChange* change);
    Error update_reading_list_entry(UpdateReadingListEntryQuery query, ReadingListEntriesChange* change);
    Error delete_reading_list_entries(const std::vector<ReadingListEntryId>& ids, ReadingListEntriesChange* change);

    //-------------------------------------------------------------------------
    // Windows
    //-------------------------------------------------------------------------

    //intentionally does not return the shared_pinned_tabs_window_id
    Error get_window_ids(GetWindowIdsQuery query, std::vector<WindowId>* ids);
    Error get_windows(const std::vector<WindowId>& ids, std::vector<WindowDto>* windows);
    WindowId allocate_window_id();
    Error create_window(CreateWindowQuery query, WindowId* window_id, WindowsChange* change);
    Error create_window(
        WindowId window_id, CreateWindowQuery query, WindowId* out_window_id, WindowsChange* change);
    Error save_window(
        SaveWindowQuery query, WindowsChange* window_change, TabsChange* tab_change,
        TabBackForwardListEntriesChange* back_forward_list_change
    );
    Error update_window(UpdateWindowQuery query, WindowsChange* window_change);
    Error move_window(MoveWindowQuery query, WindowsChange* window_change);
    Error delete_windows(
        const std::vector<WindowId>& ids, TxMode tx_mode, WindowsChange* change, TabsChange* tabs_change,
        ProfilesChange* profiles_change
    );
    Error close_window(
        WindowId window_id, TxMode tx_mode, WindowsChange* change, TabsChange* tabs_change, ProfilesChange* profiles_change,
        RecentlyClosedTabsChange* recently_closed_tabs_change
    );
    Error duplicate_window(
        DuplicateWindowQuery query, WindowId* new_window_id, WindowsChange* windows_change, TabsChange* tabs_change
    );
    Error ensure_window_state_after_tab_move(
        WindowId window_id, TxMode tx_mode, WindowsChange* windows_change, TabsChange* tabs_change, ProfilesChange* profiles_change,
        RecentlyClosedTabsChange* recently_closed_tabs_change
    );
    Error verify_windows_change(WindowsChange* change);
    Error apply_window_tab_checkpoint(const WindowTabCheckpoint& checkpoint,
        WindowsChange* windows, TabsChange* tabs, ProfilesChange* profiles);

    //-------------------------------------------------------------------------
    // Tabs
    //-------------------------------------------------------------------------

    Error get_tab_ids(GetTabIdsQuery query, std::vector<TabId>* ids);
    Error get_other_tab_ids(
        GetOtherTabIdsQuery query, std::vector<TabId>* other_ids, std::vector<TabId>* before_ids,
        std::vector<TabId>* after_ids
    );
    Error get_descendant_tab_ids(TabId tab_id, std::vector<TabId>* descendant_ids);
    Error get_tabs(const std::vector<TabId>& ids, std::vector<TabDto>* tabs);
    TabId allocate_tab_id();
    Error create_tab(
        CreateTabQuery query, TabId* tab_id, TxMode tx_mode, TabsChange* change, WindowsChange* window_change
    );
    Error create_tab(
        TabId tab_id, CreateTabQuery query, TabId* out_tab_id, TxMode tx_mode,
        TabsChange* change, WindowsChange* window_change
    );
    Error update_tab(
        UpdateTabQuery query, TxMode tx_mode, TabsChange* change, WindowsChange* window_change
    );
    Error delete_tabs(const std::vector<TabId>& ids, TxMode tx_mode, TabsChange* change, WindowsChange* window_change);
    Error duplicate_tab_with_new_window_id(
        DuplicateTabWithNewWindowIdQuery query, TabDto* new_tab, TabsChange* change, WindowsChange* window_change,
        TabBackForwardListEntriesChange* tab_back_forward_list_entries_change, TxMode tx_mode
    );
    Error duplicate_tab(DuplicateTabQuery query, TabDto* new_tab, TabsChange* change, WindowsChange* window_change);
    Error suspend_tabs(const std::vector<TabId>& ids, TabsChange* change);
    Error can_move_tab(CanMoveTabQuery query, bool* can_move);
    Error move_tab(
        MoveTabQuery query, TxMode tx_mode, TabsChange* tab_change, WindowsChange* window_change, ProfilesChange* profile_change,
        RecentlyClosedTabsChange* recently_closed_tabs_change
    );
    Error normalize_tab_seq_nums(WindowId window_id, TxMode tx_mode, TabsChange* change);

    //-------------------------------------------------------------------------
    // Tab back-forward list entries
    //-------------------------------------------------------------------------

    Error get_tab_back_forward_list_entries(TabId tab_id, std::vector<TabBackForwardListEntryDto>* entries);
    Error create_tab_back_forward_list_entry(
        CreateTabBackForwardListEntryQuery query, TabBackForwardListEntryDto* entry,
        TabBackForwardListEntriesChange* change
    );
    Error delete_tab_back_forward_list_entries(
        DeleteTabBackForwardListEntriesQuery query, TabBackForwardListEntriesChange* change
    );

    //-------------------------------------------------------------------------
    // Recently Closed Tabs
    //-------------------------------------------------------------------------

    Error get_recently_closed_tab_ids(GetRecentlyClosedTabIdsQuery query, std::vector<RecentlyClosedTabId>* ids);
    Error get_recently_closed_tabs(
        const std::vector<RecentlyClosedTabId>& ids, std::vector<RecentlyClosedTabDto>* tabs
    );
    Error create_recently_closed_tab(
        CreateRecentlyClosedTabQuery query, RecentlyClosedTabId* id, RecentlyClosedTabsChange* change
    );
    Error delete_recently_closed_tabs(
        DeleteRecentlyClosedTabsQuery query, TxMode tx_mode, RecentlyClosedTabsChange* change
    );

    //-------------------------------------------------------------------------
    // Recently Closed Windows
    //-------------------------------------------------------------------------

    Error create_recently_closed_window(CreateRecentlyClosedWindowQuery query, RecentlyClosedWindowId* id);
    Error get_recently_closed_window_ids(ProfileId profile_id, std::vector<RecentlyClosedWindowId>* ids);
    Error get_recently_closed_windows(
        const std::vector<RecentlyClosedWindowId>& ids, std::vector<RecentlyClosedWindowDto>* windows
    );
    Error delete_recently_closed_windows(const std::vector<RecentlyClosedWindowId>& ids);

    //-------------------------------------------------------------------------
    // Settings
    //-------------------------------------------------------------------------

    Error set_setting(
        const SetSettingQuery& query,
        std::string* old_value,
        SettingsChange* settings_change,
        SingleSettingAnyChange* single_change
    );
    Error set_setting_or_default(
        const SetSettingOrDefaultQuery& query,
        std::string* old_value,
        std::string* default_value,
        SettingsChange* settings_change,
        SingleSettingAnyChange* single_change
    );
    Error increment_and_clamp_setting(
        const IncrementAndClampSettingQuery& query,
        int64_t* new_value,
        int64_t* old_value,
        SettingsChange* settings_change,
        SingleSettingAnyChange* single_change
    );
    Error append_to_string_setting(
        const AppendToStringSettingQuery& query,
        const TxMode tx_mode,
        std::string* old_value,
        std::string* default_value,
        SettingsChange* settings_change,
        SingleSettingAnyChange* single_change
    );
    Error remove_all_occurrences_of_string_from_setting(
        const RemoveAllOccurrencesOfStringFromSettingQuery& query,
        const TxMode tx_mode,
        std::string* old_value,
        std::string* default_value,
        SettingsChange* settings_change,
        SingleSettingAnyChange* single_change
    );
    Error set_common_setting(
        const SetCommonSettingQuery& query,
        std::string* old_value,
        CommonSettingsChange* settings_change,
        SingleCommonSettingAnyChange* single_change
    );
    Error get_setting(const GetSettingQuery& query, std::string* value);
    Error get_setting_or_default(const GetSettingOrDefaultQuery& query, std::string* value, std::string* default_value);
    Error get_common_setting(const GetCommonSettingQuery& query, std::string* value);
    Error get_setting_by_key(ProfileId profile_id, const std::string& key, std::optional<RawSettingDto>* out_setting);
    Error get_settings_by_key_prefix(ProfileId profile_id, const std::string& key_prefix, std::vector<RawSettingDto>* settings);
    Error get_setting_for_website_or_default(const GetSettingForWebsiteOrDefaultQuery& query, std::string* value);
    Error get_all_values_for_website_setting(const GetAllValuesForWebsiteSettingQuery& query, GetAllValuesForWebsiteSettingReply* reply);
    Error delete_setting_for_website(
        const DeleteSettingForWebsiteQuery& query,
        SettingsChange* settings_change,
        SingleSettingAnyChange* single_change
    );
    // Delete all settings rows for a profile
    Error delete_settings(ProfileId profile_id);

    //-------------------------------------------------------------------------
    // Extensions
    //-------------------------------------------------------------------------

    Error install_extension(InstallExtensionQuery query, ExtensionId* id, ExtensionsChange* change);
    Error get_installed_extension_ids(GetInstalledExtensionIdsQuery query, std::vector<ExtensionId>* ids);
    Error get_installed_extensions(GetInstalledExtensionsQuery query, std::vector<ExtensionDto>* extensions);
    Error update_extension(UpdateExtensionQuery query, ExtensionsChange* change);
    Error uninstall_extension(UninstallExtensionQuery query, ExtensionsChange* change);

    //-------------------------------------------------------------------------
    // Snapshots
    //-------------------------------------------------------------------------

    Error create_snapshot(ProfileId profile_id, TxMode tx_mode, UnixTimeUtc64* created_at);
    Error get_last_snapshot(ProfileId profile_id, SnapshotDto* snapshot);
    Error delete_snapshot(ProfileId profile_id);
    Error restore_snapshot(
        RestoreSnapshotQuery query, WindowsChange* windows_change, TabsChange* tabs_change,
        ProfilesChange* profiles_change, TabBackForwardListEntriesChange* back_forward_entries_change
    );

    //-------------------------------------------------------------------------
    // Snapshot tab back-forward list entries
    //-------------------------------------------------------------------------

    Error get_snapshot_tab_back_forward_list_entries(
        SnapshotTabId snapshot_tab_id,
        std::vector<SnapshotBackForwardEntry>* entries
    );

    Error create_snapshot_tab_back_forward_list_entry(SnapshotBackForwardEntry* entry);

    //-------------------------------------------------------------------------
    // Undo Redo Tab Steps
    //-------------------------------------------------------------------------

    Error create_undo_redo_tab_step(
        CreateUndoRedoTabStepQuery query, TxMode tx_mode, UndoRedoTabStepId* id, WindowsChange* windows_change
    );
    Error is_undo_redo_tab_step_available(
        IsUndoRedoTabStepAvailableQuery query, IsUndoRedoTabStepAvailableReply* reply
    );
    Error change_undo_redo_tab_step(
        ChangeUndoRedoTabStepQuery query, TxMode tx_mode, WindowsChange* windows_change, TabsChange* tabs_change,
        TabBackForwardListEntriesChange* back_forward_entries_change
    );
    Error get_undo_redo_tab_steps(
        const std::vector<UndoRedoTabStepId>& ids, std::vector<UndoRedoTabStepDto>* steps
    );
    Error get_undo_redo_tab_step_id(
        WindowId window_id, int64_t seq_num, UndoRedoTabStepId* id, bool* exists
    );
    Error get_undo_redo_tab_step_ids(
        WindowId window_id, std::vector<UndoRedoTabStepId>* ids
    );

    //-------------------------------------------------------------------------
    // Undo Redo Tabs
    //-------------------------------------------------------------------------

    Error create_undo_redo_tab(CreateUndoRedoTabQuery query, UndoRedoTabId* id);
    Error get_undo_redo_tab_ids(
        UndoRedoTabStepId step_id, std::vector<UndoRedoTabId>* ids
    );
    Error get_undo_redo_tabs(
        const std::vector<UndoRedoTabId>& ids, std::vector<UndoRedoTabDto>* tabs
    );
    Error update_undo_redo_tab(UpdateUndoRedoTabQuery query);

    //-------------------------------------------------------------------------
    // Undo Redo Tab Back-Forward List Entries
    //-------------------------------------------------------------------------

    Error create_undo_redo_tab_back_forward_list_entry(
        CreateUndoRedoTabBackForwardListEntryQuery query, UndoRedoTabBackForwardListEntryId* id
    );
    Error get_undo_redo_tab_back_forward_list_entries(
        UndoRedoTabId undo_redo_tab_id, std::vector<UndoRedoTabBackForwardListEntryDto>* entries
    );

    //-------------------------------------------------------------------------
    // Password Items
    //-------------------------------------------------------------------------

    Error create_password_item(
        CreatePasswordItemQuery query, PasswordItemId* id, PasswordItemsChange* change, bool* already_exists
    );
    Error get_password_item_ids(GetPasswordItemIdsQuery query, std::vector<PasswordItemId>* ids);
    Error get_password_items(const std::vector<PasswordItemId>& ids, std::vector<PasswordItemDto>* items);
    Error update_password_item(UpdatePasswordItemQuery query, PasswordItemsChange* change);
    Error delete_password_items(const std::vector<PasswordItemId>& ids, PasswordItemsChange* change);
    Error get_password_items_csv_export(const std::vector<PasswordItemId>& ids, std::string* out_text);
    Error import_password_items_from_csv(
        ImportPasswordItemsFromCsvQuery query, std::vector<PasswordItemDto>* out_imported,
        std::vector<PasswordItemDto>* out_not_imported, PasswordItemsChange* change
    );
    Error generate_totp_verification_code(
        GenerateTotpVerificationCodeQuery query, std::string* out_code
    );

    //-------------------------------------------------------------------------
    // Extension Storage Areas
    //-------------------------------------------------------------------------

    Error extension_storage_areas_get(
        ExtensionStorageAreas::GetQuery query, std::vector<ExtensionStorageAreas::KeyValue>* out_key_values
    );
    Error extension_storage_areas_get_all(
        ExtensionStorageAreas::GetAllQuery query, std::vector<ExtensionStorageAreas::KeyValue>* out_key_values
    );
    Error extension_storage_areas_get_keys(
        ExtensionStorageAreas::GetKeysQuery query, std::vector<std::string>* out_keys
    );
    Error extension_storage_areas_get_bytes_in_use(
        ExtensionStorageAreas::GetBytesInUseQuery query,
        uint64_t* out_bytes_in_use
    );
    Error extension_storage_areas_set(
        ExtensionStorageAreas::SetQuery query,
        std::vector<ExtensionStorageAreas::Change>* out_changes = nullptr
    );
    Error extension_storage_areas_remove(
        ExtensionStorageAreas::RemoveQuery query,
        std::vector<ExtensionStorageAreas::Change>* out_changes = nullptr
    );
    Error extension_storage_areas_clear(
        ExtensionStorageAreas::ClearQuery query,
        std::vector<ExtensionStorageAreas::Change>* out_changes = nullptr
    );
    Error extension_session_storage_clear(ExtensionId extension_id);
    Error extension_storage_access_level_get(
        ExtensionStorageAccessLevels::GetQuery query,
        bool* out_allow_untrusted_contexts
    );
    Error extension_storage_access_level_set(
        ExtensionStorageAccessLevels::SetQuery query
    );

    //-------------------------------------------------------------------------
    // Downloads
    //-------------------------------------------------------------------------

    Error get_download_ids(GetDownloadIdsQuery query, std::vector<DownloadId>* ids);
    Error get_downloads(const std::vector<DownloadId>& ids, std::vector<DownloadDto>* downloads);
    Error create_download(CreateDownloadQuery query, DownloadId* result_id, DownloadsChange* change);
    Error update_download(UpdateDownloadQuery query, DownloadsChange* download_change);
    Error delete_downloads(const DeleteDownloadsQuery& query, TxMode tx_mode, DownloadsChange* change);
    Error get_downloads_path(const ProfileId profile_id, bool* is_unset, std::string* path);
    Error reset_downloads_state();

    //-------------------------------------------------------------------------
    // Search Engines
    //-------------------------------------------------------------------------

    Error get_search_engine_ids(GetSearchEngineIdsQuery query, std::vector<SearchEngineId>* ids);
    Error get_search_engines(const std::vector<SearchEngineId>& ids, std::vector<SearchEngineDto>* search_engines);
    Error create_search_engine(CreateSearchEngineQuery query, SearchEngineId* out_id, SearchEnginesChange* change);
    Error update_search_engine(UpdateSearchEngineQuery query, SearchEnginesChange* change);
    Error delete_search_engines(const std::vector<SearchEngineId>& ids, TxMode tx_mode, SearchEnginesChange* change);
    Error match_search_engines(MatchSearchEnginesQuery query, std::vector<SearchEngineMatch>* out_matches);

    //-------------------------------------------------------------------------
    // Filter Lists
    //-------------------------------------------------------------------------

    Error create_or_update_filter_list(
        CreateFilterListQuery query, FilterListId* out_id, FilterListsChange* change);
    Error get_filter_lists(GetFilterListsQuery query, std::vector<FilterListDto>* out_filter_lists);
    Error delete_filter_lists(const std::vector<FilterListId>& ids, FilterListsChange* change);
    // Deletes relevant builtin filter lists in all profiles at the same time
    Error delete_unknown_builtin_filter_lists(
        const std::vector<std::string>& accounted_names,
        FilterListsChange* change);

    //-------------------------------------------------------------------------
    // Overflow Menu Items
    //-------------------------------------------------------------------------

    Error get_overflow_menu_items(GetOverflowMenuItemsQuery query, std::vector<OverflowMenuItemDto>* out_items);
    Error create_overflow_menu_item(
        CreateOverflowMenuItemQuery query, TxMode tx_mode, OverflowMenuItemId* out_id, OverflowMenuItemsChange* change
    );
    Error move_overflow_menu_item(MoveOverflowMenuItemQuery query, TxMode tx_mode, OverflowMenuItemsChange* change);
    Error delete_overflow_menu_items(
        const std::vector<OverflowMenuItemId>& ids, TxMode tx_mode, OverflowMenuItemsChange* change
    );

    //-------------------------------------------------------------------------
    // Toolbar Items
    //-------------------------------------------------------------------------

    Error get_toolbar_items(GetToolbarItemsQuery query, std::vector<ToolbarItemDto>* out_items);
    Error get_default_toolbar_items(GetDefaultToolbarItemsQuery query, std::vector<ToolbarItemDto>* out_items);
    Error get_default_overflow_menu_items(bool is_compact_tab_layout, std::vector<ToolbarItemDto>* out_items);
    Error get_available_toolbar_items(GetAvailableToolbarItemsQuery query, std::vector<ToolbarItemDto>* out_items);
    Error create_toolbar_item(
        CreateToolbarItemQuery query,
        TxMode tx_mode,
        ToolbarItemId* out_id,
        ToolbarItemsChange* toolbar_items_change,
        ProgrammableButtonsChange* prog_buttons_change
    );
    Error move_toolbar_item(MoveToolbarItemQuery query, TxMode tx_mode, ToolbarItemsChange* toolbar_items_change);
    Error delete_toolbar_items(
        const std::vector<ToolbarItemId>& ids,
        TxMode tx_mode,
        ToolbarItemsChange* toolbar_items_change,
        ProgrammableButtonsChange* prog_buttons_change
    );
    Error update_toolbar_item(
        UpdateToolbarItemQuery query,
        TxMode tx_mode,
        ToolbarItemsChange* change,
        ProgrammableButtonsChange* prog_buttons_change
    );
    Error reset_toolbar_items(
        ResetToolbarItemsQuery query,
        TxMode tx_mode,
        ToolbarItemsChange* change,
        ProgrammableButtonsChange* prog_buttons_change
    );

    //-------------------------------------------------------------------------
    // Programmable Buttons
    //-------------------------------------------------------------------------

    Error get_programmable_buttons(GetProgrammableButtonsQuery query, std::vector<ProgrammableButtonDto>* out_items);
    Error create_programmable_button( //should not be exposed in core api
        CreateProgrammableButtonQuery query, ProgrammableButtonId* out_id, ProgrammableButtonsChange* change
    );
    Error delete_programmable_buttons( //should not be exposed in core api
        const std::vector<ProgrammableButtonId>& ids, ProgrammableButtonsChange* change
    );
    Error update_programmable_button(
        UpdateProgrammableButtonQuery query, TxMode tx_mode, ProgrammableButtonsChange* change
    );

    //-------------------------------------------------------------------------
    // Database
    //-------------------------------------------------------------------------

    // commit() and rollback() are operations on tx transaction
    Error begin(DbTx* tx, int* result_code = nullptr);
    bool in_transaction() const;
    // Favicons
    //-------------------------------------------------------------------------

    Error get_favicon_for_page_url(const std::string& page_url, FaviconDto* favicon,
        bool* out_found, bool* out_expired);
    Error set_favicon_for_page_url(const SetFaviconQuery& query);
    Error prune_expired_favicons();

private:
    ProfilesDb(const char* app_id, PasswordStorage* password_storage); // private constructor to enforce use of create()
    std::shared_ptr<Db> extension_storage_areas_db(const std::string& storage_id) const;
    Error create_bookmark_folder_internal(
        CreateBookmarkFolderQuery query, bool is_internal, BookmarkFolderType type, BookmarkFolderId* folder_id,
        BookmarkFoldersChange* change
    );
    Error set_profile_default_settings(ProfileId profile_id);
    Error refine_bookmark_seq_num(RefineBookmarkSeqNumQuery query, double* seq_num);
    Error find_next_bookmark_seq_num(ProfileId profile_id, const DbVariant& parent_folder_id, double* next_seq_num);
    Error bookmark_seq_num_exists(BookmarkSeqNumExistsQuery query, BookmarkSeqNumExistsReply* reply);
    Error check_circular_reference_error(int result_code, const std::string& error_message) const;
    Error find_bookmark_folder_ascendants(BookmarkFolderId folder_id, std::set<BookmarkFolderId>* ascendants);
    // descendants will also contain the folder itself
    Error find_bookmark_folder_descendants(BookmarkFolderId folder_id, BookmarkHierarchy* descendants);
    Error copy_bookmark_folder_item(
        const BookmarkItem& descendant, BookmarkFolderId target_folder, BookmarkFoldersChange* folder_change,
        BookmarksChange* bookmark_change, std::map<BookmarkFolderId, BookmarkFolderId>* folder_map
    );
    double average(double a, double b) const { return (a + b) / 2.0; }
    std::string to_string(const std::vector<int64_t>& ids) const;
    std::string to_string(const std::vector<TabId>& ids) const;
    std::string to_string(const std::vector<BookmarkFolderId>& ids) const;
    std::string to_string(const std::vector<BookmarkId>& ids) const;
    std::string to_string(const std::set<BookmarkFolderId>& ids) const;
    std::string to_string(const std::set<BookmarkId>& ids) const;
    Error get_extension_seq_num_map(ProfileId profile_id, std::map<ExtensionId, double>* seq_nums);
    Error update_extensions_seq_num(
        ProfileId profile_id, ExtensionDto* extension, const std::map<ExtensionId, double>& old_seq_nums,
        ExtensionsChange* change
    );
    Error refine_tab_seq_num(RefineTabSeqNumQuery query, double* seq_num);
    Error refresh_window_num_tabs(WindowId window_id, WindowsChange* windows_change);
    Error refine_window_seq_num(RefineWindowSeqNumQuery query, double* seq_num);
    void track_window_id(WindowId window_id);
    void track_tab_id(TabId tab_id);
    Error find_invalid_tab_ids(const std::vector<TabId>& tab_ids, std::vector<TabId>* invalid_tab_ids);
    std::vector<TabId> to_unique_tab_ids(const std::vector<TabId>& tab_ids);
    Error get_tab_index(WindowId window_id, double seq_num, int64_t* index);
    Error create_recently_closed_tab_group(WindowDto window, RecentlyClosedTabsChange* change);
    Error remove_tab_from_nested_hierarchy(TabId tab_id, const std::vector<TabId>& ignore_changes_ids,
        TabsChange* change
    );
    Error check_tab_circular_reference(TabId tab_id, TabId new_parent_tab_id);
    Error fill_verification_code(PasswordItemDto* item);
    Error create_default_overflow_menu_items(ProfileId profile_id, OverflowMenuItemsChange* change);
    Error create_default_toolbar_items(ProfileId profile_id, TxMode tx_mode, ToolbarItemsChange* change, ProgrammableButtonsChange* prog_buttons_change);
    Error normalize_overflow_menu_item_indices(TxMode tx_mode, OverflowMenuItemsChange* change);
    Error normalize_toolbar_item_positions(TxMode tx_mode, ToolbarItemsChange* change);

private:
    std::vector<std::string> get_profile_columns() const;
    std::vector<std::string> get_window_columns() const;
    std::vector<std::string> get_tab_columns() const;
    std::vector<std::string> get_snapshot_tab_columns() const;
    std::vector<std::string> get_recently_closed_tab_columns() const;
    std::vector<std::string> get_recently_closed_window_columns() const;
    std::vector<std::string> get_snapshot_back_forward_entry_columns() const;
    std::vector<std::string> get_undo_redo_tab_columns() const;
    std::vector<std::string> get_undo_redo_tab_step_columns() const;
    std::vector<std::string> get_undo_redo_tab_back_forward_list_entry_columns() const;
    std::vector<std::string> get_download_columns() const;
    std::vector<std::string> get_search_engine_columns() const;
    std::vector<std::string> get_filter_list_columns() const;
    std::vector<std::string> get_overflow_menu_item_columns() const;
    std::vector<std::string> get_toolbar_item_columns() const;
    std::vector<std::string> get_programmable_button_columns() const;

    void load_profile_from_stmt(const DbStmt& stmt, ProfileDto* profile) const;
    void load_window_from_stmt(const DbStmt& stmt, WindowDto* window) const;
    void load_tab_from_stmt(const DbStmt& stmt, TabDto* tab) const;
    void load_snapshot_tab_from_stmt(const DbStmt& stmt, SnapshotTab* snapshot) const;
    void load_recently_closed_tab_from_stmt(const DbStmt& stmt, RecentlyClosedTabDto* tab) const;
    void load_recently_closed_window_from_stmt(const DbStmt& stmt, RecentlyClosedWindowDto* tab) const;
    void load_snapshot_back_forward_entry_from_stmt(const DbStmt& stmt, SnapshotBackForwardEntry* entry) const;
    void load_undo_redo_tab_from_stmt(const DbStmt& stmt, UndoRedoTabDto* undo_redo_tab) const;
    void load_undo_redo_tab_step_from_stmt(const DbStmt& stmt, UndoRedoTabStepDto* undo_redo_tab_step) const;
    void load_undo_redo_tab_back_forward_list_entry_from_stmt(const DbStmt& stmt,UndoRedoTabBackForwardListEntryDto* entry) const;
    void load_download_from_stmt(const DbStmt& stmt, DownloadDto* download) const;
    void load_search_engine_from_stmt(const DbStmt& stmt, SearchEngineDto* search_engine) const;
    void load_filter_list_from_stmt(const DbStmt& stmt, FilterListDto* filter_list) const;
    void load_overflow_menu_item_from_stmt(const DbStmt& stmt, OverflowMenuItemDto* item) const;
    void load_toolbar_item_from_stmt(const DbStmt& stmt, ToolbarItemDto* item) const;
    void load_programmable_button_from_stmt(const DbStmt& stmt, ProgrammableButtonDto* item) const;

private:
    std::shared_ptr<Db> m_db;
    std::shared_ptr<Db> m_extension_session_storage_db;
    std::string m_app_id;
    PasswordStorage* m_password_storage = nullptr;
    std::atomic<int64_t> m_max_window_id = 0;
    std::atomic<int64_t> m_max_tab_id = 0;
};

struct BookmarkSeqNumExistsQuery {
    // Fill this field with existing ID when updating bookmark or folder
    BookmarkItemId id = nullptr;

    ProfileId profile_id = ProfileId(0);

    // If set to std::nullopt, then id field must be set and id field is used to get
    // the parent folder ID of the existing bookmark item. If id field is nullptr, then
    // parent_folder_id must not be set std::nullopt.
    std::optional<DbVariant> parent_folder_id;
    std::optional<double> seq_num;
};

struct BookmarkSeqNumExistsReply {
    bool exists = false;
    ProfileId profile_id = ProfileId(0);
    BookmarkFolderId parent_folder_id = BookmarkFolderId(0);
    double seq_num = 0.0;
};

struct RefineBookmarkSeqNumQuery {
    ProfileId profile_id;
    BookmarkItemId existing_id;
    std::optional<DbVariant> parent_folder_id;
    BookmarkInsertMode insert_mode;
    std::optional<double> seq_num;
};

struct RefineTabSeqNumQuery {
    WindowId window_id;
    double seq_num;
};

struct RefineWindowSeqNumQuery {
    ProfileId profile_id;
    double seq_num;
    SeqNumCollisionMode collision_mode = SeqNumCollisionMode::after;
};

template <typename T>
static std::vector<DbVariant>
to_db_variant_vector(const std::vector<T>& ids)
{
    std::vector<DbVariant> result;
    result.reserve(ids.size());
    for (const auto& id : ids) {
        const auto& value = id.value;
        
        // note that push_back() call instead of emplace_back() can cause "maybe-uninitialized"
        // warning/error in optimized release builds here
        result.emplace_back(value);
    }
    return result;
}

} // namespace orion
