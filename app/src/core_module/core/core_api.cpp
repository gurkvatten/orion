#define ENABLE_VERBOSE_LOGS 0

#include <common/src/log.h>
#include <core/core_api.h>
#include <core/password_storage.h>
#include <core/notification_provider.h>
#include <core/profiles_db/profiles_db.h>
#include <common/src/file_system.h>
#include <common/src/dispatch.h>
#include <common/src/run_loop.h>
#include <core/profiles_db/test.h>
#include <core/download_test.h>
#include <core/downloader.h>
#include <common/src/zip_test.h>
#include <common/src/string_util.h>
#include <common/src/dispatch_test.h>
#include <common/src/uuid.h>
#include <core/profiles_db/history_cleanup_service.h>
#include <core/profiles_db/favicon_cleanup_service.h>
#include <core/profiles_db/filter_list_update_service.h>
#include <core/webkit_bridge/API/bridge.h>
#include <common/src/filter_list.h>
#include <core/bookmark_html_exporter.h>
#include <core/bookmark_html_importer.h>
#include <core/filter_list_metadata.h>
#include <core/settings.h>

#include <iostream>
#include <algorithm>
#include <sstream>
#include <iomanip>

#define ENABLE_WINDOW_CHANGE_CHECK_FOR_SHARED_PINNED_TABS 1

#define ENABLE_SYNCH_TEST !NDEBUG && 0
#define ENABLE_NET_TEST !NDEBUG && 1
#if ENABLE_NET_TEST
#   include <sync/net/test/net_tests.h>
#endif
#define ENABLE_SYNC_TEST !NDEBUG && 0
#if ENABLE_SYNC_TEST
#   include <sync/test/sync_tests.h>
#endif
#define ENABLE_PROFILES_DB_TEST !NDEBUG && 1
#define ENABLE_DOWNLOAD_TEST !NDEBUG && 1
#define ENABLE_ZIP_TEST !NDEBUG && 1
#define ENABLE_ENUM_REFLECT_TEST !NDEBUG && 1
#if ENABLE_ENUM_REFLECT_TEST
#   include <common/src/enum_reflect_test.h>
#endif

#define ENABLE_SINGLE_SETTING_OBSERVABLE_LOGS 0

#if ENABLE_SINGLE_SETTING_OBSERVABLE_LOGS
#define single_setting_observable_log(format, ...) orion::log_debug(format, ##__VA_ARGS__)
#else
#define single_setting_observable_log(format, ...) orion::log_nop(format, ##__VA_ARGS__)
#endif

namespace orion {

namespace {

class NullPasswordStorage final : public PasswordStorage {
public:
    Error
    upsert_password_items(const char* app_id, const std::vector<PasswordItemDto>& items) override
    {
        log_error("password storage not set");
        SUCCEED();
    }

    Error
    get_password_items(const char* app_id, std::vector<PasswordItemDto>* out_items) override
    {
        if (out_items) {
            out_items->clear();
        }
        log_error("password storage not set");
        SUCCEED();
    }
};

NullPasswordStorage g_null_password_storage;

class NullDownloaderFactory final : public DownloaderFactory {
public:
    std::shared_ptr<Downloader>
    create(const char* app_id) override
    {
        return nullptr;
    }
};

NullDownloaderFactory g_null_downloader_factory;

class NullNotificationProvider final : public NotificationProvider {
public:
    Error
    show(const NotificationRequest& request) override
    {
        log_debug("notification provider not set; not displaying '%s'", request.notification_id.c_str());
        SUCCEED();
    }

    Error
    clear(NotificationSource, const std::string&, const std::string&) override
    {
        SUCCEED();
    }
};

NullNotificationProvider g_null_notification_provider;

} // namespace

std::atomic<int> global_core_api_count = 0;

CoreApi::CoreApi(const char* app_id) :
      m_app_id(app_id)
    , m_password_storage(&g_null_password_storage)
    , m_downloader_factory(&g_null_downloader_factory)
    , m_notification_provider(&g_null_notification_provider)
    , m_profiles_db(nullptr)
    , m_run_loop(nullptr)
    , m_bookmarks_change(std::make_unique<Observable<BookmarksChange>>())
    , m_bookmark_folders_change(std::make_unique<Observable<BookmarkFoldersChange>>())
    , m_profiles_change(std::make_unique<Observable<ProfilesChange>>())
    , m_history_entries_change(std::make_unique<Observable<HistoryEntriesChange>>())
    , m_history_entry_days_change(std::make_unique<Observable<HistoryEntryDaysChange>>())
    , m_reading_list_entries_change(std::make_unique<Observable<ReadingListEntriesChange>>())
    , m_windows_change(std::make_unique<Observable<WindowsChange>>())
    , m_tabs_change(std::make_unique<Observable<TabsChange>>())
    , m_settings_change(std::make_unique<Observable<SettingsChange>>())
    , m_common_settings_change(std::make_unique<Observable<CommonSettingsChange>>())
    , m_single_setting_change(std::make_unique<Observable<SingleSettingAnyChange>>())
    , m_single_common_setting_change(std::make_unique<Observable<SingleCommonSettingAnyChange>>())
    , m_extensions_change(std::make_unique<Observable<ExtensionsChange>>())
    , m_tab_back_forward_list_entries_change(std::make_unique<Observable<TabBackForwardListEntriesChange>>())
    , m_ui_windows_change(std::make_unique<Observable<UiWindowsChange>>())
    , m_recently_closed_tabs_change(std::make_unique<Observable<RecentlyClosedTabsChange>>())
    , m_password_items_change(std::make_unique<Observable<PasswordItemsChange>>())
    , m_downloads_change(std::make_unique<Observable<DownloadsChange>>())
    , m_search_engines_change(std::make_unique<Observable<SearchEnginesChange>>())
    , m_filter_lists_change(std::make_unique<Observable<FilterListsChange>>())
    , m_overflow_menu_items_change(std::make_unique<Observable<OverflowMenuItemsChange>>())
    , m_toolbar_items_change(std::make_unique<Observable<ToolbarItemsChange>>())
    , m_programmable_buttons_change(std::make_unique<Observable<ProgrammableButtonsChange>>())
    , m_notification_event(std::make_unique<Observable<NotificationEvent>>())
{
    global_core_api_count.fetch_add(1, std::memory_order_relaxed);
}

CoreApi::~CoreApi()
{
    global_core_api_count.fetch_sub(1, std::memory_order_relaxed);
    log_verbose("CoreApi::~CoreApi() [called on: %s]", current_thread_name().c_str());
}

std::shared_ptr<CoreApi>
CoreApi::create(const char* app_id)
{
    return std::shared_ptr<CoreApi>(new CoreApi(app_id)); // the new operator is used to call the private constructor
}

Error
CoreApi::init(const std::string& profiles_db_path)
{
    m_run_loop = RunLoop::init_main_loop();

    m_profiles_db = ProfilesDb::create(m_app_id.c_str(), m_password_storage);

    m_sync_service = SyncService::create(this);

    m_webextensions.internal_store = WebExtensionsInternalStore::create(this);
    m_webextensions.repository = WebExtensionsRepository::create(this);

    std::filesystem::path path = profiles_db_path.empty()
        ? get_data_directory_path(m_app_id.c_str(), { "profiles.db" })
        : profiles_db_path;
    VERIFY(ensure_parent_directories(path));
    VERIFY(m_profiles_db->open(path.string()));
    VERIFY(m_profiles_db->migrate());
    VERIFY(init_filter_lists());
    VERIFY(m_profiles_db->reset_downloads_state());

    m_history_cleanup = HistoryCleanupService::create(this, m_profiles_db->db());
    m_favicon_cleanup = FaviconCleanupService::create(m_profiles_db->db());
    m_download_cleanup = DownloadCleanupService::create(this);
    m_filter_list_update = FilterListUpdateService::create(this);

#if ENABLE_NET_TEST
    orion::net::tests::net_tests();
#endif
#if ENABLE_SYNC_TEST
    orion::sync::tests::sync_tests();
#endif
#if ENABLE_PROFILES_DB_TEST
    orion::test_profiles_db(m_profiles_db);
#endif
#if ENABLE_DOWNLOAD_TEST
    orion::test_download(this);
#endif
#if ENABLE_ZIP_TEST
    orion::test_zip(m_app_id.c_str());
#endif
#if ENABLE_ENUM_REFLECT_TEST
    orion::enum_reflect::enum_reflect_test();
#endif

    dispatch_test();

    SUCCEED();
}

void
CoreApi::set_password_storage(PasswordStorage* storage)
{
    m_password_storage = storage ? storage : &g_null_password_storage;
    if (m_profiles_db) {
        m_profiles_db->set_password_storage(m_password_storage);
    }
}

PasswordStorage&
CoreApi::password_storage() const
{
    return *m_password_storage;
}

void
CoreApi::set_notification_provider(NotificationProvider* provider)
{
    m_notification_provider = provider ? provider : &g_null_notification_provider;
}

NotificationProvider&
CoreApi::notification_provider() const
{
    return *m_notification_provider;
}

void
CoreApi::set_downloader_factory(DownloaderFactory* factory)
{
    m_downloader_factory = factory ? factory : &g_null_downloader_factory;
}

std::shared_ptr<Downloader>
CoreApi::create_downloader()
{
    return m_downloader_factory->create(m_app_id.c_str());
}

void
CoreApi::set_filter_list_auto_update_handler(Cb<std::vector<FilterListDto>> handler)
{
    m_filter_list_update->set_update_handler(std::move(handler));
}

void
CoreApi::set_iteration(IterationCb cb, void* user_data, std::chrono::milliseconds period)
{
    tls_run_loop()->set_iteration(cb, user_data, period);
}

IterationCb
CoreApi::get_iteration() const
{
    return tls_run_loop()->get_iteration();
}

void*
CoreApi::get_iteration_user_data() const
{
    return tls_run_loop()->get_iteration_user_data();
}

bool
CoreApi::run_iteration()
{
    return tls_run_loop()->run_iteration();
}

void
CoreApi::start_run_loop()
{
    prepare_run_loop();
    RunLoop::start_main(m_run_loop.get());
}

void
CoreApi::prepare_run_loop()
{
    m_history_cleanup->start();
    m_favicon_cleanup->start();
    m_download_cleanup->start();
    m_filter_list_update->start();
    // Caller drives the loop via tls_run_loop()->pump().
}

Error
CoreApi::request_stop_run_loop(std::function<void()> cb)
{
    m_filter_list_update->stop();
    m_history_cleanup->after_app_exit();
    VERIFY(tls_run_loop()->request_stop_main(cb));
    SUCCEED();
}

Error
CoreApi::init_filter_lists()
{
    std::vector<std::string> builtin_filter_list_names;
    for (const auto& metadata : filter_lists::get_metadata_list()) {
        if (metadata.filter_type == FilterListType::builtin) {
            builtin_filter_list_names.push_back(metadata.name);
        }
    }
    FilterListsChange change{};
    VERIFY(m_profiles_db->delete_unknown_builtin_filter_lists(builtin_filter_list_names, &change));
    if (!change.deleted.empty()) {
        orion::log_debug(
            "%s: deleted unknown builtin filter lists from ProfilesDb: count=%zu",
            __func__, change.deleted.size());
    }
    SUCCEED();
}

//-------------------------------------------------------------------------
// Profiles
//-------------------------------------------------------------------------

void
CoreApi::get_profile_ids(Cb<GetProfileIdsReply> cb)
{
    auto ids = std::make_shared<std::vector<ProfileId>>();

    dispatch(on_disk_thread(), [guard=shared_from_this(), this, ids] {
        VERIFY(m_profiles_db->get_profile_ids(ids.get()));
        SUCCEED();
    }).then_or_error([cb=std::move(cb), ids](Error err) {
        GetProfileIdsReply reply{
            .ids = *ids,
            .err = err,
        };
        cb(std::move(reply));
    });
}

void
CoreApi::get_default_profile_id(Cb<GetDefaultProfileIdReply> cb)
{
    auto profile_id = std::make_shared<ProfileId>(ProfileId(0));

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,profile_id] {
        VERIFY(m_profiles_db->get_default_profile_id(profile_id.get()));
        SUCCEED();
    }).then_or_error([cb=std::move(cb),profile_id](Error err) {
        GetDefaultProfileIdReply reply{
            .profile_id = *profile_id,
            .err = err,
        };
        cb(std::move(reply));
    });
}

void
CoreApi::get_internal_profile_id(Cb<GetInternalProfileIdReply> cb)
{
    auto profile_id = std::make_shared<ProfileId>(ProfileId(0));

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,profile_id] {
        VERIFY(m_profiles_db->get_internal_profile_id(profile_id.get()));
        SUCCEED();
    }).then_or_error([cb=std::move(cb),profile_id](Error err) {
        GetInternalProfileIdReply reply{
            .profile_id = *profile_id,
            .err = err,
        };
        cb(std::move(reply));
    });
}

void
CoreApi::get_profiles(const std::vector<ProfileId>& ids, Cb<GetProfilesReply> cb)
{
    auto profiles = std::make_shared<std::vector<ProfileDto>>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,ids,profiles] {
        VERIFY(m_profiles_db->get_profiles(ids, profiles.get()));
        SUCCEED();
    }).then_or_error([cb=std::move(cb),profiles](Error err) {
        GetProfilesReply reply{
            .profiles = *profiles,
            .err = err,
        };
        cb(std::move(reply));
    });
}

void
CoreApi::get_all_profiles(Cb<GetProfilesReply> cb)
{
    auto profiles = std::make_shared<std::vector<ProfileDto>>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,profiles] {
        std::vector<ProfileId> ids;
        VERIFY(m_profiles_db->get_profile_ids(&ids));
        VERIFY(m_profiles_db->get_profiles(ids, profiles.get()));
        SUCCEED();
    }).then_or_error([cb=std::move(cb),profiles](Error err) {
        cb(GetProfilesReply{
            .profiles = *profiles,
            .err = err,
        });
    });
}

void
CoreApi::create_profile(CreateProfileQuery query, Cb<CreateProfileReply> cb)
{
    auto profile_id = std::make_shared<ProfileId>(ProfileId(0));
    auto folders_change = std::make_shared<BookmarkFoldersChange>();
    auto profiles_change = std::make_shared<ProfilesChange>();
    auto overflow_menu_items_change = std::make_shared<OverflowMenuItemsChange>();
    auto toolbar_items_change = std::make_shared<ToolbarItemsChange>();
    auto prog_buttons_change = std::make_shared<ProgrammableButtonsChange>();

    dispatch(on_disk_thread(), [=,guard=shared_from_this(),this] {
        VERIFY(m_profiles_db->create_profile(
            query, profile_id.get(), folders_change.get(), profiles_change.get(), overflow_menu_items_change.get(),
            toolbar_items_change.get(), prog_buttons_change.get()
        ));
        SUCCEED();
    }).then_or_error([=,guard=shared_from_this(),this,cb=std::move(cb)](Error err) {
        CreateProfileReply reply{
            .id = *profile_id,
            .err = err,
        };
        cb(std::move(reply));
        if (!profiles_change->created.empty() || !profiles_change->updated.empty() || !profiles_change->deleted.empty()) {
            m_profiles_change->notify(std::move(*profiles_change));
        }
        if (!folders_change->created.empty() || !folders_change->updated.empty() || !folders_change->deleted.empty()) {
            m_bookmark_folders_change->notify(std::move(*folders_change));
        }
        if (!overflow_menu_items_change->created.empty()
         || !overflow_menu_items_change->updated.empty()
         || !overflow_menu_items_change->deleted.empty()) {
            m_overflow_menu_items_change->notify(std::move(*overflow_menu_items_change));
        }
        if (!toolbar_items_change->created.empty()
         || !toolbar_items_change->updated.empty()
         || !toolbar_items_change->deleted.empty()) {
            m_toolbar_items_change->notify(std::move(*toolbar_items_change));
        }
        if (!prog_buttons_change->created.empty()
         || !prog_buttons_change->updated.empty()
         || !prog_buttons_change->deleted.empty()) {
            m_programmable_buttons_change->notify(std::move(*prog_buttons_change));
        }
    });
}

void
CoreApi::update_profile(UpdateProfileQuery query, Cb<Error> cb)
{
    auto profiles_change = std::make_shared<ProfilesChange>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,query,profiles_change] {
        VERIFY(m_profiles_db->update_profile(query,profiles_change.get()));
        SUCCEED();
    }).then_or_error([guard=shared_from_this(),this,cb=std::move(cb),profiles_change](Error err) {
        cb(std::move(err));
        if (!profiles_change->created.empty() || !profiles_change->updated.empty() || !profiles_change->deleted.empty()) {
            m_profiles_change->notify(std::move(*profiles_change));
        }
    });
}

void
CoreApi::move_profile(MoveProfileQuery query, Cb<Error> cb)
{
    auto profiles_change = std::make_shared<ProfilesChange>();
    dispatch(on_disk_thread(), [guard=shared_from_this(),this,query,profiles_change] {
        VERIFY(m_profiles_db->move_profile(query, profiles_change.get()));
        SUCCEED();
    }).then_or_error([guard=shared_from_this(),this,cb=std::move(cb),profiles_change](Error err) {
        cb(std::move(err));
        if (!profiles_change->created.empty() || !profiles_change->updated.empty() || !profiles_change->deleted.empty()) {
            m_profiles_change->notify(std::move(*profiles_change));
        }
    });
}

void
CoreApi::set_default_profile(ProfileId profile_id, Cb<Error> cb)
{
    auto change = std::make_shared<ProfilesChange>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,profile_id,change] {
        VERIFY(m_profiles_db->set_default_profile(profile_id, change.get()));
        SUCCEED();
    }).then_or_error([guard=shared_from_this(),this,cb=std::move(cb),change](Error err) {
        cb(std::move(err));
        if (!change->created.empty() || !change->updated.empty() || !change->deleted.empty()) {
            m_profiles_change->notify(std::move(*change));
        }
    });
}

void
CoreApi::delete_profiles(DeleteProfilesQuery query, Cb<Error> cb)
{
    if (query.ids.empty()) {
        log_warning("CoreApi::delete_profiles: called with empty ids vector");
    }

    auto profiles_change = std::make_shared<ProfilesChange>();
    auto windows_change = std::make_shared<WindowsChange>();
    auto tabs_change = std::make_shared<TabsChange>();
    
    dispatch(on_disk_thread(), [guard=shared_from_this(),this,query,profiles_change,windows_change,tabs_change] {
        if (query.delete_file_system_data_fn) {
           for (const auto& id : query.ids) {
                VERIFY(query.delete_file_system_data_fn(id));
            }
        }
        VERIFY(m_profiles_db->delete_profiles(query.ids, profiles_change.get(), windows_change.get(), tabs_change.get()));
        VERIFY(m_profiles_db->verify_windows_change(windows_change.get()));
        SUCCEED();
    }).then_or_error([guard=shared_from_this(),this,cb=std::move(cb),profiles_change,windows_change,tabs_change](Error err) {
        cb(std::move(err));
        if (!tabs_change->created.empty() || !tabs_change->updated.empty() || !tabs_change->deleted.empty()) {
            m_tabs_change->notify(std::move(*tabs_change));
        }
        if (!windows_change->created.empty() || !windows_change->updated.empty() || !windows_change->deleted.empty()) {
            m_windows_change->notify(std::move(*windows_change));
        }
        if (!profiles_change->created.empty() || !profiles_change->updated.empty() || !profiles_change->deleted.empty()) {
            m_profiles_change->notify(std::move(*profiles_change));
        }
    });
}

void
CoreApi::delete_test_profiles(Cb<Error> cb)
{
    auto profiles_change = std::make_shared<ProfilesChange>();
    auto windows_change = std::make_shared<WindowsChange>();
    auto tabs_change = std::make_shared<TabsChange>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,profiles_change,windows_change,tabs_change] {
        std::vector<ProfileId> ids;
        VERIFY(m_profiles_db->get_profile_ids(&ids));

        std::vector<ProfileDto> profiles;
        VERIFY(m_profiles_db->get_profiles(ids, &profiles));

        std::vector<orion::ProfileId> to_delete;
        for (const auto& profile : profiles) {
            if (profile.is_test) {
                log_debug("deleting test profile: profile_id=%lld name=%s", profile.id.value, profile.name.c_str());
                to_delete.push_back(profile.id);
            }
        }

        if (!to_delete.empty()) {
            VERIFY(m_profiles_db->delete_profiles(to_delete, profiles_change.get(), windows_change.get(), tabs_change.get()));
        }
        VERIFY(m_profiles_db->verify_windows_change(windows_change.get()));
        SUCCEED();
    }).then_or_error([guard=shared_from_this(),this,cb=std::move(cb),profiles_change,windows_change,tabs_change](Error err) {
        cb(std::move(err));
        if (!tabs_change->created.empty() || !tabs_change->updated.empty() || !tabs_change->deleted.empty()) {
            m_tabs_change->notify(std::move(*tabs_change));
        }
        if (!windows_change->created.empty() || !windows_change->updated.empty() || !windows_change->deleted.empty()) {
            m_windows_change->notify(std::move(*windows_change));
        }
        if (!profiles_change->created.empty() || !profiles_change->updated.empty() || !profiles_change->deleted.empty()) {
            m_profiles_change->notify(std::move(*profiles_change));
        }
    });
}

void
CoreApi::set_selected_window_id(SetSelectedWindowIdQuery query, Cb<Error> cb)
{
    auto change = std::make_shared<ProfilesChange>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,query,change] {
        VERIFY(m_profiles_db->set_selected_window_id(query, change.get()));
        SUCCEED();
    }).then_or_error([guard=shared_from_this(),this,cb=std::move(cb),change](Error err) {
        cb(std::move(err));
        if (!change->created.empty() || !change->updated.empty() || !change->deleted.empty()) {
            m_profiles_change->notify(std::move(*change));
        }
    });
}

void
CoreApi::get_selected_window_id(ProfileId profile_id, Cb<GetSelectedWindowIdReply> cb)
{
    auto window_id = std::make_shared<WindowId>(WindowId(0));
    auto exists = std::make_shared<bool>(false);
    auto is_private = std::make_shared<bool>(false);

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,profile_id,window_id,exists,is_private] {
        VERIFY(m_profiles_db->get_selected_window_id(profile_id, window_id.get(), exists.get(), is_private.get()));
        SUCCEED();
    }).then_or_error([cb=std::move(cb),window_id,exists,is_private](Error err) {
        GetSelectedWindowIdReply reply{
            .err = err,
            .window_id = *window_id,
            .window_exists = *exists,
            .is_private = *is_private,
        };
        cb(std::move(reply));
    });
}

void
CoreApi::get_shared_pinned_tabs_window_id(ProfileId profile_id, Cb<GetSharedPinnedTabsWindowIdReply> cb)
{
    auto window_id = std::make_shared<WindowId>(WindowId(0));

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,profile_id,window_id] {
        VERIFY(m_profiles_db->profile_id_get_shared_pinned_tabs_window_id(profile_id, window_id.get()));
        SUCCEED();
    }).then_or_error([cb=std::move(cb),window_id](Error err) {
        GetSharedPinnedTabsWindowIdReply reply{
            .err = err,
            .window_id = *window_id,
        };
        cb(std::move(reply));
    });
}

//-------------------------------------------------------------------------
// Bookmark folders
//-------------------------------------------------------------------------

void
CoreApi::get_bookmark_folder_ids(ProfileId profile_id, Cb<GetBookmarkFolderIdsReply> cb)
{
    auto ids = std::make_shared<std::vector<BookmarkFolderId>>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,ids,profile_id]{
        VERIFY(m_profiles_db->get_bookmark_folder_ids(profile_id, ids.get()));
        SUCCEED();
    }).then_or_error([cb=std::move(cb),ids](Error err) {
        GetBookmarkFolderIdsReply reply{
            .ids = *ids,
            .err = err,
        };
        cb(std::move(reply));
    });
}

void
CoreApi::get_bookmark_folders(const std::vector<BookmarkFolderId>& ids, Cb<GetBookmarkFoldersReply> cb)
{
    auto folders = std::make_shared<std::vector<BookmarkFolderDto>>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,ids,folders]{
        VERIFY(m_profiles_db->get_bookmark_folders(ids, folders.get()));
        SUCCEED();
    }).then_or_error([cb=std::move(cb),folders](Error err) {
        GetBookmarkFoldersReply reply{
            .folders = *folders,
            .err = err,
        };
        cb(std::move(reply));
    });
}

void
CoreApi::get_bookmark_folders_by_query(ProfileId profile_id, Cb<GetBookmarkFoldersReply> cb)
{
    auto folders = std::make_shared<std::vector<BookmarkFolderDto>>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,profile_id,folders]{
        std::vector<BookmarkFolderId> ids;
        VERIFY(m_profiles_db->get_bookmark_folder_ids(profile_id, &ids));
        VERIFY(m_profiles_db->get_bookmark_folders(ids, folders.get()));
        SUCCEED();
    }).then_or_error([cb=std::move(cb),folders](Error err) {
        cb(GetBookmarkFoldersReply{
            .folders = *folders,
            .err = err,
        });
    });
}

void
CoreApi::get_bookmark_folder_id_by_type(
    ProfileId profile_id, BookmarkFolderType type, Cb<GetBookmarkFolderIdByTypeReply> cb)
{
    auto folder_id = std::make_shared<BookmarkFolderId>(BookmarkFolderId(0));

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,folder_id,profile_id,type] {
        VERIFY(m_profiles_db->get_bookmark_folder_id_by_type(profile_id, type, folder_id.get()));
        SUCCEED();
    }).then_or_error([cb=std::move(cb),folder_id](Error err) {
        GetBookmarkFolderIdByTypeReply reply{
            .folder_id = *folder_id,
            .err = err,
        };
        cb(std::move(reply));
    });
}

void
CoreApi::create_bookmark_folder(CreateBookmarkFolderQuery query, Cb<CreateBookmarkFolderReply> cb)
{
    auto folder_id = std::make_shared<BookmarkFolderId>();
    auto change = std::make_shared<BookmarkFoldersChange>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,query,folder_id,change] {
        VERIFY(m_profiles_db->create_bookmark_folder(query, folder_id.get(), change.get()));
        SUCCEED();
    }).then_or_error([guard=shared_from_this(),this,cb=std::move(cb),folder_id,change](Error err) {
        CreateBookmarkFolderReply reply{
            .folder_id = *folder_id,
            .err = err,
        };
        cb(std::move(reply));
        if (!change->created.empty() || !change->updated.empty() || !change->deleted.empty()) {
            m_bookmark_folders_change->notify(std::move(*change));
        }
    });
}

void
CoreApi::update_bookmark_folder(UpdateBookmarkFolderQuery query, Cb<Error> cb)
{
    auto change = std::make_shared<BookmarkFoldersChange>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,query,change] {
        VERIFY(m_profiles_db->update_bookmark_folder(query, change.get()));
        SUCCEED();
    }).then_or_error([guard=shared_from_this(),this,cb=std::move(cb),change](Error err) {
        cb(std::move(err));
        if (!change->created.empty() || !change->updated.empty() || !change->deleted.empty()) {
            m_bookmark_folders_change->notify(std::move(*change));
        }
    });
}

void
CoreApi::delete_bookmark_folders(const std::vector<BookmarkFolderId>& ids, Cb<Error> cb)
{
    if (ids.empty()) {
        log_warning("CoreApi::delete_bookmark_folders: called with empty ids vector");
    }

    auto change = std::make_shared<BookmarkFoldersChange>();
    auto bookmarks_change = std::make_shared<BookmarksChange>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,ids,change,bookmarks_change] {
        VERIFY(m_profiles_db->delete_bookmark_folders(ids, change.get(), bookmarks_change.get()));
        SUCCEED();
    }).then_or_error([guard=shared_from_this(),this,cb=std::move(cb),change,bookmarks_change](Error err) {
        cb(std::move(err));
        // First notify bookmarks change, then bookmark folders change (the order is important)
        if (!bookmarks_change->created.empty() || !bookmarks_change->updated.empty() || !bookmarks_change->deleted.empty()) {
            m_bookmarks_change->notify(std::move(*bookmarks_change));
        }
        if (!change->created.empty() || !change->updated.empty() || !change->deleted.empty()) {
            m_bookmark_folders_change->notify(std::move(*change));
        }
    });
}

void
CoreApi::copy_bookmarks(CopyBookmarksQuery query, Cb<CopyBookmarksReply> cb)
{
    auto reply = std::make_shared<CopyBookmarksReply>();
    auto folder_change = std::make_shared<BookmarkFoldersChange>();
    auto bookmark_change = std::make_shared<BookmarksChange>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,query,reply,folder_change,bookmark_change] {
        VERIFY(m_profiles_db->copy_bookmarks(query, reply.get(), folder_change.get(), bookmark_change.get()));
        SUCCEED();
    }).then_or_error([guard=shared_from_this(),this,cb=std::move(cb),reply,folder_change,bookmark_change](Error err) {
        reply->err = err;
        cb(std::move(*reply));

        // first: notify folders change (the order is important)
        if (!folder_change->created.empty() || !folder_change->updated.empty() || !folder_change->deleted.empty()) {
            m_bookmark_folders_change->notify(std::move(*folder_change));
        }

        // second: notify bookmarks change
        if (!bookmark_change->created.empty() || !bookmark_change->updated.empty() || !bookmark_change->deleted.empty()) {
            m_bookmarks_change->notify(std::move(*bookmark_change));
        }
    });
}

Error
CoreApi::get_bookmark_item_recursive(
    ProfileId profile_id, const BookmarkFolderId* folder_id, BookmarkItemDto* item_out) const
{
    RAISE_CHECK(item_out);

    if (folder_id == nullptr) {
        // Special (virtual) root folder
        item_out->folder = BookmarkFolderDto{
            .id = BookmarkFolderId(0),
            .profile_id = profile_id,
            .name = "",
            .is_internal = false,
            .parent_folder_id = BookmarkFolderId(0),
            .type = BookmarkFolderType::normal,
        };
        item_out->type = BookmarkItemType::folder;
    } else {
        // Normal or internal folder
        std::vector<BookmarkFolderDto> folders{};
        VERIFY(m_profiles_db->get_bookmark_folders({*folder_id}, &folders));
        RAISE_CHECK(!folders.empty());
        item_out->folder = folders[0];
        item_out->type = BookmarkItemType::folder;
    }

    std::vector<BookmarkItem> children{};
    VERIFY(m_profiles_db->get_bookmark_folder_children(profile_id, folder_id, &children));

    for (const auto& child : children) {
        BookmarkItemDto child_dto{};
        if (child.type == BookmarkItemType::folder) {
            auto id = static_cast<BookmarkFolderId>(child.id);
            VERIFY(get_bookmark_item_recursive(profile_id, &id, &child_dto));
        } else if (child.type == BookmarkItemType::bookmark) {
            std::vector<BookmarkDto> bookmarks{};
            GetBookmarksQuery query{
                .ids = {static_cast<BookmarkId>(child.id)},
                .order_by = BookmarkOrderByMode::parent_folder_id_asc_seq_num_asc,
            };
            VERIFY(m_profiles_db->get_bookmarks(query, &bookmarks));
            RAISE_CHECK(!bookmarks.empty());
            child_dto.bookmark = bookmarks[0];
            child_dto.type = BookmarkItemType::bookmark;
        } else {
            RAISE(ErrorCode::invalid_type, "invalid bookmark item type: %d", static_cast<int>(child.type));
        }
        item_out->children.emplace_back(child_dto);
    }
    SUCCEED();
}

void
CoreApi::export_bookmarks_to_html(
    ProfileId profile_id,
    std::function<void(const std::string& html, Error err)> reply)
{
    auto html = std::make_shared<std::string>();

    dispatch(on_disk_thread(), [=,guard=shared_from_this(),this] {
        BookmarkItemDto root_item{};
        VERIFY(get_bookmark_item_recursive(profile_id, nullptr, &root_item));
        
        std::shared_ptr<BookmarkHtmlExporter> exporter = BookmarkHtmlExporter::create();

        VERIFY(exporter->export_bookmarks_to_html(root_item, html.get()));
        SUCCEED();
    }).then_or_error([reply=std::move(reply),html](Error err) {
        reply(*html, err);
    });
}

Error
CoreApi::create_bookmark_child_items_recursive(
    ProfileId profile_id,
    BookmarkFolderId parent_folder_id,
    BookmarkItemDto root_item,
    BookmarkFoldersChange* folders_change,
    BookmarksChange* bookmarks_change)
{
    for (const auto& child : root_item.children) {
        if (child.type == BookmarkItemType::folder) {
            CreateBookmarkFolderQuery query{
                .profile_id = profile_id,
                .name = child.folder.name,
                .parent_folder_id = parent_folder_id,
            };
            BookmarkFolderId new_folder_id{};
            VERIFY(m_profiles_db->create_bookmark_folder(query, &new_folder_id, folders_change));
            VERIFY(create_bookmark_child_items_recursive(
                profile_id, new_folder_id, child, folders_change, bookmarks_change
            ));
        } else if (child.type == BookmarkItemType::bookmark) {
            CreateBookmarkQuery query{
                .profile_id = profile_id,
                .name = child.bookmark.name,
                .address = child.bookmark.address,
                .parent_folder_id = parent_folder_id,
            };
            BookmarkId bookmark_id{};
            VERIFY(m_profiles_db->create_bookmark(query, &bookmark_id, bookmarks_change));
        } else {
            RAISE(ErrorCode::invalid_type, "invalid bookmark item type: %d", static_cast<int>(child.type));
        }
    }
    SUCCEED();
}

void
CoreApi::import_bookmarks_from_html(
    ProfileId profile_id,
    const std::string& html,
    const std::string& folder_name_prefix,
    std::function<void(int num_imported, Error err)> reply)
{
    auto num_imported = std::make_shared<int>(0);

    dispatch(on_disk_thread(), [=,guard=shared_from_this(),this] {
        std::shared_ptr<BookmarkHtmlImporter> importer = BookmarkHtmlImporter::create();
        BookmarkItemDto root_item{};
        VERIFY(importer->import_bookmarks_from_html(html, &root_item));

        DbTx tx{};
        VERIFY(m_profiles_db->begin(&tx));

        std::string new_folder_name{};
        VERIFY(m_profiles_db->find_unique_bookmark_folder_name(profile_id, folder_name_prefix, nullptr, &new_folder_name));

        // Accumulate (and merge) changes and send notifications after the transaction is committed
        BookmarkFoldersChange folders_change{};
        BookmarksChange bookmarks_change{};

        CreateBookmarkFolderQuery query{
            .profile_id = profile_id,
            .name = new_folder_name,
            .parent_folder_id = root_item.folder.parent_folder_id,
        };

        BookmarkFolderId new_folder_id{};
        VERIFY(m_profiles_db->create_bookmark_folder(query, &new_folder_id, &folders_change));

        VERIFY(create_bookmark_child_items_recursive(
            profile_id, new_folder_id, root_item, &folders_change, &bookmarks_change
        ));

        VERIFY(tx.commit());

        *num_imported = static_cast<int>(bookmarks_change.created.size());

        // Dispatch change notifications
        if (!folders_change.created.empty() || !folders_change.updated.empty() || !folders_change.deleted.empty()) {
            m_bookmark_folders_change->notify(std::move(folders_change));
        }
        if (!bookmarks_change.created.empty() || !bookmarks_change.updated.empty() || !bookmarks_change.deleted.empty()) {
            m_bookmarks_change->notify(std::move(bookmarks_change));
        }
        SUCCEED();
    }).then_or_error([reply=std::move(reply),num_imported](Error err) {
        reply(*num_imported, err);
    });
}

//-------------------------------------------------------------------------
// Bookmarks
//-------------------------------------------------------------------------

void
CoreApi::get_bookmark_ids(GetBookmarkIdsQuery query, Cb<GetBookmarkIdsReply> cb)
{
    auto ids = std::make_shared<std::vector<BookmarkId>>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,ids,query]{
        VERIFY(m_profiles_db->get_bookmark_ids(query, ids.get()));
        SUCCEED();
    }).then_or_error([cb=std::move(cb),ids](Error err) {
        GetBookmarkIdsReply reply{
            .ids = *ids,
            .err = err,
        };
        cb(std::move(reply));
    });
}

void
CoreApi::get_recent_bookmark_ids(GetRecentBookmarkIdsQuery query, Cb<GetRecentBookmarkIdsReply> cb)
{
    auto ids = std::make_shared<std::vector<BookmarkId>>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,ids,query]{
        VERIFY(m_profiles_db->get_recent_bookmark_ids(query, ids.get()));
        SUCCEED();
    }).then_or_error([cb=std::move(cb),ids](Error err) {
        GetRecentBookmarkIdsReply reply{
            .ids = *ids,
            .err = err,
        };
        cb(std::move(reply));
    });
}

void
CoreApi::get_bookmarks(GetBookmarksQuery query, Cb<GetBookmarksReply> cb)
{
    auto bookmarks = std::make_shared<std::vector<BookmarkDto>>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,query,bookmarks]{
        VERIFY(m_profiles_db->get_bookmarks(query, bookmarks.get()));
        SUCCEED();
    }).then_or_error([cb=std::move(cb),bookmarks](Error err) {
        GetBookmarksReply reply{
            .bookmarks = *bookmarks,
            .err = err,
        };
        cb(std::move(reply));
    });
}

void
CoreApi::get_bookmarks_by_query(GetBookmarksByQuery query, Cb<GetBookmarksReply> cb)
{
    auto bookmarks = std::make_shared<std::vector<BookmarkDto>>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,query,bookmarks]{
        std::vector<BookmarkId> ids;
        VERIFY(m_profiles_db->get_bookmark_ids(GetBookmarkIdsQuery{
            .profile_id = query.profile_id,
            .search_filter = query.search_filter,
            .parent_folder_id = query.parent_folder_id,
        }, &ids));
        VERIFY(m_profiles_db->get_bookmarks(GetBookmarksQuery{
            .ids = ids,
            .order_by = query.order_by,
        }, bookmarks.get()));
        SUCCEED();
    }).then_or_error([cb=std::move(cb),bookmarks](Error err) {
        cb(GetBookmarksReply{
            .bookmarks = *bookmarks,
            .err = err,
        });
    });
}

void
CoreApi::create_bookmark(CreateBookmarkQuery query, Cb<CreateBookmarkReply> cb)
{
    auto bookmark_id = std::make_shared<BookmarkId>();
    auto change = std::make_shared<BookmarksChange>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,query,bookmark_id,change] {
        VERIFY(m_profiles_db->create_bookmark(query, bookmark_id.get(), change.get()));
        SUCCEED();
    }).then_or_error([guard=shared_from_this(),this,cb=std::move(cb),bookmark_id,change](Error err) {
        CreateBookmarkReply reply{
            .bookmark_id = *bookmark_id,
            .err = err,
        };
        cb(std::move(reply));
        if (!change->created.empty() || !change->updated.empty() || !change->deleted.empty()) {
            m_bookmarks_change->notify(std::move(*change));
        }
    });
}

void
CoreApi::update_bookmark(UpdateBookmarkQuery query, Cb<Error> cb)
{
    auto change = std::make_shared<BookmarksChange>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,query,change] {
        VERIFY(m_profiles_db->update_bookmark(query, change.get()));
        SUCCEED();
    }).then_or_error([guard=shared_from_this(),this,cb=std::move(cb),change](Error err) {
        cb(std::move(err));
        if (!change->created.empty() || !change->updated.empty() || !change->deleted.empty()) {
            m_bookmarks_change->notify(std::move(*change));
        }
    });
}

void
CoreApi::delete_bookmarks(const std::vector<BookmarkId>& ids, Cb<Error> cb)
{
    if (ids.empty()) {
        log_warning("CoreApi::delete_bookmarks: called with empty ids vector");
    }

    auto change = std::make_shared<BookmarksChange>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,ids,change] {
        VERIFY(m_profiles_db->delete_bookmarks(ids, change.get()));
        SUCCEED();
    }).then_or_error([guard=shared_from_this(),this,cb=std::move(cb),change](Error err) {
        cb(std::move(err));
        if (!change->created.empty() || !change->updated.empty() || !change->deleted.empty()) {
            m_bookmarks_change->notify(std::move(*change));
        }
    });
}

void
CoreApi::move_bookmark(MoveBookmarkQuery query, Cb<Error> cb)
{
    auto change = std::make_shared<BookmarksChange>();
    auto folders_change = std::make_shared<BookmarkFoldersChange>();
    dispatch(on_disk_thread(), [guard=shared_from_this(),this,query,change,folders_change] {
        VERIFY(m_profiles_db->move_bookmark(query, change.get()));

        std::vector<BookmarkDto> bookmarks;
        VERIFY(m_profiles_db->get_bookmarks(GetBookmarksQuery{.ids={query.bookmark_id}}, &bookmarks));
        if (!bookmarks.empty()) {
            VERIFY(m_profiles_db->normalize_bookmark_seq_num(
                bookmarks[0].profile_id, query.target_parent_folder_id,
                change.get(), folders_change.get()));
        }
        SUCCEED();
    }).then_or_error([guard=shared_from_this(),this,cb=std::move(cb),change,folders_change](Error err) {
        cb(std::move(err));
        if (!change->created.empty() || !change->updated.empty() || !change->deleted.empty()) {
            m_bookmarks_change->notify(std::move(*change));
        }
        if (!folders_change->created.empty() || !folders_change->updated.empty() || !folders_change->deleted.empty()) {
            m_bookmark_folders_change->notify(std::move(*folders_change));
        }
    });
}

void
CoreApi::move_bookmark_folder(MoveBookmarkFolderQuery query, Cb<Error> cb)
{
    auto bookmarks_change = std::make_shared<BookmarksChange>();
    auto change = std::make_shared<BookmarkFoldersChange>();
    dispatch(on_disk_thread(), [guard=shared_from_this(),this,query,bookmarks_change,change] {
        VERIFY(m_profiles_db->move_bookmark_folder(query, change.get()));

        std::vector<BookmarkFolderDto> folders;
        VERIFY(m_profiles_db->get_bookmark_folders({query.folder_id}, &folders));
        if (!folders.empty()) {
            VERIFY(m_profiles_db->normalize_bookmark_seq_num(
                folders[0].profile_id, query.target_parent_folder_id,
                bookmarks_change.get(), change.get()));
        }
        SUCCEED();
    }).then_or_error([guard=shared_from_this(),this,cb=std::move(cb),bookmarks_change,change](Error err) {
        cb(std::move(err));
        if (!bookmarks_change->created.empty() || !bookmarks_change->updated.empty() || !bookmarks_change->deleted.empty()) {
            m_bookmarks_change->notify(std::move(*bookmarks_change));
        }
        if (!change->created.empty() || !change->updated.empty() || !change->deleted.empty()) {
            m_bookmark_folders_change->notify(std::move(*change));
        }
    });
}

void
CoreApi::is_current_tab_bookmarked(IsCurrentTabBookmarkedQuery query, Cb<IsCurrentTabBookmarkedReply> cb)
{
    auto reply = std::make_shared<IsCurrentTabBookmarkedReply>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,query,reply] {
        VERIFY(m_profiles_db->is_current_tab_bookmarked(query, reply.get()));
        SUCCEED();
    }).then_or_error([cb=std::move(cb),reply](Error err) {
        reply->err = err;
        cb(std::move(*reply));
    });
}

//-------------------------------------------------------------------------
// History entries
//-------------------------------------------------------------------------

void
CoreApi::get_history_entry_ids(GetHistoryEntryIdsQuery query, Cb<GetHistoryEntryIdsReply> cb)
{
    auto ids = std::make_shared<std::vector<HistoryEntryId>>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,query,ids] {
        VERIFY(m_profiles_db->get_history_entry_ids(query, ids.get()));
        SUCCEED();
    }).then_or_error([cb=std::move(cb),ids](Error err) {
        GetHistoryEntryIdsReply reply{
            .ids = *ids,
            .err = err,
        };
        cb(std::move(reply));
    });
}

void
CoreApi::get_nonempty_history_days(GetNonemptyHistoryDaysQuery query, Cb<GetNonemptyHistoryDaysReply> cb)
{
    auto days = std::make_shared<std::vector<HistoryEntryDay>>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,days,query] {
        VERIFY(m_profiles_db->get_nonempty_history_days(query, days.get()));
        SUCCEED();
    }).then_or_error([cb=std::move(cb),days](Error err) {
        GetNonemptyHistoryDaysReply reply{
            .days = std::move(*days),
            .err = err,
        };
        cb(std::move(reply));
    });
}

void
CoreApi::get_history_entries(ProfileHistoryQuery query, Cb<GetHistoryEntriesReply> cb)
{
    auto entries = std::make_shared<std::vector<HistoryEntryDto>>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,query,entries] {
        VERIFY(m_profiles_db->get_history_entries(query, entries.get()));
        SUCCEED();
    }).then_or_error([cb=std::move(cb),entries](Error err) {
        GetHistoryEntriesReply reply{
            .entries = *entries,
            .err = err,
        };
        cb(std::move(reply));
    });
}

void
CoreApi::get_history_entries(const std::vector<HistoryEntryId>& ids, Cb<GetHistoryEntriesReply> cb)
{
    auto entries = std::make_shared<std::vector<HistoryEntryDto>>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,ids,entries] {
        VERIFY(m_profiles_db->get_history_entries(ids, entries.get()));
        SUCCEED();
    }).then_or_error([cb=std::move(cb),entries](Error err) {
        GetHistoryEntriesReply reply{
            .entries = *entries,
            .err = err,
        };
        cb(std::move(reply));
    });
}

void
CoreApi::get_history_entries_by_query(GetHistoryEntryIdsQuery query, Cb<GetHistoryEntriesReply> cb)
{
    auto entries = std::make_shared<std::vector<HistoryEntryDto>>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,query,entries] {
        std::vector<HistoryEntryId> ids;
        VERIFY(m_profiles_db->get_history_entry_ids(query, &ids));
        VERIFY(m_profiles_db->get_history_entries(ids, entries.get()));
        SUCCEED();
    }).then_or_error([cb=std::move(cb),entries](Error err) {
        cb(GetHistoryEntriesReply{
            .entries = *entries,
            .err = err,
        });
    });
}

void
CoreApi::create_history_entry(CreateHistoryEntryQuery query, Cb<CreateHistoryEntryReply> cb)
{
    auto entry_id = std::make_shared<HistoryEntryId>(HistoryEntryId(0));
    auto entries_change = std::make_shared<HistoryEntriesChange>();
    auto days_change = std::make_shared<HistoryEntryDaysChange>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,query,entry_id,entries_change,days_change] {
        VERIFY(m_profiles_db->create_history_entry(query, entry_id.get(), entries_change.get(), days_change.get()));
        SUCCEED();
    }).then_or_error([guard=shared_from_this(),this,cb=std::move(cb),entry_id,entries_change,days_change](Error err) {
        CreateHistoryEntryReply reply{
            .err = err,
            .id = *entry_id,
        };
        cb(std::move(reply));
        if (!entries_change->created.empty() || !entries_change->updated.empty() || !entries_change->deleted.empty()) {
            m_history_entries_change->notify(std::move(*entries_change));
        }
        if (!days_change->created.empty() || !days_change->updated.empty() || !days_change->deleted.empty()) {
            m_history_entry_days_change->notify(std::move(*days_change));
        }
    });
}

void
CoreApi::delete_history_entries(const std::vector<HistoryEntryId>& ids, Cb<Error> cb)
{
    if (ids.empty()) {
        log_warning("CoreApi::delete_history_entries: called with empty ids vector");
    }

    auto entries_change = std::make_shared<HistoryEntriesChange>();
    auto days_change = std::make_shared<HistoryEntryDaysChange>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,ids,entries_change,days_change] {
        VERIFY(m_profiles_db->delete_history_entries(ids, entries_change.get(), days_change.get()));
        SUCCEED();
    }).then_or_error([guard=shared_from_this(),this,cb=std::move(cb),entries_change,days_change](Error err) {
        cb(std::move(err));
        if (!entries_change->created.empty() || !entries_change->updated.empty() || !entries_change->deleted.empty()) {
            m_history_entries_change->notify(std::move(*entries_change));
        }
        if (!days_change->created.empty() || !days_change->updated.empty() || !days_change->deleted.empty()) {
            m_history_entry_days_change->notify(std::move(*days_change));
        }
    });
}

void
CoreApi::clear_history_entries(ProfileHistoryQuery query, Cb<Error> cb)
{
    auto days_change = std::make_shared<HistoryEntryDaysChange>();
    
    dispatch(on_disk_thread(), [guard=shared_from_this(),this,query = std::move(query),days_change] {
        VERIFY(m_profiles_db->clear_history_entries(query, days_change.get()));
        SUCCEED();
    }).then_or_error([guard=shared_from_this(),this,cb=std::move(cb),days_change](Error err) {
        cb(std::move(err));
        if (!days_change->deleted.empty()) {
            m_history_entry_days_change->notify(std::move(*days_change));
        }
    });
}

//-------------------------------------------------------------------------
// Reading list entries
//-------------------------------------------------------------------------

void
CoreApi::get_reading_list_entry_ids(GetReadingListEntryIdsQuery query, Cb<GetReadingListEntryIdsReply> cb)
{
    auto ids = std::make_shared<std::vector<ReadingListEntryId>>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,ids,query] {
        VERIFY(m_profiles_db->get_reading_list_entry_ids(query, ids.get()));
        SUCCEED();
    }).then_or_error([cb=std::move(cb),ids](Error err) {
        GetReadingListEntryIdsReply reply{
            .ids = *ids,
            .err = err,
        };
        cb(std::move(reply));
    });
}

void
CoreApi::get_reading_list_entries(const std::vector<ReadingListEntryId>& ids, Cb<GetReadingListEntriesReply> cb)
{
    auto entries = std::make_shared<std::vector<ReadingListEntryDto>>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,ids,entries] {
        VERIFY(m_profiles_db->get_reading_list_entries(ids, entries.get()));
        SUCCEED();
    }).then_or_error([cb=std::move(cb),entries](Error err) {
        GetReadingListEntriesReply reply{
            .entries = *entries,
            .err = err,
        };
        cb(std::move(reply));
    });
}

void
CoreApi::get_reading_list_entries_by_query(GetReadingListEntryIdsQuery query, Cb<GetReadingListEntriesReply> cb)
{
    auto entries = std::make_shared<std::vector<ReadingListEntryDto>>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,query,entries] {
        std::vector<ReadingListEntryId> ids;
        VERIFY(m_profiles_db->get_reading_list_entry_ids(query, &ids));
        VERIFY(m_profiles_db->get_reading_list_entries(ids, entries.get()));
        SUCCEED();
    }).then_or_error([cb=std::move(cb),entries](Error err) {
        cb(GetReadingListEntriesReply{
            .entries = *entries,
            .err = err,
        });
    });
}

void
CoreApi::create_reading_list_entry(CreateReadingListEntryQuery query, Cb<CreateReadingListEntryReply> cb)
{
    auto entry_id = std::make_shared<ReadingListEntryId>(ReadingListEntryId(0));
    auto change = std::make_shared<ReadingListEntriesChange>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,query,entry_id,change] {
        VERIFY(m_profiles_db->create_reading_list_entry(query, entry_id.get(), change.get()));
        SUCCEED();
    }).then_or_error([guard=shared_from_this(),this,cb=std::move(cb),entry_id,change](Error err) {
        CreateReadingListEntryReply reply{
            .err = err,
            .id = *entry_id,
        };
        cb(std::move(reply));
        if (!change->created.empty() || !change->updated.empty() || !change->deleted.empty()) {
            m_reading_list_entries_change->notify(std::move(*change));
        }
    });
}

void
CoreApi::update_reading_list_entry(UpdateReadingListEntryQuery query, Cb<Error> cb)
{
    auto change = std::make_shared<ReadingListEntriesChange>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,query,change] {
        VERIFY(m_profiles_db->update_reading_list_entry(query, change.get()));
        SUCCEED();
    }).then_or_error([guard=shared_from_this(),this,cb=std::move(cb),change](Error err) {
        cb(std::move(err));
        if (!change->created.empty() || !change->updated.empty() || !change->deleted.empty()) {
            m_reading_list_entries_change->notify(std::move(*change));
        }
    });
}

void
CoreApi::delete_reading_list_entries(const std::vector<ReadingListEntryId>& ids, Cb<Error> cb)
{
    if (ids.empty()) {
        log_warning("CoreApi::delete_reading_list_entries: called with empty ids vector");
    }

    auto change = std::make_shared<ReadingListEntriesChange>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,ids,change] {
        VERIFY(m_profiles_db->delete_reading_list_entries(ids, change.get()));
        SUCCEED();
    }).then_or_error([guard=shared_from_this(),this,cb=std::move(cb),change](Error err) {
        cb(std::move(err));
        if (!change->created.empty() || !change->updated.empty() || !change->deleted.empty()) {
            m_reading_list_entries_change->notify(std::move(*change));
        }
    });
}

//-------------------------------------------------------------------------
// Windows
//-------------------------------------------------------------------------

void
CoreApi::get_window_ids(GetWindowIdsQuery query, Cb<GetWindowIdsReply> cb)
{
    auto ids = std::make_shared<std::vector<WindowId>>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,ids,query] {
        VERIFY(m_profiles_db->get_window_ids(query, ids.get()));
        SUCCEED();
    }).then_or_error([cb=std::move(cb),ids](Error err) {
        GetWindowIdsReply reply{
            .ids = *ids,
            .err = err,
        };
        cb(std::move(reply));
    });
}

void
CoreApi::get_windows(const std::vector<WindowId>& ids, Cb<GetWindowsReply> cb)
{
    auto windows = std::make_shared<std::vector<WindowDto>>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,ids,windows] {
        VERIFY(m_profiles_db->get_windows(ids, windows.get()));
        SUCCEED();
    }).then_or_error([cb=std::move(cb),windows](Error err) {
        GetWindowsReply reply{
            .windows = *windows,
            .err = err,
        };
        cb(std::move(reply));
    });
}

void
CoreApi::get_windows_by_query(ProfileId profile_id, Cb<GetWindowsReply> cb)
{
    auto windows = std::make_shared<std::vector<WindowDto>>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,profile_id,windows] {
        std::vector<WindowId> ids;
        VERIFY(m_profiles_db->get_window_ids({.profile_id=profile_id}, &ids));
        VERIFY(m_profiles_db->get_windows(ids, windows.get()));
        SUCCEED();
    }).then_or_error([cb=std::move(cb),windows](Error err) {
        cb(GetWindowsReply{
            .windows = *windows,
            .err = err,
        });
    });
}

void
CoreApi::create_window(CreateWindowQuery query, Cb<CreateWindowReply> cb)
{
    db_create_window(allocate_window_id(), std::move(query), std::move(cb));
}

void
CoreApi::db_create_window(WindowId window_id, CreateWindowQuery query, Cb<CreateWindowReply> cb)
{
    auto change = std::make_shared<WindowsChange>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,query,window_id,change] {
        VERIFY(m_profiles_db->create_window(window_id, query, nullptr, change.get()));
        VERIFY(m_profiles_db->verify_windows_change(change.get()));
        SUCCEED();
    }).then_or_error([guard=shared_from_this(),this,cb=std::move(cb),window_id,change](Error err) {
        CreateWindowReply reply{
            .err = err,
            .id = window_id,
        };
        cb(std::move(reply));
        if (!change->created.empty() || !change->updated.empty() || !change->deleted.empty()) {
            m_windows_change->notify(std::move(*change));
        }
    });
}

WindowId
CoreApi::allocate_window_id()
{
    return m_profiles_db ? m_profiles_db->allocate_window_id() : WindowId(0);
}

void
CoreApi::db_apply_window_tab_checkpoint(WindowTabCheckpoint checkpoint, Cb<Error> cb)
{
    auto windows = std::make_shared<WindowsChange>();
    auto tabs = std::make_shared<TabsChange>();
    auto profiles = std::make_shared<ProfilesChange>();
    dispatch(on_disk_thread(), [guard=shared_from_this(), this,
        checkpoint=std::move(checkpoint), windows, tabs, profiles] {
        return m_profiles_db->apply_window_tab_checkpoint(checkpoint,
            windows.get(), tabs.get(), profiles.get());
    }).then_or_error([guard=shared_from_this(), this, cb=std::move(cb), windows, tabs, profiles](Error err) {
        // Failed transactions must not publish intermediate/rolled-back records.
        if(!err) {
            if(!tabs->created.empty() || !tabs->updated.empty() || !tabs->deleted.empty())
                m_tabs_change->notify(std::move(*tabs));
            if(!windows->created.empty() || !windows->updated.empty() || !windows->deleted.empty())
                m_windows_change->notify(std::move(*windows));
            if(!profiles->updated.empty()) m_profiles_change->notify(std::move(*profiles));
        }
        cb(std::move(err));
    });
}

void
CoreApi::update_window(UpdateWindowQuery query, Cb<Error> cb)
{
    auto window_change = std::make_shared<WindowsChange>();
    auto tab_change = std::make_shared<TabsChange>();
    auto back_forward_list_change = std::make_shared<TabBackForwardListEntriesChange>();

    if (query.update_selected_tab_id) {
        log_verbose("CoreApi::update_window: updating selected_tab_id for window_id=%lld to tab_id=%lld",
            query.id.value, query.selected_tab_id.value
        );
    }

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,query,window_change,tab_change,back_forward_list_change] {
        if(query.update_is_tab_group && !query.is_tab_group)
        {
            SaveWindowQuery save_window_query{.window_id = query.id};
            VERIFY(m_profiles_db->save_window(save_window_query, window_change.get(), tab_change.get(), back_forward_list_change.get()));
        }
        VERIFY(m_profiles_db->update_window(query, window_change.get()));
        VERIFY(m_profiles_db->verify_windows_change(window_change.get()));
        SUCCEED();
    }).then_or_error([guard=shared_from_this(),this,cb=std::move(cb),window_change,tab_change,back_forward_list_change](Error err) {
        cb(std::move(err));
        if (!window_change->created.empty() || !window_change->updated.empty() || !window_change->deleted.empty()) {
            m_windows_change->notify(std::move(*window_change));
        }
        if (!tab_change->created.empty() || !tab_change->updated.empty() || !tab_change->deleted.empty()) {
            m_tabs_change->notify(std::move(*tab_change));
        }
        if (!back_forward_list_change->created.empty() || !back_forward_list_change->updated.empty() || !back_forward_list_change->deleted.empty()) {
            m_tab_back_forward_list_entries_change->notify(std::move(*back_forward_list_change));
        }
    });
}

void
CoreApi::move_window(MoveWindowQuery query, Cb<Error> cb)
{
    auto window_change = std::make_shared<WindowsChange>();
    dispatch(on_disk_thread(), [guard=shared_from_this(),this,query,window_change] {
        VERIFY(m_profiles_db->move_window(query, window_change.get()));
        SUCCEED();
    }).then_or_error([guard=shared_from_this(),this,cb=std::move(cb),window_change](Error err) {
        cb(std::move(err));
        if (!window_change->created.empty() || !window_change->updated.empty() || !window_change->deleted.empty()) {
            m_windows_change->notify(std::move(*window_change));
        }
    });
}

void
CoreApi::delete_windows(const std::vector<WindowId>& ids, Cb<Error> cb)
{
    if (ids.empty()) {
        log_warning("CoreApi::delete_windows: called with empty ids vector");
    }

    auto change = std::make_shared<WindowsChange>();
    auto tabs_change = std::make_shared<TabsChange>();
    auto profiles_change = std::make_shared<ProfilesChange>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,ids,change,tabs_change,profiles_change] {
        VERIFY(m_profiles_db->delete_windows(ids, TxMode::use_tx, change.get(), tabs_change.get(), profiles_change.get()));
        VERIFY(m_profiles_db->verify_windows_change(change.get()));
        SUCCEED();
    }).then_or_error([guard=shared_from_this(),this,cb=std::move(cb),change,tabs_change,profiles_change](Error err) {
        cb(std::move(err));
        if (!tabs_change->created.empty() || !tabs_change->updated.empty() || !tabs_change->deleted.empty()) {
            m_tabs_change->notify(std::move(*tabs_change));
        }
        if (!change->created.empty() || !change->updated.empty() || !change->deleted.empty()) {
            m_windows_change->notify(std::move(*change));
        }
        if (!profiles_change->created.empty() || !profiles_change->updated.empty() || !profiles_change->deleted.empty()) {
            m_profiles_change->notify(std::move(*profiles_change));
        }
    });
}

void
CoreApi::close_window(WindowId window_id, Cb<Error> cb)
{
    auto change = std::make_shared<WindowsChange>();
    auto tabs_change = std::make_shared<TabsChange>();
    auto profiles_change = std::make_shared<ProfilesChange>();
    auto recently_closed_tabs_change = std::make_shared<RecentlyClosedTabsChange>();

    dispatch(on_disk_thread(), [=,guard=shared_from_this(),this] {
        VERIFY(m_profiles_db->close_window(
            window_id, TxMode::use_tx, change.get(), tabs_change.get(), profiles_change.get(), recently_closed_tabs_change.get()
        ));
        VERIFY(m_profiles_db->verify_windows_change(change.get()));
        SUCCEED();
    }).then_or_error([=,guard=shared_from_this(),this,cb=std::move(cb)](Error err) {
        cb(std::move(err));
        if (!tabs_change->created.empty() || !tabs_change->updated.empty() || !tabs_change->deleted.empty()) {
            m_tabs_change->notify(std::move(*tabs_change));
        }
        if (!change->created.empty() || !change->updated.empty() || !change->deleted.empty()) {
            m_windows_change->notify(std::move(*change));
        }
        if (!profiles_change->created.empty() || !profiles_change->updated.empty() || !profiles_change->deleted.empty()) {
            m_profiles_change->notify(std::move(*profiles_change));
        }
        if (!recently_closed_tabs_change->created.empty() || !recently_closed_tabs_change->deleted.empty()) {
            m_recently_closed_tabs_change->notify(std::move(*recently_closed_tabs_change));
        }
    });
}

int64_t
CoreApi::ref_ui_window()
{
    m_ui_window_ref_count++;
    log_verbose("ref_ui_window: win_count=%lld", m_ui_window_ref_count);
    return m_ui_window_ref_count;
}

int64_t
CoreApi::unref_ui_window()
{
    if (m_ui_window_ref_count == 0) {
        log_warning("unref_ui_window: ref count is already zero");
    } else {
        m_ui_window_ref_count--;
    }
    log_verbose("unref_ui_window: win_count=%lld", m_ui_window_ref_count);
    return m_ui_window_ref_count;
}

int64_t
CoreApi::get_ui_window_ref_count()
{
    return m_ui_window_ref_count;
}

void
CoreApi::register_ui_window(UiWindowId id)
{
    log_verbose("register_ui_window: ui_window_id=%p", id.value);

    if (id == UiWindowId(nullptr)) {
        log_warning("register_ui_window: attempt to register null UI window id");
        return;
    }

    auto it = std::find_if(m_window_records.begin(), m_window_records.end(), [id](const auto& record) {
        return record.ui_window_id == id;
    });
    if (it != m_window_records.end()) {
        log_warning("register_ui_window: attempt to register already registered UI window id %d", id);
        return;
    }
    WindowRecord record{
        .ui_window_id = id,
        .window_id = WindowId(0),
        .owner_window_id = WindowId(0),
    };
    m_window_records.push_back(record);
    m_ui_windows_change->notify(UiWindowsChange{.created={id}});
}

void
CoreApi::unregister_ui_window(UiWindowId id)
{
    log_verbose("unregister_ui_window: ui_window_id=%p", id.value);

    if (id == UiWindowId(nullptr)) {
        log_warning("unregister_ui_window: attempt to unregister null UI window id");
        return;
    }

    auto it = std::find_if(m_window_records.begin(), m_window_records.end(), [id](const auto& record) {
        return record.ui_window_id == id;
    });
    if (it == m_window_records.end()) {
        log_warning("unregister_ui_window: attempt to unregister non-registered UI window id %p", id.value);
        return;
    }
    m_window_records.erase(it);
    m_ui_windows_change->notify(UiWindowsChange{.deleted={id}});
}

std::vector<UiWindowId>
CoreApi::get_registered_ui_window_ids() const
{
    // defensive copy
    std::vector<UiWindowId> ids;
    for (const auto& rec : m_window_records) {
        if (rec.ui_window_id != UiWindowId(nullptr)) {
            ids.push_back(rec.ui_window_id);
        }
    }
    return ids;
}

void
CoreApi::debug_log_window_records() const
{
    std::vector<std::string> debug_records;
    for (const auto& rec : m_window_records) {
        std::ostringstream oss;
        oss << std::hex << rec.ui_window_id.value;
        std::string str = "{ui_wnd_id=" + oss.str() + " wnd_id=" + std::to_string(rec.window_id.value)
            + " owner_wnd_id=" + std::to_string(rec.owner_window_id.value) + "}";
        debug_records.push_back(str);
    }
    log_verbose("windows: [%s]", str::join_strings(debug_records, " ").c_str());
}

void
CoreApi::set_window_id_for_ui_window(UiWindowId ui_window_id, WindowId window_id)
{
    log_verbose("set_window_id_for_ui_window: ui_window_id=%p window_id=%lld",
        ui_window_id.value, window_id.value
    );

    if (ui_window_id == UiWindowId(nullptr)) {
        log_warning("set_window_id_for_ui_window: attempt to set window id for null UI window id");
        return;
    }

    auto it = std::find_if(m_window_records.begin(), m_window_records.end(), [ui_window_id](const auto& record) {
        return record.ui_window_id == ui_window_id;
    });
    if (it == m_window_records.end()) {
        log_warning("set_window_id_for_ui_window: attempt to set window id for non-registered UI window id %d", ui_window_id);
        return;
    }
    it->window_id = window_id;

    debug_log_window_records();
}

std::vector<UiWindowId>
CoreApi::get_ui_windows_by_window_id(WindowId window_id) const
{
    std::vector<UiWindowId> ids;
    for (const auto& rec : m_window_records) {
        if (rec.window_id == window_id && rec.ui_window_id != UiWindowId(nullptr)) {
            ids.push_back(rec.ui_window_id);
        }
    }
    return ids;
}

void
CoreApi::set_owner_for_ui_window(UiWindowId ui_window_id, WindowId owner_window_id)
{
    log_verbose("set_owner_for_ui_window: ui_window_id=%p owner_window_id=%lld",
        ui_window_id.value, owner_window_id.value
    );

    if (ui_window_id == UiWindowId(nullptr)) {
        log_warning("set_owner_for_ui_window: attempt to set owner for null UI window id");
        return;
    }

    auto it = std::find_if(m_window_records.begin(), m_window_records.end(), [ui_window_id](const auto& record) {
        return record.ui_window_id == ui_window_id;
    });
    if (it == m_window_records.end()) {
        log_warning("set_owner_for_ui_window: attempt to set owner for non-registered UI window id %d", ui_window_id);
        return;
    }
    
    if (it->owner_window_id.value && it->owner_window_id != owner_window_id) {
        log_warning("set_window_id_for_ui_window: changing ownership of UI window id %p from window id %lld to window id %lld",
            ui_window_id, it->window_id.value, owner_window_id.value
        );
    }

    std::vector<UiWindowId> existing_owned_ui_windows = get_ui_window_ids_owned_by_window_id(owner_window_id);
    if (!existing_owned_ui_windows.empty()) {
        log_warning("set_owner_for_ui_window: window id %lld already owns %zu UI window ids, will now also own UI window id %p",
            owner_window_id.value, existing_owned_ui_windows.size(), ui_window_id.value
        );
    }

    it->owner_window_id = owner_window_id;

    debug_log_window_records();
}

WindowId
CoreApi::get_owner_window_id_for_ui_window(UiWindowId ui_window_id) const
{
    if (ui_window_id == UiWindowId(nullptr)) {
        log_warning("get_owner_window_id_for_ui_window: attempt to get owner for null UI window id");
        return WindowId(0);
    }

    auto it = std::find_if(m_window_records.begin(), m_window_records.end(), [ui_window_id](const auto& record) {
        return record.ui_window_id == ui_window_id;
    });
    if (it == m_window_records.end()) {
        log_warning("get_owner_for_ui_window: attempt to get owner for non-registered UI window id %p",
            ui_window_id.value
        );
        return WindowId(0);
    }
    return it->owner_window_id;
}

std::vector<UiWindowId>
CoreApi::get_ui_window_ids_owned_by_window_id(WindowId owner_window_id) const
{
    std::vector<UiWindowId> ids;
    for (const auto& rec : m_window_records) {
        if (rec.owner_window_id == owner_window_id && rec.ui_window_id != UiWindowId(nullptr)) {
            ids.push_back(rec.ui_window_id);
        }
    }
    return ids;
}

std::vector<UiWindowId>
CoreApi::get_ui_window_ids() const
{
    std::vector<UiWindowId> ids;
    for (const auto& rec : m_window_records) {
        if (rec.ui_window_id != UiWindowId(nullptr)) {
            ids.push_back(rec.ui_window_id);
        }
    }
    return ids;
}

void
CoreApi::duplicate_window(DuplicateWindowQuery query, Cb<DuplicateWindowReply> cb)
{
    auto windows_change = std::make_shared<WindowsChange>();
    auto tabs_change = std::make_shared<TabsChange>();
    auto new_window_id = std::make_shared<WindowId>(WindowId(0));

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,query,windows_change,tabs_change,new_window_id] {
        VERIFY(m_profiles_db->duplicate_window(query, new_window_id.get(), windows_change.get(), tabs_change.get()));
        VERIFY(m_profiles_db->verify_windows_change(windows_change.get()));
        SUCCEED();
    }).then_or_error([guard=shared_from_this(),this,cb=std::move(cb),windows_change,tabs_change,new_window_id](Error err) {
        DuplicateWindowReply reply{
            .err = err,
            .new_window_id = *new_window_id,
        };
        cb(std::move(reply));
        if (!windows_change->created.empty() || !windows_change->updated.empty() || !windows_change->deleted.empty()) {
            m_windows_change->notify(std::move(*windows_change));
        }
        if (!tabs_change->created.empty() || !tabs_change->updated.empty() || !tabs_change->deleted.empty()) {
            m_tabs_change->notify(std::move(*tabs_change));
        }
    });
}

//-------------------------------------------------------------------------
// Tabs
//-------------------------------------------------------------------------

void
CoreApi::get_tab_ids(GetTabIdsQuery query, Cb<GetTabIdsReply> cb)
{
    auto ids = std::make_shared<std::vector<TabId>>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,ids,query] {
        VERIFY(m_profiles_db->get_tab_ids(query, ids.get()));
        SUCCEED();
    }).then_or_error([cb=std::move(cb),ids](Error err) {
        GetTabIdsReply reply{
            .ids = *ids,
            .err = err,
        };
        cb(std::move(reply));
    });
}

void
CoreApi::get_other_tab_ids(GetOtherTabIdsQuery query, Cb<GetOtherTabIdsReply> cb)
{
    auto other_ids = std::make_shared<std::vector<TabId>>();
    auto before_ids = std::make_shared<std::vector<TabId>>();
    auto after_ids = std::make_shared<std::vector<TabId>>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,query,other_ids,before_ids,after_ids] {
        VERIFY(m_profiles_db->get_other_tab_ids(query, other_ids.get(), before_ids.get(), after_ids.get()));
        SUCCEED();
    }).then_or_error([cb=std::move(cb),other_ids,before_ids,after_ids](Error err) {
        GetOtherTabIdsReply reply{
            .err = err,
            .other_tab_ids = *other_ids,
            .before_tab_ids = *before_ids,
            .after_tab_ids = *after_ids,
        };
        cb(std::move(reply));
    });
}

void
CoreApi::get_descendant_tab_ids(TabId tab_id, Cb<GetDescendantTabIdsReply> cb)
{
    auto tab_ids = std::make_shared<std::vector<TabId>>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,tab_id,tab_ids] {
        VERIFY(m_profiles_db->get_descendant_tab_ids(tab_id, tab_ids.get()));
        SUCCEED();
    }).then_or_error([cb=std::move(cb),tab_ids](Error err) {
        GetDescendantTabIdsReply reply{
            .err = err,
            .descendant_tab_ids = *tab_ids,
        };
        cb(std::move(reply));
    });
}

void
CoreApi::get_tabs(const std::vector<TabId>& ids, Cb<GetTabsReply> cb)
{
    auto tabs = std::make_shared<std::vector<TabDto>>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,ids,tabs] {
        VERIFY(m_profiles_db->get_tabs(ids, tabs.get()));
        SUCCEED();
    }).then_or_error([cb=std::move(cb),tabs](Error err) {
        GetTabsReply reply{
            .tabs = *tabs,
            .err = err,
        };
        cb(std::move(reply));
    });
}

void
CoreApi::get_tabs_by_query(GetTabIdsQuery query, Cb<GetTabsReply> cb)
{
    auto tabs = std::make_shared<std::vector<TabDto>>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,query,tabs] {
        std::vector<TabId> ids;
        VERIFY(m_profiles_db->get_tab_ids(query, &ids));
        VERIFY(m_profiles_db->get_tabs(ids, tabs.get()));
        SUCCEED();
    }).then_or_error([cb=std::move(cb),tabs](Error err) {
        cb(GetTabsReply{
            .tabs = *tabs,
            .err = err,
        });
    });
}

void
CoreApi::create_tab(CreateTabQuery query, Cb<CreateTabReply> cb)
{
    auto tab_id = std::make_shared<TabId>(TabId(0));
    auto change = std::make_shared<TabsChange>();
    auto window_change = std::make_shared<WindowsChange>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,query,tab_id,change,window_change] {
        // Maybe use a single transaction here
        VERIFY(m_profiles_db->create_tab(query, tab_id.get(), TxMode::use_tx, change.get(), window_change.get()));
        VERIFY(m_profiles_db->normalize_tab_seq_nums(query.window_id, TxMode::use_tx, change.get()));
        VERIFY(m_profiles_db->verify_windows_change(window_change.get()));
        SUCCEED();
    }).then_or_error([guard=shared_from_this(),this,cb=std::move(cb),tab_id,change,window_change](Error err) {
        CreateTabReply reply{
            .err = err,
            .id = *tab_id,
        };
        cb(std::move(reply));
        if (!change->created.empty() || !change->updated.empty() || !change->deleted.empty()) {
            m_tabs_change->notify(std::move(*change));
        }
        if (!window_change->created.empty() || !window_change->updated.empty() || !window_change->deleted.empty()) {
            m_windows_change->notify(std::move(*window_change));
        }
    });
}

TabId
CoreApi::allocate_tab_id()
{
    return m_profiles_db ? m_profiles_db->allocate_tab_id() : TabId(0);
}

void
CoreApi::db_create_tab(TabId tab_id, CreateTabQuery query, Cb<CreateTabReply> cb)
{
    auto change = std::make_shared<TabsChange>();
    auto window_change = std::make_shared<WindowsChange>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,query,tab_id,change,window_change] {
        VERIFY(m_profiles_db->create_tab(
            tab_id, query, nullptr, TxMode::use_tx, change.get(), window_change.get()));
        VERIFY(m_profiles_db->normalize_tab_seq_nums(
            query.window_id, TxMode::use_tx, change.get()));
        VERIFY(m_profiles_db->verify_windows_change(window_change.get()));
        SUCCEED();
    }).then_or_error([guard=shared_from_this(),this,cb=std::move(cb),tab_id,change,window_change](Error err) {
        cb(CreateTabReply{.err = err, .id = tab_id});
        if (!change->created.empty() || !change->updated.empty() || !change->deleted.empty()) {
            m_tabs_change->notify(std::move(*change));
        }
        if (!window_change->created.empty() || !window_change->updated.empty() || !window_change->deleted.empty()) {
            m_windows_change->notify(std::move(*window_change));
        }
    });
}

void
CoreApi::db_delete_tabs(const std::vector<TabId>& ids, Cb<Error> cb)
{
    delete_tabs(ids, std::move(cb));
}

void
CoreApi::update_tab(UpdateTabQuery query, Cb<Error> cb)
{
    update_tabs({query}, std::move(cb));
}

void
CoreApi::update_tabs(const std::vector<UpdateTabQuery>& queries, Cb<Error> cb)
{
    if (queries.empty()) {
        log_warning("CoreApi::update_tabs: called with empty queries vector");
    }

    auto change = std::make_shared<TabsChange>();
    auto window_change = std::make_shared<WindowsChange>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,queries,change,window_change] {
        // Maybe use a single transaction here

        // Do not normalize after each update, but only once per window
        std::set<WindowId> normalize_window_ids{};

        for (const auto& query : queries) {
            VERIFY(m_profiles_db->update_tab(query, TxMode::use_tx, change.get(), window_change.get()));

            if (query.update_window_id || query.update_seq_num) {
                normalize_window_ids.insert(query.window_id);
            }
        }

        if (change->updated_seq_num || change->updated_window_id) {
            for (const auto& create : change->created) {
                normalize_window_ids.insert(create.window_id);
            }
            for (const auto& update : change->updated) {
                normalize_window_ids.insert(update.window_id);
            }
            // Ignore deletes
        }

        for (const auto& window_id : normalize_window_ids) {
            if (window_id.value) {
                VERIFY(m_profiles_db->normalize_tab_seq_nums(window_id, TxMode::use_tx, change.get()));
            }
        }
        VERIFY(m_profiles_db->verify_windows_change(window_change.get()));
        SUCCEED();
    }).then_or_error([guard=shared_from_this(),this,cb=std::move(cb),change,window_change](Error err) {
        cb(std::move(err));
        if (!change->created.empty() || !change->updated.empty() || !change->deleted.empty()) {
            m_tabs_change->notify(std::move(*change));
        }
        if (!window_change->created.empty() || !window_change->updated.empty() || !window_change->deleted.empty()) {
            m_windows_change->notify(std::move(*window_change));
        }
    });
}

void
CoreApi::move_tab(MoveTabQuery query, Cb<Error> cb)
{
    auto change = std::make_shared<TabsChange>();
    auto window_change = std::make_shared<WindowsChange>();
    auto profiles_change = std::make_shared<ProfilesChange>();
    auto recently_closed_change = std::make_shared<RecentlyClosedTabsChange>();
    dispatch(on_disk_thread(), [guard=shared_from_this(),this,query,change,window_change,profiles_change,recently_closed_change] {
        VERIFY(m_profiles_db->move_tab(query, TxMode::use_tx, change.get(), window_change.get(), profiles_change.get(), recently_closed_change.get()));
        VERIFY(m_profiles_db->verify_windows_change(window_change.get()));
        SUCCEED();
    }).then_or_error([guard=shared_from_this(),this,cb=std::move(cb),change,window_change,profiles_change,recently_closed_change](Error err) {
        cb(std::move(err));
        if (!change->created.empty() || !change->updated.empty() || !change->deleted.empty()) {
            m_tabs_change->notify(std::move(*change));
        }
        if (!window_change->created.empty() || !window_change->updated.empty() || !window_change->deleted.empty()) {
            m_windows_change->notify(std::move(*window_change));
        }
        if (!profiles_change->created.empty() || !profiles_change->updated.empty() || !profiles_change->deleted.empty()) {
            m_profiles_change->notify(std::move(*profiles_change));
        }
        if (!recently_closed_change->created.empty() || !recently_closed_change->deleted.empty()) {
            m_recently_closed_tabs_change->notify(std::move(*recently_closed_change));
        }
    });
}

void
CoreApi::delete_tabs(const std::vector<TabId>& ids, Cb<Error> cb)
{
    if (ids.empty()) {
        log_warning("CoreApi::delete_tabs: called with empty ids vector");
    }

    auto change = std::make_shared<TabsChange>();
    auto window_change = std::make_shared<WindowsChange>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,ids,change,window_change] {
        VERIFY(m_profiles_db->delete_tabs(ids, TxMode::use_tx, change.get(), window_change.get()));
        VERIFY(m_profiles_db->verify_windows_change(window_change.get()));
        SUCCEED();
    }).then_or_error([guard=shared_from_this(),this,cb=std::move(cb),change,window_change](Error err) {
        cb(std::move(err));
        if (!change->created.empty() || !change->updated.empty() || !change->deleted.empty()) {
            m_tabs_change->notify(std::move(*change));
        }
        if (!window_change->created.empty() || !window_change->updated.empty() || !window_change->deleted.empty()) {
            m_windows_change->notify(std::move(*window_change));
        }
    });
}

void
CoreApi::duplicate_tab(DuplicateTabQuery query, Cb<DuplicateTabReply> cb)
{
    auto change = std::make_shared<TabsChange>();
    auto window_change = std::make_shared<WindowsChange>();
    auto new_tab = std::make_shared<TabDto>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,query,change,new_tab,window_change] {
        VERIFY(m_profiles_db->duplicate_tab(query, new_tab.get(), change.get(), window_change.get()));
        VERIFY(m_profiles_db->verify_windows_change(window_change.get()));
        SUCCEED();
    }).then_or_error([guard=shared_from_this(),this,cb=std::move(cb),change,new_tab,window_change](Error err) {
        DuplicateTabReply reply{
            .err = err,
            .new_tab = *new_tab,
        };
        cb(std::move(reply));
        if (!change->created.empty() || !change->updated.empty() || !change->deleted.empty()) {
            m_tabs_change->notify(std::move(*change));
        }
        if (!window_change->created.empty() || !window_change->updated.empty() || !window_change->deleted.empty()) {
            m_windows_change->notify(std::move(*window_change));
        }
    });
}

void
CoreApi::suspend_tabs(const std::vector<TabId>& ids, Cb<Error> cb)
{
    if (ids.empty()) {
        log_warning("CoreApi::suspend_tabs: called with empty ids vector");
    }

    auto change = std::make_shared<TabsChange>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,ids,change] {
        VERIFY(m_profiles_db->suspend_tabs(ids, change.get()));
        SUCCEED();
    }).then_or_error([guard=shared_from_this(),this,cb=std::move(cb),change](Error err) {
        cb(std::move(err));
        if (!change->created.empty() || !change->updated.empty() || !change->deleted.empty()) {
            m_tabs_change->notify(std::move(*change));
        }
    });
}

void
CoreApi::can_move_tab(CanMoveTabQuery query, Cb<CanMoveTabReply> cb)
{
    auto can_move = std::make_shared<bool>(false);
    
    dispatch(on_disk_thread(), [guard=shared_from_this(),this,query,can_move] {
        VERIFY(m_profiles_db->can_move_tab(query, can_move.get()));
        SUCCEED();
    }).then_or_error([guard=shared_from_this(),this,cb=std::move(cb),can_move](Error err){
        CanMoveTabReply reply{
            .err = err,
            .can_move = *can_move,
        };
        cb(std::move(reply));
    });
}

//-------------------------------------------------------------------------
// Tab back-forward list entries
//-------------------------------------------------------------------------

void
CoreApi::get_tab_back_forward_list_entries(TabId tab_id, Cb<GetTabBackForwardListReply> cb)
{
    auto entries = std::make_shared<std::vector<TabBackForwardListEntryDto>>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,tab_id,entries] {
        VERIFY(m_profiles_db->get_tab_back_forward_list_entries(tab_id, entries.get()));
        SUCCEED();
    }).then_or_error([cb=std::move(cb),entries](Error err) {
        GetTabBackForwardListReply reply{
            .err = err,
            .entries = *entries,
        };
        cb(std::move(reply));
    });
}

void
CoreApi::create_tab_back_forward_list_entry(
    CreateTabBackForwardListEntryQuery query, Cb<CreateTabBackForwardListEntryReply> cb)
{
    auto entry = std::make_shared<TabBackForwardListEntryDto>();
    auto change = std::make_shared<TabBackForwardListEntriesChange>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,query,entry,change] {
        VERIFY(m_profiles_db->create_tab_back_forward_list_entry(query, entry.get(), change.get()));
        SUCCEED();
    }).then_or_error([guard=shared_from_this(),this,cb=std::move(cb),entry,change](Error err) {
        CreateTabBackForwardListEntryReply reply{
            .err = err,
            .entry = *entry,
        };
        cb(std::move(reply));
        if (!change->created.empty() || !change->updated.empty() || !change->deleted.empty()) {
            m_tab_back_forward_list_entries_change->notify(std::move(*change));
        }
    });
}

void
CoreApi::delete_tab_back_forward_list_entries(DeleteTabBackForwardListEntriesQuery query, Cb<Error> cb)
{
    auto change = std::make_shared<TabBackForwardListEntriesChange>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,query,change] {
        VERIFY(m_profiles_db->delete_tab_back_forward_list_entries(query, change.get()));
        SUCCEED();
    }).then_or_error([guard=shared_from_this(),this,cb=std::move(cb),change](Error err) {
        cb(std::move(err));
        if (!change->created.empty() || !change->updated.empty() || !change->deleted.empty()) {
            m_tab_back_forward_list_entries_change->notify(std::move(*change));
        }
    });
}

//-------------------------------------------------------------------------
// Recently Closed Tabs
//-------------------------------------------------------------------------

void
CoreApi::get_recently_closed_tab_ids(GetRecentlyClosedTabIdsQuery query, Cb<GetRecentlyClosedTabIdsReply> cb)
{
    auto ids = std::make_shared<std::vector<RecentlyClosedTabId>>();

    dispatch(on_disk_thread(), [=,guard=shared_from_this(),this] {
        VERIFY(m_profiles_db->get_recently_closed_tab_ids(query, ids.get()));
        SUCCEED();
    }).then_or_error([cb=std::move(cb),ids](Error err) {
        GetRecentlyClosedTabIdsReply reply{
            .ids = *ids,
            .err = err,
        };
        cb(std::move(reply));
    });
}

void
CoreApi::get_recently_closed_tabs(const std::vector<RecentlyClosedTabId>& ids, Cb<GetRecentlyClosedTabsReply> cb)
{
    auto tabs = std::make_shared<std::vector<RecentlyClosedTabDto>>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,ids,tabs] {
        VERIFY(m_profiles_db->get_recently_closed_tabs(ids, tabs.get()));
        SUCCEED();
    }).then_or_error([cb=std::move(cb),tabs](Error err) {
        GetRecentlyClosedTabsReply reply{
            .tabs = *tabs,
            .err = err,
        };
        cb(std::move(reply));
    });
}

void
CoreApi::create_recently_closed_tab(CreateRecentlyClosedTabQuery query, Cb<CreateRecentlyClosedTabReply> cb)
{
    auto tab_id = std::make_shared<RecentlyClosedTabId>();
    auto change = std::make_shared<RecentlyClosedTabsChange>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,query,tab_id,change] {
        VERIFY(m_profiles_db->create_recently_closed_tab(query, tab_id.get(), change.get()));
        SUCCEED();
    }).then_or_error([guard=shared_from_this(),this,cb=std::move(cb),tab_id,change](Error err) {
        CreateRecentlyClosedTabReply reply{
            .err = err,
            .id = *tab_id,
        };
        cb(std::move(reply));

        if (!change->created.empty() || !change->deleted.empty()) {
            m_recently_closed_tabs_change->notify(std::move(*change));
        }
    });
}

void
CoreApi::delete_recently_closed_tabs(DeleteRecentlyClosedTabsQuery query, Cb<Error> cb)
{
    if (query.ids.empty()) {
        log_warning("CoreApi::delete_recently_closed_tabs: called with empty ids vector");
    }

    auto change = std::make_shared<RecentlyClosedTabsChange>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,query,change] {
        VERIFY(m_profiles_db->delete_recently_closed_tabs(query, TxMode::use_tx, change.get()));
        SUCCEED();
    }).then_or_error([guard=shared_from_this(),this,cb=std::move(cb),change](Error err) {
        cb(std::move(err));
        
        if (!change->created.empty() || !change->deleted.empty()) {
            m_recently_closed_tabs_change->notify(std::move(*change));
        }
    });
}

//-------------------------------------------------------------------------
// Recently Closed Windows
//-------------------------------------------------------------------------

void
CoreApi::get_recently_closed_window_ids(ProfileId profile_id, Cb<GetRecentlyClosedWindowIdsReply> cb)
{
    auto ids = std::make_shared<std::vector<RecentlyClosedWindowId>>();

    dispatch(on_disk_thread(), [=,guard=shared_from_this(),this] {
        VERIFY(m_profiles_db->get_recently_closed_window_ids(profile_id, ids.get()));
        SUCCEED();
    }).then_or_error([cb=std::move(cb),ids](Error err) {
        GetRecentlyClosedWindowIdsReply reply{
            .ids = *ids,
            .err = err,
        };
        cb(std::move(reply));
    });
}

void
CoreApi::get_recently_closed_windows(
    const std::vector<RecentlyClosedWindowId>& ids, Cb<GetRecentlyClosedWindowsReply> cb)
{
    auto windows = std::make_shared<std::vector<RecentlyClosedWindowDto>>();

    dispatch(on_disk_thread(), [=,guard=shared_from_this(),this] {
        VERIFY(m_profiles_db->get_recently_closed_windows(ids, windows.get()));
        SUCCEED();
    }).then_or_error([=,cb=std::move(cb)](Error err) {
        GetRecentlyClosedWindowsReply reply{
            .windows = *windows,
            .err = err,
        };
        cb(std::move(reply));
    });
}

void
CoreApi::get_recently_closed_windows_by_query(ProfileId profile_id, Cb<GetRecentlyClosedWindowsReply> cb)
{
    auto windows = std::make_shared<std::vector<RecentlyClosedWindowDto>>();

    dispatch(on_disk_thread(), [=,guard=shared_from_this(),this] {
        std::vector<RecentlyClosedWindowId> ids;
        VERIFY(m_profiles_db->get_recently_closed_window_ids(profile_id, &ids));
        VERIFY(m_profiles_db->get_recently_closed_windows(ids, windows.get()));
        SUCCEED();
    }).then_or_error([=,cb=std::move(cb)](Error err) {
        cb(GetRecentlyClosedWindowsReply{
            .windows = *windows,
            .err = err,
        });
    });
}

void
CoreApi::delete_recently_closed_windows(const std::vector<RecentlyClosedWindowId>& ids, Cb<Error> cb)
{
    if (ids.empty()) {
        log_warning("CoreApi::delete_recently_closed_windows: called with empty ids vector");
    }

    dispatch(on_disk_thread(), [=,guard=shared_from_this(),this] {
        VERIFY(m_profiles_db->delete_recently_closed_windows(ids));
        SUCCEED();
    }).then_or_error([=,cb=std::move(cb)](Error err) {
        cb(std::move(err));
    });
}

//-------------------------------------------------------------------------
// Undo Redo Tab Steps
//-------------------------------------------------------------------------

void
CoreApi::is_undo_redo_tab_step_available(IsUndoRedoTabStepAvailableQuery query, Cb<IsUndoRedoTabStepAvailableReply> cb)
{
    auto reply = std::make_shared<IsUndoRedoTabStepAvailableReply>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,query,reply] {
        VERIFY(m_profiles_db->is_undo_redo_tab_step_available(query, reply.get()));
        SUCCEED();
    }).then_or_error([cb=std::move(cb),reply](Error err) {
        reply->err = err;
        cb(*reply);
    });
}

void
CoreApi::create_undo_redo_tab_step(CreateUndoRedoTabStepQuery query, Cb<CreateUndoRedoTabStepReply> cb)
{
    auto step_id = std::make_shared<UndoRedoTabStepId>(UndoRedoTabStepId(0));
    auto windows_change = std::make_shared<WindowsChange>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,query,step_id,windows_change] {
        VERIFY(m_profiles_db->create_undo_redo_tab_step(query, TxMode::use_tx, step_id.get(), windows_change.get()));
        VERIFY(m_profiles_db->verify_windows_change(windows_change.get()));
        SUCCEED();
    }).then_or_error([guard=shared_from_this(),this,cb=std::move(cb),step_id,windows_change](Error err) {
        CreateUndoRedoTabStepReply reply{
            .err = err,
            .id = *step_id,
        };
        cb(std::move(reply));
        if (!windows_change->created.empty() || !windows_change->updated.empty() || !windows_change->deleted.empty()) {
            m_windows_change->notify(*windows_change);
        }
    });
}

void
CoreApi::change_undo_redo_tab_step(ChangeUndoRedoTabStepQuery query, Cb<ChangeUndoRedoTabStepReply> cb)
{
    auto windows_change = std::make_shared<WindowsChange>();
    auto tabs_change = std::make_shared<TabsChange>();
    auto tab_back_forward_list_entries_change = std::make_shared<TabBackForwardListEntriesChange>();

    dispatch(on_disk_thread(), [=,guard=shared_from_this(),this] {
        VERIFY(m_profiles_db->change_undo_redo_tab_step(
            query, TxMode::use_tx, windows_change.get(), tabs_change.get(),
            tab_back_forward_list_entries_change.get()
        ));
        VERIFY(m_profiles_db->verify_windows_change(windows_change.get()));
        SUCCEED();
    }).then_or_error([=,guard=shared_from_this(),this,cb=std::move(cb)](Error err) {
        ChangeUndoRedoTabStepReply reply{
            .err = err,
        };
        cb(std::move(reply));
        if (!windows_change->created.empty() || !windows_change->updated.empty() || !windows_change->deleted.empty()) {
            m_windows_change->notify(*windows_change);
        }
        if (!tabs_change->created.empty() || !tabs_change->updated.empty() || !tabs_change->deleted.empty()) {
            m_tabs_change->notify(*tabs_change);
        }
        if (!tab_back_forward_list_entries_change->created.empty()
         || !tab_back_forward_list_entries_change->updated.empty()
         || !tab_back_forward_list_entries_change->deleted.empty()
        ) {
            m_tab_back_forward_list_entries_change->notify(*tab_back_forward_list_entries_change);
        }
    });
}

//-------------------------------------------------------------------------
// Settings
//-------------------------------------------------------------------------

void
CoreApi::set_setting_internal(
    const SetSettingQuery& query,
    Cb<SetSettingReply<std::string>> cb,
    std::shared_ptr<SettingsChange> settings_change,
    std::shared_ptr<SingleSettingAnyChange> single_change)
{
    auto old_value = std::make_shared<std::string>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,query,old_value,settings_change,single_change] {
        VERIFY(m_profiles_db->set_setting(query, old_value.get(), settings_change.get(), single_change.get()));
        SUCCEED();
    }).then_or_error([guard=shared_from_this(),this,cb=std::move(cb),query,old_value](Error err) {
        SetSettingReply<std::string> reply{};
        reply.err = err;
        reply.new_value = query.value;
        if (old_value) {
            reply.old_value = *old_value;
        }
        // reply.setting and reply.default_value are left empty here, as they will be filled in the parent lambda
        cb(std::move(reply));
    });
}

void
CoreApi::set_common_setting_internal(
    const SetCommonSettingQuery& query,
    Cb<SetSettingReply<std::string>> cb,
    std::shared_ptr<CommonSettingsChange> settings_change,
    std::shared_ptr<SingleCommonSettingAnyChange> single_change)
{
    auto old_value = std::make_shared<std::string>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,query,old_value,settings_change,single_change] {
        VERIFY(m_profiles_db->set_common_setting(query, old_value.get(), settings_change.get(), single_change.get()));
        SUCCEED();
    }).then_or_error([guard=shared_from_this(),this,cb=std::move(cb),query,old_value](Error err) {
        SetSettingReply<std::string> reply{};
        reply.err = err;
        reply.new_value = query.value;
        if (old_value) {
            reply.old_value = *old_value;
        }
        // reply.setting and reply.default_value are left empty here, as they will be filled in the parent lambda
        cb(std::move(reply));
    });
}

void
CoreApi::toggle_setting_internal(
    const SetSettingQuery& query, Cb<SetSettingReply<std::string>> cb, std::shared_ptr<SettingsChange> settings_change,
    std::shared_ptr<SingleSettingAnyChange> single_change)
{
    auto old_value = std::make_shared<std::string>();
    auto toggle_query = std::make_shared<SetSettingQuery>(query); // make a copy, we need to set .value field later

    dispatch(on_disk_thread(), [=,guard=shared_from_this(),this] {
        GetSettingQuery get_query{
            .profile_id = query.profile_id,
            .key = query.key,
            .realm = query.realm,
            .key_name = query.key_name,
            .type = query.type,
            .default_value = query.default_value,
        };
        VERIFY(m_profiles_db->get_setting(get_query, old_value.get()));
        bool v{};
        VERIFY(settings::unmarshal_value(*old_value, &v));
        toggle_query->value = settings::marshal_value(!v);
        VERIFY(m_profiles_db->set_setting(*toggle_query, old_value.get(), settings_change.get(), single_change.get()));
        SUCCEED();
    }).then_or_error([=,guard=shared_from_this(),this,cb=std::move(cb)](Error err) {
        SetSettingReply<std::string> reply{};
        reply.new_value = toggle_query->value;
        reply.err = err;
        if (old_value) {
            reply.old_value = *old_value;
        }
        // reply.setting and reply.default_value are left empty here, as they will be filled in the parent lambda
        cb(std::move(reply));
    });
}

void CoreApi::set_setting_raw(const SetSettingQuery& query, Cb<SetSettingReply<std::string>> cb) {
    auto settings_change = std::make_shared<SettingsChange>();
    auto single_change = std::make_shared<SingleSettingAnyChange>();

    set_setting_internal(query, [guard=shared_from_this(),this,cb=std::move(cb),settings_change,single_change](SetSettingReply<std::string> reply) {
        bool success = !reply.err;
        cb(std::move(reply));
        if (success) {
            notify_settings_change_internal(*settings_change, *single_change);
        }
    }, settings_change, single_change);
}

void CoreApi::get_setting_raw(const GetSettingQuery& query, Cb<GetSettingReply<std::string>> cb) {
    get_setting_internal(query, std::move(cb));
}

void
CoreApi::get_setting_or_default_raw(const GetSettingOrDefaultQuery& query, Cb<GetSettingReply<std::string>> cb)
{
    auto value = std::make_shared<std::string>();
    auto default_value = std::make_shared<std::string>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,query,value,default_value] {
        VERIFY(m_profiles_db->get_setting_or_default(query, value.get(), default_value.get()));
        SUCCEED();
    }).then_or_error([cb=std::move(cb),value,default_value](Error err) {
        GetSettingReply<std::string> reply{};
        reply.default_value = *default_value;
        reply.value = *value;
        reply.err = err;
        cb(std::move(reply));
    });
}

void
CoreApi::set_setting_or_default_internal(
    const SetSettingOrDefaultQuery& query,
    Cb<SetSettingReply<std::string>> cb,
    std::shared_ptr<SettingsChange> settings_change,
    std::shared_ptr<SingleSettingAnyChange> single_change)
{
    auto old_value = std::make_shared<std::string>();
    auto default_value = std::make_shared<std::string>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,query,old_value,default_value,settings_change,single_change] {
        VERIFY(m_profiles_db->set_setting_or_default(query, old_value.get(), default_value.get(), settings_change.get(), single_change.get()));
        SUCCEED();
    }).then_or_error([guard=shared_from_this(),this,cb=std::move(cb),query,old_value,default_value,settings_change,single_change](Error err) {
        SetSettingReply<std::string> reply{};
        reply.err = err;
        reply.new_value = query.value;
        reply.default_value = *default_value;
        if (old_value) {
            reply.old_value = *old_value;
        }
        cb(std::move(reply));
        bool success = !reply.err;
        if (success) {
            notify_settings_change_internal(*settings_change, *single_change);
        }
    });
}

void
CoreApi::get_settings_by_key_prefix(ProfileId profile_id, const std::string& key_prefix, Cb<GetSettingsByKeyPrefixReply> cb)
{
    auto settings = std::make_shared<std::vector<RawSettingDto>>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,profile_id,key_prefix,settings] {
        VERIFY(m_profiles_db->get_settings_by_key_prefix(profile_id, key_prefix, settings.get()));
        SUCCEED();
    }).then_or_error([cb=std::move(cb),settings](Error err) {
        cb(GetSettingsByKeyPrefixReply{
            .settings = *settings,
            .err = err,
        });
    });
}

void
CoreApi::get_setting_for_website_or_default(
    const GetSettingForWebsiteOrDefaultQuery& query, Cb<GetSettingForWebsiteOrDefaultReply> cb)
{
    auto value = std::make_shared<std::string>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,query,value] {
        VERIFY(m_profiles_db->get_setting_for_website_or_default(query, value.get()));
        SUCCEED();
    }).then_or_error([cb=std::move(cb),value](Error err) {
        GetSettingForWebsiteOrDefaultReply reply{
            .err = err,
            .value = *value,
        };
        cb(std::move(reply));
    });
}

void
CoreApi::increment_and_clamp_setting(
    const IncrementAndClampSettingQuery& query, Cb<SetSettingReply<int64_t>> cb)
{
    auto old_value = std::make_shared<int64_t>();
    auto new_value = std::make_shared<int64_t>();
    auto settings_change = std::make_shared<SettingsChange>();
    auto single_change = std::make_shared<SingleSettingAnyChange>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,query,new_value,old_value,settings_change,single_change] {
        VERIFY(m_profiles_db->increment_and_clamp_setting(
            query, new_value.get(), old_value.get(), settings_change.get(), single_change.get())
        );
        SUCCEED();
    }).then_or_error([guard=shared_from_this(),this,cb=std::move(cb),query,new_value,old_value,settings_change,single_change](Error err) {
        SetSettingReply<int64_t> reply{};
        reply.err = err;
        reply.new_value = *new_value;
        reply.default_value = query.default_value;
        if (old_value) {
            reply.old_value = *old_value;
        }
        cb(std::move(reply));
        bool success = !reply.err;
        if (success) {
            notify_settings_change_internal(*settings_change, *single_change);
        }
    });
}

void
CoreApi::append_to_string_setting(
    const AppendToStringSettingQuery& query, Cb<SetSettingReply<std::string>> cb)
{
    auto old_value = std::make_shared<std::string>();
    auto new_value = std::make_shared<std::string>();
    auto settings_change = std::make_shared<SettingsChange>();
    auto single_change = std::make_shared<SingleSettingAnyChange>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,query,new_value,old_value,settings_change,single_change] {
        VERIFY(m_profiles_db->append_to_string_setting(
            query, TxMode::use_tx, new_value.get(), old_value.get(), settings_change.get(), single_change.get())
        );
        SUCCEED();
    }).then_or_error([guard=shared_from_this(),this,cb=std::move(cb),query,new_value,old_value,settings_change,single_change](Error err) {
        SetSettingReply<std::string> reply{};
        reply.err = err;
        reply.new_value = *new_value;
        reply.default_value = query.default_value;
        if (old_value) {
            reply.old_value = *old_value;
        }
        cb(std::move(reply));
        bool success = !reply.err;
        if (success) {
            notify_settings_change_internal(*settings_change, *single_change);
        }
    });
}

void
CoreApi::remove_all_occurrences_of_string_from_setting(
    const RemoveAllOccurrencesOfStringFromSettingQuery& query, Cb<SetSettingReply<std::string>> cb)
{
    auto old_value = std::make_shared<std::string>();
    auto new_value = std::make_shared<std::string>();
    auto settings_change = std::make_shared<SettingsChange>();
    auto single_change = std::make_shared<SingleSettingAnyChange>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,query,new_value,old_value,settings_change,single_change] {
        VERIFY(m_profiles_db->remove_all_occurrences_of_string_from_setting(
            query, TxMode::use_tx, new_value.get(), old_value.get(), settings_change.get(), single_change.get())
        );
        SUCCEED();
    }).then_or_error([guard=shared_from_this(),this,cb=std::move(cb),query,new_value,old_value,settings_change,single_change](Error err) {
        SetSettingReply<std::string> reply{};
        reply.err = err;
        reply.new_value = *new_value;
        reply.default_value = query.default_value;
        if (old_value) {
            reply.old_value = *old_value;
        }
        cb(std::move(reply));
        bool success = !reply.err;
        if (success) {
            notify_settings_change_internal(*settings_change, *single_change);
        }
    });
}

void
CoreApi::get_all_values_for_website_setting(
    const GetAllValuesForWebsiteSettingQuery& query, Cb<GetAllValuesForWebsiteSettingReply> cb)
{
    auto reply = std::make_shared<GetAllValuesForWebsiteSettingReply>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,query,reply] {
        VERIFY(m_profiles_db->get_all_values_for_website_setting(query, reply.get()));
        SUCCEED();
    }).then_or_error([cb=std::move(cb),reply](Error err) {
        cb(std::move(*reply));
    });
}

void
CoreApi::get_setting_internal(const GetSettingQuery& query, Cb<GetSettingReply<std::string>> cb)
{
    auto value = std::make_shared<std::string>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,query,value] {
        VERIFY(m_profiles_db->get_setting(query, value.get()));
        SUCCEED();
    }).then_or_error([cb=std::move(cb),value](Error err) {
        GetSettingReply<std::string> reply{};
        reply.value = *value;
        reply.err = err;
        // reply.setting and reply.default_value are left empty here, as they will be filled in the parent lambda
        cb(std::move(reply));
    });
}

void
CoreApi::get_common_setting_internal(const GetCommonSettingQuery& query, Cb<GetSettingReply<std::string>> cb)
{
    auto value = std::make_shared<std::string>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,query,value] {
        VERIFY(m_profiles_db->get_common_setting(query, value.get()));
        SUCCEED();
    }).then_or_error([cb=std::move(cb),value](Error err) {
        GetSettingReply<std::string> reply{
            .value = *value,
            .err = err,
        };
        // reply.setting and reply.default_value are left empty here, as they will be filled in the parent lambda
        cb(std::move(reply));
    });
}

ObserverId
CoreApi::add_single_setting_change_observer_internal(const std::string& key, Cb<SingleSettingAnyChange> cb)
{
    orion::ObserverId observer_id = m_single_setting_change->add_observer(std::move(cb),
        [key](const SingleSettingAnyChange& change) {
            return change.key == key;
        });
    single_setting_observable_log("add_single_setting_change_observer_internal: observer_id=%zu key=%s",
        observer_id, key.c_str());
    return observer_id;
}

void
CoreApi::notify_settings_change_internal(
    const SettingsChange& settings_change,
    const SingleSettingAnyChange& single_change)
{
    if (!settings_change.updated.empty()) {
        m_settings_change->notify(settings_change);
    }
    if (single_change.profile_id != ProfileId{0}) {
        m_single_setting_change->notify(single_change);
    }
}

void
CoreApi::notify_common_settings_change_internal(
    const CommonSettingsChange& settings_change,
    const SingleCommonSettingAnyChange& single_change)
{
    if (!settings_change.updated.empty()) {
        m_common_settings_change->notify(settings_change);
    }
    if (!single_change.key.empty()) {
        m_single_common_setting_change->notify(single_change);
    }
}

std::string
CoreApi::get_website_setting_realm() const
{
    return "settings::website::specific";
}

std::string
CoreApi::get_website_setting_key(const std::string& website, const std::string& key_name) const
{
    const std::string realm = get_website_setting_realm();
    return std::format("{}::{}::{}", realm, website, key_name);
}

orion::GetSettingQuery
CoreApi::create_get_setting_query_for_website_bool(CreateGetSettingQueryForWebsiteBoolArg arg)
{
    orion::GetSettingQuery query{
        .profile_id = arg.profile_id,
        .key = get_website_setting_key(arg.website, arg.key_name),
        .realm = get_website_setting_realm(),
        .key_name = arg.key_name,
        .type = "boolean",
        .default_value = arg.default_value ? "1" : "0",
    };
    return query;
}

orion::SetSettingQuery
CoreApi::create_set_setting_query_for_website_bool(CreateSetSettingQueryForWebsiteBoolArg arg)
{
    const std::string realm = "settings::website::specific";
    orion::SetSettingQuery query{
        .profile_id = arg.profile_id,
        .key = std::format("{}::{}::{}", realm, arg.website, arg.key_name),
        .realm = realm,
        .key_name = arg.key_name,
        .type = "boolean",
        .value = arg.value ? "1" : "0",
        .default_value = arg.default_value ? "1" : "0",
    };
    return query;
}

orion::SetSettingQuery
CoreApi::create_set_setting_query_for_website_integer(CreateSetSettingQueryForWebsiteIntegerArg arg)
{
    const std::string realm = "settings::website::specific";
    orion::SetSettingQuery query{
        .profile_id = arg.profile_id,
        .key = std::format("{}::{}::{}", realm, arg.website, arg.key_name),
        .realm = realm,
        .key_name = arg.key_name,
        .type = "integer",
        .value = std::to_string(arg.value),
        .default_value = std::to_string(arg.default_value),
    };
    return query;
}

void
CoreApi::reset_website_settings(ResetWebsiteSettings query, Cb<Error> cb)
{
    auto change = std::make_shared<SettingsChange>();
    auto single_change = std::make_shared<SingleSettingAnyChange>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,query,change,single_change] {
        for (const auto& setting : settings::website::specific::all()) {
            DeleteSettingForWebsiteQuery delete_query{
                .profile_id = query.profile_id,
                .website = query.website,
                .key_name = setting->key_name(),
            };
            VERIFY(m_profiles_db->delete_setting_for_website(delete_query, change.get(), single_change.get()));
        }
        SUCCEED();
    }).then_or_error([guard=shared_from_this(),this,cb=std::move(cb),change,single_change](Error err) {
        cb(std::move(err));
        notify_settings_change_internal(*change, *single_change);
    });
}

//-------------------------------------------------------------------------
// Extensions
//-------------------------------------------------------------------------

bridge::HostAttachment
CoreApi::attach_profile_host(
    ProfileId profile_id, bridge::ProfileHost& host)
{
    return get_or_create_profile_bridge(profile_id)->attach(host);
}

std::weak_ptr<bridge::ProfileBridge>
CoreApi::get_profile_bridge(ProfileId profile_id) const
{
    auto it = m_webextensions.profile_bridges.find(profile_id);
    return it == m_webextensions.profile_bridges.end()
        ? std::weak_ptr<bridge::ProfileBridge>()
        : it->second;
}

std::weak_ptr<bridge::ProfileWorld>
CoreApi::get_profile_world(
    ProfileId profile_id, PrivateMode private_mode) const
{
    auto it = m_webextensions.profile_bridges.find(profile_id);
    return it == m_webextensions.profile_bridges.end()
        ? std::weak_ptr<bridge::ProfileWorld>()
        : it->second->world(private_mode);
}

std::shared_ptr<bridge::ProfileBridge>
CoreApi::get_or_create_profile_bridge(ProfileId profile_id)
{
    auto [it, inserted] = m_webextensions.profile_bridges.try_emplace(profile_id);
    if(inserted) {
        it->second = std::shared_ptr<bridge::ProfileBridge>(
            new bridge::ProfileBridge(*this, profile_id));
        it->second->initialize_worlds();
    }
    return it->second;
}

void
CoreApi::remove_profile_bridge(bridge::ProfileBridge& bridge)
{
    auto it = m_webextensions.profile_bridges.find(bridge.profile_id());
    if(it != m_webextensions.profile_bridges.end()
        && it->second.get() == &bridge) {
        m_webextensions.profile_bridges.erase(it);
    }
}

void
CoreApi::install_extension(InstallExtensionQuery query, Cb<InstallExtensionReply> cb)
{
    auto change = std::make_shared<ExtensionsChange>();
    auto id = std::make_shared<ExtensionId>();

    dispatch(on_main_thread(), [guard=shared_from_this(),this,query,id,change] {
        VERIFY(m_profiles_db->install_extension(query, id.get(), change.get()));
        SUCCEED();
    }).then_or_error([guard=shared_from_this(),this,cb=std::move(cb),id,change](Error err) {
        InstallExtensionReply reply{
            .err = err,
            .id = *id,
        };
        cb(std::move(reply));
        if (!change->created.empty() || !change->updated.empty() || !change->deleted.empty()) {
            m_extensions_change->notify(std::move(*change));
        }
    });
}

void
CoreApi::get_installed_extension_ids(GetInstalledExtensionIdsQuery query, Cb<GetAllInstalledExtensionIdsReply> cb)
{
    auto ids = std::make_shared<std::vector<ExtensionId>>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,ids,query] {
        VERIFY(m_profiles_db->get_installed_extension_ids(query, ids.get()));
        SUCCEED();
    }).then_or_error([cb=std::move(cb),ids](Error err) {
        GetAllInstalledExtensionIdsReply reply{
            .err = err,
            .ids = *ids,
        };
        cb(std::move(reply));
    });
}

void
CoreApi::get_installed_extensions(GetInstalledExtensionsQuery query, Cb<GetInstalledExtensionsReply> cb)
{
    auto extensions = std::make_shared<std::vector<ExtensionDto>>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,extensions,query] {
        VERIFY(m_profiles_db->get_installed_extensions(query, extensions.get()));
        SUCCEED();
    }).then_or_error([cb=std::move(cb),extensions](Error err) {
        GetInstalledExtensionsReply reply{
            .err = err,
            .extensions = *extensions,
        };
        cb(std::move(reply));
    });
}

void
CoreApi::get_installed_extensions_by_query(GetInstalledExtensionIdsQuery query, Cb<GetInstalledExtensionsReply> cb)
{
    auto extensions = std::make_shared<std::vector<ExtensionDto>>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,extensions,query] {
        std::vector<ExtensionId> ids;
        VERIFY(m_profiles_db->get_installed_extension_ids(query, &ids));
        VERIFY(m_profiles_db->get_installed_extensions({ids}, extensions.get()));
        SUCCEED();
    }).then_or_error([cb=std::move(cb),extensions](Error err) {
        cb(GetInstalledExtensionsReply{
            .err = err,
            .extensions = *extensions,
        });
    });
}

std::string
CoreApi::get_extension_fetch_path(const WebExtensions::ExtensionInfo& extension)
{
    std::filesystem::path path = get_extension_fetch_directory_path(m_app_id.c_str(), { orion::generate_uuid_v4(extension.installURL.value_or("")) });
    ensure_parent_directories(path);
    return path.string();
}

void
CoreApi::update_extension(UpdateExtensionQuery query, Cb<UpdateExtensionReply> cb)
{
    auto change = std::make_shared<ExtensionsChange>();

    dispatch(on_main_thread(), [guard=shared_from_this(),this,query,change] {
        VERIFY(m_profiles_db->update_extension(query, change.get()));
        SUCCEED();
    }).then_or_error([guard=shared_from_this(),this,cb=std::move(cb),change](Error err) {
        UpdateExtensionReply reply{
            .err = err,
        };
        cb(std::move(reply));
        if (!change->created.empty() || !change->updated.empty() || !change->deleted.empty()) {
            m_extensions_change->notify(std::move(*change));
        }
    });
}

void
CoreApi::uninstall_extension(UninstallExtensionQuery query, Cb<UninstallExtensionReply> cb)
{
    auto change = std::make_shared<ExtensionsChange>();

    dispatch(on_main_thread(), [guard=shared_from_this(),this,query,change] {
        VERIFY(m_profiles_db->uninstall_extension(query, change.get()));
        SUCCEED();
    }).then_or_error([guard=shared_from_this(),this,cb=std::move(cb),change](Error err) {
        UninstallExtensionReply reply{
            .err = err,
        };
        cb(std::move(reply));
        if (!change->created.empty() || !change->updated.empty() || !change->deleted.empty()) {
            m_extensions_change->notify(std::move(*change));
        }
    });
}

//-------------------------------------------------------------------------
// Snapshots
//-------------------------------------------------------------------------

void
CoreApi::create_snapshot(ProfileId profile_id, Cb<CreateSnapshotReply> cb)
{
    auto created_at = std::make_shared<UnixTimeUtc64>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,profile_id,created_at] {
        VERIFY(m_profiles_db->create_snapshot(profile_id, TxMode::use_tx, created_at.get()));
        SUCCEED();
    }).then_or_error([cb=std::move(cb),created_at](Error err) {
        CreateSnapshotReply reply{
            .err = err,
            .created_at = *created_at,
        };
        cb(reply);
    });
}

void
CoreApi::get_last_snapshot(ProfileId profile_id, Cb<GetLastSnapshotReply> cb)
{
    auto snapshot = std::make_shared<SnapshotDto>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,profile_id,snapshot] {
        VERIFY(m_profiles_db->get_last_snapshot(profile_id, snapshot.get()));
        SUCCEED();
    }).then_or_error([cb=std::move(cb),snapshot](Error err) {
        GetLastSnapshotReply reply{
            .err = err,
            .snapshot = *snapshot,
        };
        cb(std::move(reply));
    });
}

void
CoreApi::delete_snapshot(ProfileId profile_id, Cb<DeleteSnapshotReply> cb)
{
    dispatch(on_disk_thread(), [guard=shared_from_this(),this,profile_id] {
        VERIFY(m_profiles_db->delete_snapshot(profile_id));
        SUCCEED();
    }).then_or_error([cb=std::move(cb)](Error err) {
        cb(DeleteSnapshotReply{ .err = err });
    });
}

void
CoreApi::delete_setting_for_website(DeleteSettingForWebsiteQuery query, Cb<Error> cb)
{
    auto change = std::make_shared<SettingsChange>();
    auto single_change = std::make_shared<SingleSettingAnyChange>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,query,change,single_change] {
        VERIFY(m_profiles_db->delete_setting_for_website(query, change.get(), single_change.get()));
        SUCCEED();
    }).then_or_error([guard=shared_from_this(),this,cb=std::move(cb),change,single_change](Error err) {
        cb(std::move(err));
        notify_settings_change_internal(*change, *single_change);
    });
}

void
CoreApi::delete_settings(ProfileId profile_id, Cb<Error> cb)
{
    dispatch(on_disk_thread(), [guard=shared_from_this(),this,profile_id] {
        VERIFY(m_profiles_db->delete_settings(profile_id));
        SUCCEED();
    }).then_or_error([cb=std::move(cb)](Error err) {
        cb(err);
    });
}

void
CoreApi::restore_snapshot(RestoreSnapshotQuery query, Cb<RestoreSnapshotReply> cb)
{
    auto windows_change = std::make_shared<WindowsChange>();
    auto tabs_change = std::make_shared<TabsChange>();
    auto profiles_change = std::make_shared<ProfilesChange>();
    auto back_forward_entries_change = std::make_shared<TabBackForwardListEntriesChange>();

    dispatch(on_disk_thread(), [=,guard=shared_from_this(),this] {
        VERIFY(m_profiles_db->restore_snapshot(
            query, windows_change.get(), tabs_change.get(), profiles_change.get(),
            back_forward_entries_change.get()
        ));
        VERIFY(m_profiles_db->verify_windows_change(windows_change.get()));
        SUCCEED();
    }).then_or_error(
        [=,guard=shared_from_this(),this,cb=std::move(cb)]
        (Error err) {
            RestoreSnapshotReply reply{
                .err = err,
                .profile_id = query.profile_id,
                .created_at = query.created_at,
            };
            cb(reply);
            if (!windows_change->created.empty() || !windows_change->updated.empty() || !windows_change->deleted.empty()) {
                m_windows_change->notify(std::move(*windows_change));
            }
            if (!tabs_change->created.empty() || !tabs_change->updated.empty() || !tabs_change->deleted.empty()) {
                m_tabs_change->notify(std::move(*tabs_change));
            }
            if (!profiles_change->created.empty() || !profiles_change->updated.empty() || !profiles_change->deleted.empty()) {
                m_profiles_change->notify(std::move(*profiles_change));
            }
            if (!back_forward_entries_change->created.empty()
             || !back_forward_entries_change->updated.empty()
             || !back_forward_entries_change->deleted.empty()
            ) {
                m_tab_back_forward_list_entries_change->notify(std::move(*back_forward_entries_change));
            }
        });
}

//-------------------------------------------------------------------------
// Password Items
//-------------------------------------------------------------------------

void
CoreApi::create_password_item(CreatePasswordItemQuery query, Cb<CreatePasswordItemReply> cb)
{
    auto item_id = std::make_shared<PasswordItemId>(PasswordItemId(0));
    auto change = std::make_shared<PasswordItemsChange>();
    auto already_exists = std::make_shared<bool>(false);

    dispatch(on_disk_thread(), [=,guard=shared_from_this(),this] {
        VERIFY(m_profiles_db->create_password_item(
            query, item_id.get(), change.get(), already_exists.get()
        ));
        SUCCEED();
    }).then_or_error([=,guard=shared_from_this(),this](Error err) {
        CreatePasswordItemReply reply{
            .err = err,
            .id = *item_id,
            .already_exists = *already_exists,
        };
        cb(reply);
        if (!change->created.empty() || !change->updated.empty() || !change->deleted.empty()) {
            m_password_items_change->notify(*change);
        }
    });
}

void
CoreApi::create_never_save_password_item(CreateNeverSavePasswordItemQuery query, Cb<CreateNeverSavePasswordItemReply> cb)
{
    auto item_id = std::make_shared<PasswordItemId>(PasswordItemId(0));
    auto change = std::make_shared<PasswordItemsChange>();
    auto already_exists = std::make_shared<bool>(false);

    dispatch(on_disk_thread(), [=,guard=shared_from_this(),this] {
        CreatePasswordItemQuery create_query{
            .profile_id = query.profile_id,
            .url = query.url,
            .username = "Passwords not saved",
            .password = "",
        };
        VERIFY(m_profiles_db->create_password_item(
            create_query, item_id.get(), change.get(), already_exists.get()
        ));
        SUCCEED();
    }).then_or_error([=,guard=shared_from_this(),this](Error err) {
        CreateNeverSavePasswordItemReply reply{
            .err = err,
            .id = *item_id,
            .already_exists = *already_exists,
        };
        cb(reply);
        if (!change->created.empty() || !change->updated.empty() || !change->deleted.empty()) {
            m_password_items_change->notify(*change);
        }
    });
}

void
CoreApi::get_password_item_ids(GetPasswordItemIdsQuery query, Cb<GetPasswordItemIdsReply> cb)
{
    auto ids = std::make_shared<std::vector<PasswordItemId>>();

    dispatch(on_disk_thread(), [=,guard=shared_from_this(),this] {
        VERIFY(m_profiles_db->get_password_item_ids(query, ids.get()));
        SUCCEED();
    }).then_or_error([=,guard=shared_from_this(),this](Error err) {
        cb(GetPasswordItemIdsReply{
            .err = err,
            .ids = *ids,
        });
    });
}

void
CoreApi::get_password_items(const std::vector<PasswordItemId>& ids, Cb<GetPasswordItemsReply> cb)
{
    auto items = std::make_shared<std::vector<PasswordItemDto>>();

    dispatch(on_disk_thread(), [=,guard=shared_from_this(),this] {
        VERIFY(m_profiles_db->get_password_items(ids, items.get()));
        SUCCEED();
    }).then_or_error([=,guard=shared_from_this(),this](Error err) {
        cb(GetPasswordItemsReply{
            .err = err,
            .items = *items,
        });
    });
}

void
CoreApi::get_password_items_by_query(GetPasswordItemIdsQuery query, Cb<GetPasswordItemsReply> cb)
{
    auto items = std::make_shared<std::vector<PasswordItemDto>>();

    dispatch(on_disk_thread(), [=,guard=shared_from_this(),this] {
        std::vector<PasswordItemId> ids;
        VERIFY(m_profiles_db->get_password_item_ids(query, &ids));
        VERIFY(m_profiles_db->get_password_items(ids, items.get()));
        SUCCEED();
    }).then_or_error([=,guard=shared_from_this(),this](Error err) {
        cb(GetPasswordItemsReply{
            .err = err,
            .items = *items,
        });
    });
}

void
CoreApi::get_never_save_password_item(GetNeverSavePasswordItemQuery query, Cb<GetNeverSavePasswordItemReply> cb)
{
    auto item = std::make_shared<PasswordItemDto>();

    dispatch(on_disk_thread(), [=,guard=shared_from_this(),this] {
        GetPasswordItemIdsQuery get_query{
            .profile_id = query.profile_id,
            .authority = query.authority,
            .username = "Passwords not saved",
        };
        std::vector<PasswordItemId> ids;
        VERIFY(m_profiles_db->get_password_item_ids(get_query, &ids));
        std::vector<PasswordItemDto> items;
        VERIFY(m_profiles_db->get_password_items(ids, &items));
        RAISE_CHECK(items.size() <= 1); // sanity check
        if (!items.empty()) {
            *item = items.front();
        }
        SUCCEED();
    }).then_or_error([=,guard=shared_from_this(),this](Error err) {
        GetNeverSavePasswordItemReply reply{
            .err = err,
            .id = item->id,
            .never_save = item->id != PasswordItemId(0),
        };
        cb(reply);
    });
}

void
CoreApi::update_password_item(UpdatePasswordItemQuery query, Cb<Error> cb)
{
    auto change = std::make_shared<PasswordItemsChange>();

    dispatch(on_disk_thread(), [=,guard=shared_from_this(),this] {
        VERIFY(m_profiles_db->update_password_item(query, change.get()));
        SUCCEED();
    }).then_or_error([=,guard=shared_from_this(),this](Error err) {
        cb(err);
        if (!change->created.empty() || !change->updated.empty() || !change->deleted.empty()) {
            m_password_items_change->notify(*change);
        }
    });
}

void
CoreApi::delete_password_items(const std::vector<PasswordItemId>& ids, Cb<Error> cb)
{
    auto change = std::make_shared<PasswordItemsChange>();

    dispatch(on_disk_thread(), [=,guard=shared_from_this(),this] {
        VERIFY(m_profiles_db->delete_password_items(ids, change.get()));
        SUCCEED();
    }).then_or_error([=,guard=shared_from_this(),this](Error err) {
        cb(err);
        if (!change->created.empty() || !change->updated.empty() || !change->deleted.empty()) {
            m_password_items_change->notify(*change);
        }
    });
}

void
CoreApi::get_password_items_csv_export(const std::vector<PasswordItemId>& ids, Cb<GetPasswordItemsCsvExportReply> cb)
{
    auto csv_text = std::make_shared<std::string>();

    dispatch(on_disk_thread(), [=,guard=shared_from_this(),this] {
        VERIFY(m_profiles_db->get_password_items_csv_export(ids, csv_text.get()));
        SUCCEED();
    }).then_or_error([=,guard=shared_from_this(),this](Error err) {
        GetPasswordItemsCsvExportReply reply{
            .err = err,
            .csv_text = *csv_text,
        };
        cb(reply);
    });
}

void
CoreApi::import_password_items_from_csv(ImportPasswordItemsFromCsvQuery query, Cb<ImportPasswordItemsFromCsvReply> cb)
{
    auto imported = std::make_shared<std::vector<PasswordItemDto>>();
    auto not_imported = std::make_shared<std::vector<PasswordItemDto>>();
    auto change = std::make_shared<PasswordItemsChange>();

    dispatch(on_disk_thread(), [=,guard=shared_from_this(),this] {
        VERIFY(m_profiles_db->import_password_items_from_csv(query, imported.get(), not_imported.get(), change.get()));
        SUCCEED();
    }).then_or_error([=,guard=shared_from_this(),this](Error err) {
        ImportPasswordItemsFromCsvReply reply{
            .err = err,
            .imported = *imported,
            .not_imported = *not_imported,
        };
        cb(reply);
        if (!change->created.empty() || !change->updated.empty() || !change->deleted.empty()) {
            m_password_items_change->notify(*change);
        }
    });
}

Error
CoreApi::generate_totp_verification_code(GenerateTotpVerificationCodeQuery query, std::string* out_code)
{
    VERIFY(m_profiles_db->generate_totp_verification_code(query, out_code));
    SUCCEED();
}

//-------------------------------------------------------------------------
// Extension Storage Areas
//-------------------------------------------------------------------------

void
CoreApi::extension_storage_areas_get(ExtensionStorageAreas::GetQuery query, Cb<ExtensionStorageAreas::GetReply> cb)
{
    auto kvs = std::make_shared<std::vector<ExtensionStorageAreas::KeyValue>>();

    dispatch(on_disk_thread(), [=,guard=shared_from_this(),this] {
        VERIFY(m_profiles_db->extension_storage_areas_get(query, kvs.get()));
        SUCCEED();
    }).then_or_error([=,cb=std::move(cb)](Error err) {
        ExtensionStorageAreas::GetReply reply{
            .err = err,
            .key_values = *kvs,
        };
        cb(reply);
    });
}

void
CoreApi::extension_storage_areas_get_all(
    ExtensionStorageAreas::GetAllQuery query, Cb<ExtensionStorageAreas::GetAllReply> cb)
{
    auto kvs = std::make_shared<std::vector<ExtensionStorageAreas::KeyValue>>();

    dispatch(on_disk_thread(), [=,guard=shared_from_this(),this] {
        VERIFY(m_profiles_db->extension_storage_areas_get_all(query, kvs.get()));
        SUCCEED();
    }).then_or_error([=,cb=std::move(cb)](Error err) {
        ExtensionStorageAreas::GetAllReply reply{
            .err = err,
            .key_values = *kvs,
        };
        cb(reply);
    });
}

void
CoreApi::extension_storage_areas_get_keys(
    ExtensionStorageAreas::GetKeysQuery query, Cb<ExtensionStorageAreas::GetKeysReply> cb)
{
    auto keys = std::make_shared<std::vector<std::string>>();

    dispatch(on_disk_thread(), [=,guard=shared_from_this(),this] {
        VERIFY(m_profiles_db->extension_storage_areas_get_keys(query, keys.get()));
        SUCCEED();
    }).then_or_error([=,cb=std::move(cb)](Error err) {
        ExtensionStorageAreas::GetKeysReply reply{
            .err = err,
            .keys = *keys,
        };
        cb(reply);
    });
}

void
CoreApi::extension_storage_areas_get_bytes_in_use(
    ExtensionStorageAreas::GetBytesInUseQuery query,
    Cb<ExtensionStorageAreas::GetBytesInUseReply> cb)
{
    auto bytes_in_use = std::make_shared<uint64_t>();
    dispatch(on_disk_thread(), [=,guard=shared_from_this(),this] {
        VERIFY(m_profiles_db->extension_storage_areas_get_bytes_in_use(
            query, bytes_in_use.get()));
        SUCCEED();
    }).then_or_error([=,cb=std::move(cb)](Error err) {
        cb(ExtensionStorageAreas::GetBytesInUseReply{
            .err = err,
            .bytes_in_use = *bytes_in_use,
        });
    });
}

void
CoreApi::extension_storage_areas_set(
    ExtensionStorageAreas::SetQuery query,
    Cb<ExtensionStorageAreas::MutationReply> cb)
{
    auto changes = std::make_shared<std::vector<ExtensionStorageAreas::Change>>();
    dispatch(on_disk_thread(), [=,guard=shared_from_this(),this] {
        VERIFY(m_profiles_db->extension_storage_areas_set(query, changes.get()));
        SUCCEED();
    }).then_or_error([=,cb=std::move(cb)](Error err) {
        cb(ExtensionStorageAreas::MutationReply{
            .err = err,
            .changes = std::move(*changes),
        });
    });
}

void
CoreApi::extension_storage_areas_remove(
    ExtensionStorageAreas::RemoveQuery query,
    Cb<ExtensionStorageAreas::MutationReply> cb)
{
    auto changes = std::make_shared<std::vector<ExtensionStorageAreas::Change>>();
    dispatch(on_disk_thread(), [=,guard=shared_from_this(),this] {
        VERIFY(m_profiles_db->extension_storage_areas_remove(query, changes.get()));
        SUCCEED();
    }).then_or_error([=,cb=std::move(cb)](Error err) {
        cb(ExtensionStorageAreas::MutationReply{
            .err = err,
            .changes = std::move(*changes),
        });
    });
}

void
CoreApi::extension_storage_areas_clear(
    ExtensionStorageAreas::ClearQuery query,
    Cb<ExtensionStorageAreas::MutationReply> cb)
{
    auto changes = std::make_shared<std::vector<ExtensionStorageAreas::Change>>();
    dispatch(on_disk_thread(), [=,guard=shared_from_this(),this] {
        VERIFY(m_profiles_db->extension_storage_areas_clear(query, changes.get()));
        SUCCEED();
    }).then_or_error([=,cb=std::move(cb)](Error err) {
        cb(ExtensionStorageAreas::MutationReply{
            .err = err,
            .changes = std::move(*changes),
        });
    });
}

void
CoreApi::extension_session_storage_clear(ExtensionId extension_id, Cb<Error> cb)
{
    dispatch(on_disk_thread(), [=,guard=shared_from_this(),this] {
        VERIFY(m_profiles_db->extension_session_storage_clear(extension_id));
        SUCCEED();
    }).then_or_error([cb=std::move(cb)](Error err) {
        cb(err);
    });
}

void
CoreApi::extension_storage_access_level_get(
    ExtensionStorageAccessLevels::GetQuery query,
    Cb<ExtensionStorageAccessLevels::GetReply> cb)
{
    auto allow_untrusted_contexts = std::make_shared<bool>();
    dispatch(on_disk_thread(), [=,guard=shared_from_this(),this] {
        VERIFY(m_profiles_db->extension_storage_access_level_get(
            query, allow_untrusted_contexts.get()));
        SUCCEED();
    }).then_or_error([=,cb=std::move(cb)](Error err) {
        cb(ExtensionStorageAccessLevels::GetReply{
            .err = err,
            .allow_untrusted_contexts = *allow_untrusted_contexts,
        });
    });
}

void
CoreApi::extension_storage_access_level_set(
    ExtensionStorageAccessLevels::SetQuery query,
    Cb<Error> cb)
{
    dispatch(on_disk_thread(), [=,guard=shared_from_this(),this] {
        VERIFY(m_profiles_db->extension_storage_access_level_set(query));
        SUCCEED();
    }).then_or_error([=,cb=std::move(cb)](Error err) {
        cb(err);
    });
}

//-------------------------------------------------------------------------
// Downloads
//-------------------------------------------------------------------------

void
CoreApi::create_download(CreateDownloadQuery query, Cb<CreateDownloadReply> cb)
{
    auto download_id = std::make_shared<DownloadId>(DownloadId(0));
    auto change = std::make_shared<DownloadsChange>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,query,download_id,change] {
        VERIFY(m_profiles_db->create_download(query, download_id.get(), change.get()));
        SUCCEED();
    }).then_or_error([guard=shared_from_this(),this,cb=std::move(cb),download_id,change](Error err) {
        CreateDownloadReply reply{
            .err = err,
            .id = *download_id,
        };
        cb(std::move(reply));
        if (!change->created.empty() || !change->updated.empty() || !change->deleted.empty()) {
            m_downloads_change->notify(std::move(*change));
        }
    });
}

void
CoreApi::update_downloads(const std::vector<UpdateDownloadQuery>& queries, Cb<Error> cb)
{
    if (queries.empty()) {
        log_warning("CoreApi::update_downloads: called with empty queries vector");
    }

    auto change = std::make_shared<DownloadsChange>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,queries,change] {
        std::vector<DownloadId> download_ids;
        for (const auto& query : queries) {
            download_ids.push_back(query.id);
        }

        std::vector<DownloadDto> downloads;
        VERIFY(m_profiles_db->get_downloads(download_ids, &downloads));

        std::map<DownloadId, ProfileId> download_to_profile_map;
        for (const auto& download : downloads) {
            download_to_profile_map[download.id] = download.profile_id;
        }

        std::map<ProfileId, settings::RemoveDownloadItemsMode> profile_to_remove_mode_map;
        for (const auto& [download_id, profile_id] : download_to_profile_map) {
            std::optional<RawSettingDto> setting{};
            VERIFY(m_profiles_db->get_setting_by_key(
                profile_id, settings::general::remove_download_items_mode.key(), &setting));
            settings::RemoveDownloadItemsMode remove_mode = settings::RemoveDownloadItemsMode::not_set;
            if (setting.has_value()) {
                int64_t int_val = 0;
                VERIFY(settings::unmarshal_value(setting->value, &int_val));
                remove_mode = static_cast<settings::RemoveDownloadItemsMode>(int_val);
            }
            profile_to_remove_mode_map[profile_id] = remove_mode;
        }

        for (const auto& query : queries) {
            if (query.update_operation_state && query.operation_state == DownloadOperationState::finished) {
                auto profile_iter = download_to_profile_map.find(query.id);
                if (profile_iter != download_to_profile_map.end()) {
                    auto profile_id = profile_iter->second;
                    auto remove_mode_iter = profile_to_remove_mode_map.find(profile_id);
                    if (remove_mode_iter != profile_to_remove_mode_map.end()) {
                        auto remove_mode = remove_mode_iter->second;
                        if (remove_mode == settings::RemoveDownloadItemsMode::upon_successful_download) {
                            // Delete the download instead of updating it after successful download
                            DeleteDownloadsQuery delete_query{
                                .ids = {query.id},
                            };
                            VERIFY(m_profiles_db->delete_downloads(delete_query, TxMode::use_tx, change.get()));
                            continue;
                        }
                    }
                }
            }
            VERIFY(m_profiles_db->update_download(query, change.get()));
        }
        SUCCEED();
    }).then_or_error([guard=shared_from_this(),this,cb=std::move(cb),change](Error err) {
        cb(std::move(err));
        if (!change->created.empty() || !change->updated.empty() || !change->deleted.empty()) {
            m_downloads_change->notify(std::move(*change));
        }
    });
}

void
CoreApi::get_download_ids(GetDownloadIdsQuery query, Cb<GetDownloadIdsReply> cb)
{
    auto ids = std::make_shared<std::vector<DownloadId>>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,ids,query = std::move(query)] {
        VERIFY(m_profiles_db->get_download_ids(query, ids.get()));
        SUCCEED();
    }).then_or_error([cb=std::move(cb),ids](Error err) {
        GetDownloadIdsReply reply{
            .ids = *ids,
            .err = err,
        };
        cb(std::move(reply));
    });
}

void
CoreApi::get_downloads(const std::vector<DownloadId>& ids, Cb<GetDownloadsReply> cb)
{
    auto downloads = std::make_shared<std::vector<DownloadDto>>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,ids,downloads] {
        VERIFY(m_profiles_db->get_downloads(ids, downloads.get()));
        SUCCEED();
    }).then_or_error([cb=std::move(cb),downloads](Error err) {
        GetDownloadsReply reply{
            .downloads = *downloads,
            .err = err,
        };
        cb(std::move(reply));
    });
}

void
CoreApi::get_downloads_by_query(GetDownloadIdsQuery query, Cb<GetDownloadsReply> cb)
{
    auto downloads = std::make_shared<std::vector<DownloadDto>>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,query,downloads] {
        std::vector<DownloadId> ids;
        VERIFY(m_profiles_db->get_download_ids(query, &ids));
        VERIFY(m_profiles_db->get_downloads(ids, downloads.get()));
        SUCCEED();
    }).then_or_error([cb=std::move(cb),downloads](Error err) {
        GetDownloadsReply reply{
            .downloads = *downloads,
            .err = err,
        };
        cb(std::move(reply));
    });
}


void
CoreApi::delete_downloads(const DeleteDownloadsQuery& query, Cb<DeleteDownloadsReply> cb)
{
    if (query.ids.empty() && query.profile_id == ProfileId{0}) {
        log_warning("CoreApi::delete_downloads: called with empty ids vector and profile_id==0");
    }

    auto change = std::make_shared<DownloadsChange>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,query,change] {
        VERIFY(m_profiles_db->delete_downloads(query, TxMode::use_tx, change.get()));
        SUCCEED();
    }).then_or_error([guard=shared_from_this(),this,cb=std::move(cb),change](Error err) {
        DeleteDownloadsReply reply{
            .err = err,
            .num_deleted = static_cast<int64_t>(change->deleted.size()),
        };
        cb(std::move(reply));
        if (!change->created.empty() || !change->updated.empty() || !change->deleted.empty()) {
            m_downloads_change->notify(std::move(*change));
        }
    });
}

//-------------------------------------------------------------------------
// Search Engines
//-------------------------------------------------------------------------

void
CoreApi::get_search_engine_ids(GetSearchEngineIdsQuery query, Cb<GetSearchEngineIdsReply> cb)
{
    auto ids = std::make_shared<std::vector<SearchEngineId>>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,ids,query] {
        VERIFY(m_profiles_db->get_search_engine_ids(query, ids.get()));
        SUCCEED();
    }).then_or_error([cb=std::move(cb),ids](Error err) {
        GetSearchEngineIdsReply reply{
            .ids = *ids,
            .err = err,
        };
        cb(std::move(reply));
    });
}

void
CoreApi::get_search_engines(const std::vector<SearchEngineId>& ids, Cb<GetSearchEnginesReply> cb)
{
    auto engines = std::make_shared<std::vector<SearchEngineDto>>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,ids,engines] {
        VERIFY(m_profiles_db->get_search_engines(ids, engines.get()));
        SUCCEED();
    }).then_or_error([cb=std::move(cb),engines](Error err) {
        GetSearchEnginesReply reply{
            .search_engines = *engines,
            .err = err,
        };
        cb(std::move(reply));
    });
}

void
CoreApi::get_search_engines_by_query(GetSearchEngineIdsQuery query, Cb<GetSearchEnginesReply> cb)
{
    auto engines = std::make_shared<std::vector<SearchEngineDto>>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,query,engines] {
        std::vector<SearchEngineId> ids;
        VERIFY(m_profiles_db->get_search_engine_ids(query, &ids));
        VERIFY(m_profiles_db->get_search_engines(ids, engines.get()));
        SUCCEED();
    }).then_or_error([cb,engines](Error err) {
        cb(GetSearchEnginesReply{
            .search_engines = *engines,
            .err = err,
        });
    });
}

void
CoreApi::create_search_engine(CreateSearchEngineQuery query, Cb<CreateSearchEngineReply> cb)
{
    auto id = std::make_shared<SearchEngineId>(SearchEngineId(0));
    auto change = std::make_shared<SearchEnginesChange>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,query,id,change] {
        VERIFY(m_profiles_db->create_search_engine(query, id.get(), change.get()));
        SUCCEED();
    }).then_or_error([guard=shared_from_this(),this,cb=std::move(cb),id,change](Error err) {
        CreateSearchEngineReply reply{
            .id = *id,
            .err = err,
        };
        cb(std::move(reply));
        if (!change->created.empty() || !change->updated.empty() || !change->deleted.empty()) {
            m_search_engines_change->notify(std::move(*change));
        }
    });
}

void
CoreApi::update_search_engine(UpdateSearchEngineQuery query, Cb<Error> cb)
{
    auto change = std::make_shared<SearchEnginesChange>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,query,change] {
        VERIFY(m_profiles_db->update_search_engine(query, change.get()));
        SUCCEED();
    }).then_or_error([guard=shared_from_this(),this,cb=std::move(cb),change](Error err) {
        cb(std::move(err));
        if (!change->created.empty() || !change->updated.empty() || !change->deleted.empty()) {
            m_search_engines_change->notify(std::move(*change));
        }
    });
}

void
CoreApi::delete_search_engines(const std::vector<SearchEngineId>& ids, Cb<Error> cb)
{
    auto change = std::make_shared<SearchEnginesChange>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,ids,change] {
        VERIFY(m_profiles_db->delete_search_engines(ids, TxMode::no_tx, change.get()));
        SUCCEED();
    }).then_or_error([guard=shared_from_this(),this,cb=std::move(cb),change](Error err) {
        cb(std::move(err));
        if (!change->created.empty() || !change->updated.empty() || !change->deleted.empty()) {
            m_search_engines_change->notify(std::move(*change));
        }
    });
}

void
CoreApi::match_search_engines(MatchSearchEnginesQuery query, Cb<MatchSearchEnginesReply> cb)
{
    auto matches = std::make_shared<std::vector<SearchEngineMatch>>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,query,matches]{
        VERIFY(m_profiles_db->match_search_engines(query, matches.get()));
        SUCCEED();
    }).then_or_error([cb=std::move(cb),matches](Error err) {
        MatchSearchEnginesReply reply{
            .err = err,
            .matches = *matches,
        };
        cb(std::move(reply));
    });
}

//-------------------------------------------------------------------------
// Filter Lists
//-------------------------------------------------------------------------

void
CoreApi::create_or_update_filter_list(CreateFilterListQuery query, Cb<CreateFilterListReply> cb)
{
    auto id = std::make_shared<FilterListId>(FilterListId(0));
    auto change = std::make_shared<FilterListsChange>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,query,id,change] {
        VERIFY(m_profiles_db->create_or_update_filter_list(query, id.get(), change.get()));
        SUCCEED();
    }).then_or_error([guard=shared_from_this(),this,cb=std::move(cb),id,change](Error err) {
        CreateFilterListReply reply{
            .id = *id,
            .err = err,
        };
        cb(std::move(reply));
        if (!change->created.empty() || !change->updated.empty() || !change->deleted.empty()) {
            m_filter_lists_change->notify(std::move(*change));
        }
    });
}

//-------------------------------------------------------------------------
// Favicons
//-------------------------------------------------------------------------

void
CoreApi::get_favicon(const std::string& page_url, Cb<GetFaviconReply> cb)
{
    auto favicon = std::make_shared<FaviconDto>();
    auto found = std::make_shared<bool>(false);
    auto expired = std::make_shared<bool>(false);

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,page_url,favicon,found,expired] {
        VERIFY(m_profiles_db->get_favicon_for_page_url(page_url, favicon.get(), found.get(), expired.get()));
        SUCCEED();
    }).then_or_error([cb=std::move(cb),favicon,found,expired](Error err) {
        GetFaviconReply reply{
            .icon_url = favicon->icon_url,
            .data = favicon->data,
            .found = *found,
            .expired = *expired,
            .err = err,
        };
        cb(std::move(reply));
    });
}

void
CoreApi::get_filter_lists(GetFilterListsQuery query, Cb<GetFilterListsReply> cb)
{
    auto filter_lists = std::make_shared<std::vector<FilterListDto>>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,query,filter_lists] {
        VERIFY(m_profiles_db->get_filter_lists(query, filter_lists.get()));
        SUCCEED();
    }).then_or_error([cb=std::move(cb),filter_lists](Error err) {
        GetFilterListsReply reply{
            .filter_lists = *filter_lists,
            .err = err,
        };
        cb(std::move(reply));
    });
}

void
CoreApi::delete_filter_lists(const std::vector<FilterListId>& ids, Cb<Error> cb)
{
    auto change = std::make_shared<FilterListsChange>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,ids,change] {
        VERIFY(m_profiles_db->delete_filter_lists(ids, change.get()));
        SUCCEED();
    }).then_or_error([guard=shared_from_this(),this,cb=std::move(cb),change](Error err) {
        cb(std::move(err));
        if (!change->created.empty() || !change->updated.empty() || !change->deleted.empty()) {
            m_filter_lists_change->notify(std::move(*change));
        }
    });
}

void
CoreApi::set_favicon(SetFaviconQuery query, Cb<Error> cb)
{
    dispatch(on_disk_thread(), [guard=shared_from_this(),this,query] {
        VERIFY(m_profiles_db->set_favicon_for_page_url(query));
        SUCCEED();
    }).then_or_error([cb=std::move(cb)](Error err) {
        cb(std::move(err));
    });
}

void
CoreApi::parse_filter_list(LoadFilterListQuery query, Cb<LoadFilterListReply> cb)
{
    auto items = std::make_shared<std::vector<FilterListItem>>();
    auto parsed_rule_count = std::make_shared<int32_t>(0);
    auto version = std::make_shared<std::string>();

    dispatch(on_cpu_thread(), [guard=shared_from_this(),this,query,items,parsed_rule_count,version] {
        VERIFY(orion::filter_list_parse(
            query.filter_list.get(), query.raw_data.c_str(), query.stop_token, items.get(), parsed_rule_count.get(),
            version.get()
        ));
        SUCCEED();
    }).then_or_error([cb=std::move(cb),items,parsed_rule_count,version](Error err) {
        LoadFilterListReply reply{
            .err = err,
            .items = *items,
            .parsed_rule_count = *parsed_rule_count,
            .version = *version,
        };
        cb(std::move(reply));
    });
}

//-------------------------------------------------------------------------
// Overflow Menu Items
//-------------------------------------------------------------------------

void
CoreApi::get_overflow_menu_items(GetOverflowMenuItemsQuery query, Cb<GetOverflowMenuItemsReply> cb)
{
    auto items = std::make_shared<std::vector<OverflowMenuItemDto>>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,query,items] {
        VERIFY(m_profiles_db->get_overflow_menu_items(query, items.get()));
        SUCCEED();
    }).then_or_error([cb=std::move(cb),items](Error err) {
        GetOverflowMenuItemsReply reply{
            .items = *items,
            .err = err,
        };
        cb(std::move(reply));
    });
}

void
CoreApi::create_overflow_menu_item(CreateOverflowMenuItemQuery query, Cb<CreateOverflowMenuItemReply> cb)
{
    auto id = std::make_shared<OverflowMenuItemId>(OverflowMenuItemId(0));
    auto change = std::make_shared<OverflowMenuItemsChange>();

    dispatch(on_disk_thread(), [=,guard=shared_from_this(),this] {
        VERIFY(m_profiles_db->create_overflow_menu_item(query, TxMode::use_tx, id.get(), change.get()));
        SUCCEED();
    }).then_or_error([=,guard=shared_from_this(),this,cb=std::move(cb)](Error err) {
        CreateOverflowMenuItemReply reply{
            .id = *id,
            .err = err,
        };
        cb(std::move(reply));
        if (!change->created.empty() || !change->updated.empty() || !change->deleted.empty()) {
            m_overflow_menu_items_change->notify(std::move(*change));
        }
    });
}

void
CoreApi::move_overflow_menu_item(MoveOverflowMenuItemQuery query, Cb<Error> cb)
{
    auto change = std::make_shared<OverflowMenuItemsChange>();

    dispatch(on_disk_thread(), [=,guard=shared_from_this(),this] {
        VERIFY(m_profiles_db->move_overflow_menu_item(query, TxMode::use_tx, change.get()));
        SUCCEED();
    }).then_or_error([=,guard=shared_from_this(),this,cb=std::move(cb)](Error err) {
        cb(std::move(err));
        if (!change->created.empty() || !change->updated.empty() || !change->deleted.empty()) {
            m_overflow_menu_items_change->notify(std::move(*change));
        }
    });
}

void
CoreApi::delete_overflow_menu_items(const std::vector<OverflowMenuItemId>& ids, Cb<Error> cb)
{
    auto change = std::make_shared<OverflowMenuItemsChange>();

    dispatch(on_disk_thread(), [=,guard=shared_from_this(),this] {
        VERIFY(m_profiles_db->delete_overflow_menu_items(ids, TxMode::use_tx, change.get()));
        SUCCEED();
    }).then_or_error([=,guard=shared_from_this(),this,cb=std::move(cb)](Error err) {
        cb(std::move(err));
        if (!change->created.empty() || !change->updated.empty() || !change->deleted.empty()) {
            m_overflow_menu_items_change->notify(std::move(*change));
        }
    });
}

//-------------------------------------------------------------------------
// Toolbar Items
//-------------------------------------------------------------------------

void
CoreApi::get_toolbar_items(GetToolbarItemsQuery query, Cb<GetToolbarItemsReply> cb)
{
    auto items = std::make_shared<std::vector<ToolbarItemDto>>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,query,items] {
        VERIFY(m_profiles_db->get_toolbar_items(query, items.get()));
        SUCCEED();
    }).then_or_error([cb=std::move(cb),items](Error err) {
        GetToolbarItemsReply reply{
            .items = *items,
            .err = err,
        };
        cb(std::move(reply));
    });
}

void
CoreApi::get_default_toolbar_items(GetDefaultToolbarItemsQuery query, Cb<GetDefaultToolbarItemsReply> cb)
{
    auto items = std::make_shared<std::vector<ToolbarItemDto>>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,query,items] {
        VERIFY(m_profiles_db->get_default_toolbar_items(query, items.get()));
        SUCCEED();
    }).then_or_error([cb=std::move(cb),items](Error err) {
        GetDefaultToolbarItemsReply reply{
            .items = *items,
            .err = err,
        };
        cb(std::move(reply));
    });
}

void
CoreApi::get_available_toolbar_items(GetAvailableToolbarItemsQuery query, Cb<GetAvailableToolbarItemsReply> cb)
{
    auto items = std::make_shared<std::vector<ToolbarItemDto>>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,query,items] {
        VERIFY(m_profiles_db->get_available_toolbar_items(query, items.get()));
        SUCCEED();
    }).then_or_error([cb=std::move(cb),items](Error err) {
        GetAvailableToolbarItemsReply reply{
            .items = *items,
            .err = err,
        };
        cb(std::move(reply));
    });
}

void
CoreApi::create_toolbar_item(CreateToolbarItemQuery query, Cb<CreateToolbarItemReply> cb)
{
    auto id = std::make_shared<ToolbarItemId>(ToolbarItemId(0));
    auto change = std::make_shared<ToolbarItemsChange>();
    auto programmable_buttons_change = std::make_shared<ProgrammableButtonsChange>();

    dispatch(on_disk_thread(), [=,guard=shared_from_this(),this] {
        VERIFY(m_profiles_db->create_toolbar_item(query, TxMode::use_tx, id.get(), change.get(), programmable_buttons_change.get()));
        SUCCEED();
    }).then_or_error([=,guard=shared_from_this(),this,cb=std::move(cb)](Error err) {
        CreateToolbarItemReply reply{
            .id = *id,
            .err = err,
        };
        cb(std::move(reply));
        if (!change->created.empty() || !change->updated.empty() || !change->deleted.empty()) {
            m_toolbar_items_change->notify(std::move(*change));
        }
        if (!programmable_buttons_change->created.empty() || !programmable_buttons_change->updated.empty() || !programmable_buttons_change->deleted.empty()) {
            m_programmable_buttons_change->notify(std::move(*programmable_buttons_change));
        }
    });
}

void
CoreApi::move_toolbar_item(MoveToolbarItemQuery query, Cb<Error> cb)
{
    auto change = std::make_shared<ToolbarItemsChange>();

    dispatch(on_disk_thread(), [=,guard=shared_from_this(),this] {
        VERIFY(m_profiles_db->move_toolbar_item(query, TxMode::use_tx, change.get()));
        SUCCEED();
    }).then_or_error([=,guard=shared_from_this(),this,cb=std::move(cb)](Error err) {
        cb(std::move(err));
        if (!change->created.empty() || !change->updated.empty() || !change->deleted.empty()) {
            m_toolbar_items_change->notify(std::move(*change));
        }
    });
}

void
CoreApi::update_toolbar_item(UpdateToolbarItemQuery query, Cb<Error> cb)
{
    auto change = std::make_shared<ToolbarItemsChange>();
    auto programmable_buttons_change = std::make_shared<ProgrammableButtonsChange>();

    dispatch(on_disk_thread(), [=,guard=shared_from_this(),this] {
        VERIFY(m_profiles_db->update_toolbar_item(
            query, TxMode::use_tx, change.get(), programmable_buttons_change.get()));
        SUCCEED();
    }).then_or_error([=,guard=shared_from_this(),this,cb=std::move(cb)](Error err) {
        cb(std::move(err));
        if (!change->created.empty() || !change->updated.empty() || !change->deleted.empty()) {
            m_toolbar_items_change->notify(std::move(*change));
        }
        if (!programmable_buttons_change->created.empty()
         || !programmable_buttons_change->updated.empty()
         || !programmable_buttons_change->deleted.empty()) {
            m_programmable_buttons_change->notify(std::move(*programmable_buttons_change));
        }
    });
}

void
CoreApi::delete_toolbar_items(const std::vector<ToolbarItemId>& ids, Cb<Error> cb)
{
    auto change = std::make_shared<ToolbarItemsChange>();
    auto programmable_buttons_change = std::make_shared<ProgrammableButtonsChange>();

    dispatch(on_disk_thread(), [=,guard=shared_from_this(),this] {
        VERIFY(m_profiles_db->delete_toolbar_items(ids, TxMode::use_tx, change.get(), programmable_buttons_change.get()));
        SUCCEED();
    }).then_or_error([=,guard=shared_from_this(),this,cb=std::move(cb)](Error err) {
        cb(std::move(err));
        if (!change->created.empty() || !change->updated.empty() || !change->deleted.empty()) {
            m_toolbar_items_change->notify(std::move(*change));
        }
        if (!programmable_buttons_change->created.empty() || !programmable_buttons_change->updated.empty() || !programmable_buttons_change->deleted.empty()) {
            m_programmable_buttons_change->notify(std::move(*programmable_buttons_change));
        }
    });
}

void
CoreApi::reset_toolbar_items(ResetToolbarItemsQuery query, Cb<Error> cb)
{
    auto change = std::make_shared<ToolbarItemsChange>();
    auto prog_buttons_change = std::make_shared<ProgrammableButtonsChange>();

    dispatch(on_disk_thread(), [=,guard=shared_from_this(),this] {
        VERIFY(m_profiles_db->reset_toolbar_items(query, TxMode::use_tx, change.get(), prog_buttons_change.get()));
        SUCCEED();
    }).then_or_error([=,guard=shared_from_this(),this,cb=std::move(cb)](Error err) {
        cb(std::move(err));
        if (!change->created.empty() || !change->updated.empty() || !change->deleted.empty()) {
            m_toolbar_items_change->notify(std::move(*change));
        }
        if (!prog_buttons_change->created.empty() || !prog_buttons_change->updated.empty() || !prog_buttons_change->deleted.empty()) {
            m_programmable_buttons_change->notify(std::move(*prog_buttons_change));
        }
    });
}

//-------------------------------------------------------------------------
// Programmable Buttons
//-------------------------------------------------------------------------

void
CoreApi::get_programmable_buttons(GetProgrammableButtonsQuery query, Cb<GetProgrammableButtonsReply> cb)
{
    auto items = std::make_shared<std::vector<ProgrammableButtonDto>>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,query,items] {
        VERIFY(m_profiles_db->get_programmable_buttons(query, items.get()));
        SUCCEED();
    }).then_or_error([cb=std::move(cb),items](Error err) {
        GetProgrammableButtonsReply reply{
            .items = *items,
            .err = err,
        };
        cb(std::move(reply));
    });
}

void
CoreApi::create_programmable_button(CreateProgrammableButtonQuery query, Cb<Error> cb)
{
    auto toolbar_items_change = std::make_shared<ToolbarItemsChange>();
    auto prog_buttons_change = std::make_shared<ProgrammableButtonsChange>();

    dispatch(on_disk_thread(), [guard=shared_from_this(),this,query,toolbar_items_change,prog_buttons_change] {
        DbTx tx{};
        VERIFY(m_profiles_db->begin(&tx));

        std::optional<RawSettingDto> setting{};
        VERIFY(m_profiles_db->get_setting_by_key(query.profile_id, settings::tabs::tab_layout.key(), &setting));

        settings::TabLayout tab_layout = settings::TabLayout::standard;
        if (setting.has_value()) {
            VERIFY(settings::unmarshal_value(setting->value, &tab_layout));
        }

        CreateToolbarItemQuery item_query{
            .profile_id = query.profile_id,
            .item_type = ToolbarItemType::programmable_button,
            .next_item_id = query.next_item_id,
            .is_compact_tab_layout = (tab_layout == settings::TabLayout::compact),
            .in_overflow_menu = query.in_overflow_menu,
        };

        ToolbarItemId toolbar_item_id{};
        VERIFY(m_profiles_db->create_toolbar_item(
            item_query, TxMode::no_tx, &toolbar_item_id, toolbar_items_change.get(), prog_buttons_change.get()));
        
        UpdateProgrammableButtonQuery update_query{
            .id = static_cast<ProgrammableButtonId>(toolbar_item_id.value),
            .title = query.title,
            .update_title = true,
            .icon_string = query.icon_string,
            .update_icon_string = true,
            .action_type = query.action_type,
            .update_action_type = true,
            .action_string = query.action_string,
            .update_action_string = true,
            .keyboard_shortcut = query.keyboard_shortcut,
            .update_keyboard_shortcut = true,
        };

        VERIFY(m_profiles_db->update_programmable_button(update_query, TxMode::no_tx, prog_buttons_change.get()));
        VERIFY(tx.commit());
        SUCCEED();
    }).then_or_error([cb=std::move(cb),this,toolbar_items_change,prog_buttons_change](Error err) {
        cb(std::move(err));
        if(!toolbar_items_change->created.empty() || !toolbar_items_change->updated.empty() || !toolbar_items_change->deleted.empty())
        {
            m_toolbar_items_change->notify(std::move(*toolbar_items_change));
        }
        if(!prog_buttons_change->created.empty() || !prog_buttons_change->updated.empty() || !prog_buttons_change->deleted.empty())
        {
            m_programmable_buttons_change->notify(std::move(*prog_buttons_change));
        }
    });
}

void
CoreApi::update_programmable_button(UpdateProgrammableButtonQuery query, Cb<Error> cb)
{
    auto change = std::make_shared<ProgrammableButtonsChange>();
    dispatch(on_disk_thread(), [guard=shared_from_this(),this,query, change] {
        VERIFY(m_profiles_db->update_programmable_button(query, TxMode::use_tx, change.get()));
        SUCCEED();
    }).then_or_error([cb=std::move(cb),this,change](Error err) {
        cb(std::move(err));
        if(!change->created.empty() || !change->updated.empty() || !change->deleted.empty())
        {
            m_programmable_buttons_change->notify(std::move(*change));
        }
    });
}

//-------------------------------------------------------------------------
// Other
//-------------------------------------------------------------------------

void
CoreApi::async(std::function<void()> work)
{
    dispatch(on_main_thread(), std::move(work));
}

void
CoreApi::async_after(std::chrono::milliseconds after, std::function<void()> work)
{
    dispatch(on_main_thread(), orion::after(after), std::move(work));
}

void
CoreApi::async_disk(std::function<void()> work)
{
    dispatch(on_disk_thread(), std::move(work));
}

void
CoreApi::when_all(std::vector<Cb<Cb<Error>>> fns, Cb<Error> done_cb)
{
    if (fns.empty()) {
        dispatch(on_main_thread(), [done_cb=std::move(done_cb)] {
            done_cb(Error::ok());
        });
        return;
    }

    auto remaining = std::make_shared<size_t>(fns.size());
    auto any_error = std::make_shared<Error>(Error::ok());

    for (std::function<void(Cb<Error>)> fn : fns) {
        fn([remaining,any_error,done_cb](Error err) {
            if (err && !*any_error) {
                // Store the first error that occurred, but still wait for all operations to complete before
                // calling the final callback
                *any_error = err;
            }
            (*remaining)--;
            if (*remaining == 0) {
                done_cb(*any_error);
            }
        });
    }
}

} // namespace orion
