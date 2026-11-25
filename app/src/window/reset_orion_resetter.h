
#pragma once

#include <cstdio>
#include <ctime>
#include <string>
#include <map>
#include <unordered_map>
#include <optional>
#include <algorithm>

#include "app.h"
#include "util/profile_manager.h"

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
#include <webkit/webkit.h>
#endif

/**
 * @brief Base interface for all option resetters used by the Reset Orion dialog.
 *
 * Implementations should override @c reset_option to perform the actual
 * clearing operation and @c is_checked to report whether the option is
 * currently selected for the given profile.
 */
class OptionResetterBase
{
protected:
    /**
     * Mapping from dialog "older than" dropdown index to seconds.
     * Indices follow the OlderThan enum layout in settings.
     */
    static inline std::unordered_map<int, std::int64_t> seconds_ago_mapping = {
        {2, 3600},          // one_hour
        {3, 86400},         // one_day
        {4, 7 * 86400},     // one_week
        {5, 30 * 86400},    // one_month
    };

public:
    virtual ~OptionResetterBase() = default;

    /**
     * @brief Perform the reset operation for the given profile.
     *
     * @param profile_id Profile identifier to operate on.
     * @param drop_down_index The OlderThan index selected in the dialog.
     * @param win The main window instance.
     */
    virtual void
    reset_option(orion::ProfileId profile_id, int drop_down_index, MainWindow* win) {
        orion::log_debug("Unimplemented");
    }

    /**
     * @brief Query whether this reset option is currently checked for a profile.
     *
     * @param profile_id Profile identifier to query.
     * @return true if the option is checked for @p profile_id, false otherwise.
     */
    virtual bool is_checked(orion::ProfileId profile_id) = 0;
};


template <std::size_t N>
struct StringLiteral
{
    char value[N];
    constexpr StringLiteral(const char (&str)[N])
    {
        std::copy_n(str, N, value);
    }
};

/**
 * @brief Template helper that ties a core SettingTag to a widget check button.
 *
 * Provides static callbacks used by the GTK template to synchronise the
 * widget state with the core setting and to persist changes.
 */
template <typename SettingTag, StringLiteral WidgetName>
class OptionResetter : public OptionResetterBase
{
private:
    static inline std::map<orion::ProfileId, bool> checked_states;
    static inline const auto s_setting = orion::settings::Setting<SettingTag>(orion::settings::reset_orion::realm_name().c_str());

public:
    explicit OptionResetter() = default;

protected:
    /**
     * @brief GTK callback for when the check button is toggled by the user.
     *
     * Updates the cached state and persists the new value to the core API.
     */
    static void
    on_toggled_static(GtkCheckButton* button, gpointer user_data)
    {
        orion::ProfileId profile_id = main_window_get_current_profile_id(MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(button))));
        bool val = gtk_check_button_get_active(button);
        checked_states[profile_id] = val;
        global_core_api->set_setting(profile_id, s_setting, val,
                                     [](orion::SetSettingReply<bool> reply)
                                     { 
                                        VERIFY_LOG(reply.err); 
                                     });
    }

    /**
     * @brief GTK callback invoked when the widget is mapped; synchronises
     * the widget state from the core setting store.
     */
    static void
    on_map_static(GtkCheckButton* button, gpointer user_data)
    {
        orion::ProfileId profile_id = main_window_get_current_profile_id(MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(button))));
        auto wrapped_ref = std::make_shared<orion::gtk::WeakRefWrapper>(button);
        global_core_api->get_setting(profile_id, s_setting,
                                     [wrapped_ref, profile_id,user_data](orion::GetSettingReply<bool> reply)
                                     {
                                         VERIFY_LOG(reply.err);
                                         checked_states[profile_id] = reply.value;
                                         GtkCheckButton* button = GTK_CHECK_BUTTON(wrapped_ref->get());
                                         if (button)
                                         {
                                             g_signal_handlers_block_by_func(button, (gpointer)on_toggled_static, user_data);
                                             gtk_check_button_set_active(button, reply.value);
                                             g_signal_handlers_unblock_by_func(button, (gpointer)on_toggled_static, user_data);
                                             g_object_unref(button);
                                         }
                                     });
    }
    
    /**
     * @brief See OptionResetterBase::is_checked.
     */
    bool
    is_checked(orion::ProfileId profile_id) override
    {
        auto it = checked_states.find(profile_id);
        if (it != checked_states.end())
        {
            return it->second;
        }
        return false;
    }

public:
    /**
     * @brief Bind the GTK template callbacks for this widget name.
     *
     * @param widget_class GTK widget class to bind callbacks to.
     */
    static void
    register_callbacks(GtkWidgetClass* widget_class)
    {
        std::string toggle_name = std::string("on_") + WidgetName.value + "_toggled";
        std::string map_name = std::string("on_") + WidgetName.value + "_map";

        gtk_widget_class_bind_template_callback_full(
            widget_class,
            toggle_name.c_str(),
            G_CALLBACK(on_toggled_static));

        gtk_widget_class_bind_template_callback_full(
            widget_class,
            map_name.c_str(),
            G_CALLBACK(on_map_static));
    }
};

/**
 * The following concrete resetter classes implement the actual reset logic
 * for individual options shown in the Reset Orion dialog. Each class
 * overrides reset_option and performs core API calls as necessary.
 */
namespace res = orion::settings::reset_orion;

class CleanHistoryResetter final :
    public OptionResetter<res::internal_key_clear_history, "clear_history_check_button">
{
public:
    using OptionResetter::OptionResetter;

    /**
     * @brief Clear history entries according to the selected time range.
     *
     * If @p drop_down_index indicates all time the entire history is cleared.
     */
    virtual void
    reset_option(orion::ProfileId profile_id, int drop_down_index, MainWindow* win) override
    {
        orion::log_debug("Resetting clear history with older than index: %d", drop_down_index);

        if (drop_down_index <= 1) // not_set (0) or all_time (1)    
        {
            const orion::ProfileHistoryQuery query{
                .profile_id = profile_id,
            };
            global_core_api->clear_history_entries(query, [](orion::Error err)
            {
                VERIFY_LOG(err);
            });
            return;
        }

        int64_t seconds_ago = seconds_ago_mapping[drop_down_index];

        int64_t cutoff = static_cast<int64_t>(std::time(nullptr)) - seconds_ago;

        orion::GetHistoryEntryIdsQuery query{
            .profile_id = profile_id,
            .visited_after  = orion::UnixTimeUtc64(0),
            .visited_before = orion::UnixTimeUtc64(cutoff),
        };

        global_core_api->get_history_entry_ids(query, [](orion::GetHistoryEntryIdsReply reply)
        {
            VERIFY_LOG(reply.err);
            if (!reply.ids.empty())
            {
                global_core_api->delete_history_entries(reply.ids, [reply](orion::Error err)
                {
                    VERIFY_LOG(err);
                });
            }
        });
    }
};

class BookmarksResetter final :
    public OptionResetter<res::internal_key_remove_bookmarks, "remove_bookmarks_check_button">
{
public:
    using OptionResetter::OptionResetter;

    /**
     * @brief Remove bookmarks optionally filtered by creation time.
     *
     * When removing all bookmarks folders that are not internal are also
     * removed.
     */
    virtual void
    reset_option(orion::ProfileId profile_id, int drop_down_index, MainWindow* win) override
    {
        const bool all_time = drop_down_index <= 1;

        std::optional<orion::UnixTimeUtc64> created_before;
        if (!all_time)
        {
            int64_t seconds_ago = seconds_ago_mapping[drop_down_index];
            int64_t cutoff = static_cast<int64_t>(std::time(nullptr)) - seconds_ago;
            created_before = orion::UnixTimeUtc64(cutoff);
        }

        orion::log_debug("Resetting remove bookmarks (all_time=%d, created_before=%s)",
            all_time, created_before ? std::to_string(created_before->value).c_str() : "(none)");

        orion::GetBookmarkIdsQuery query{
            .profile_id = profile_id,
            .created_before = created_before,
        };

        global_core_api->get_bookmark_ids(query, [profile_id, all_time](orion::GetBookmarkIdsReply reply)
        {
            VERIFY_LOG(reply.err);
            if (!reply.ids.empty())
            {
                global_core_api->delete_bookmarks(reply.ids, [](orion::Error err)
                {
                    VERIFY_LOG(err);
                });
            }

            if (!all_time)
                return;

            global_core_api->get_bookmark_folder_ids(profile_id, [](orion::GetBookmarkFolderIdsReply reply)
            {
                VERIFY_LOG(reply.err);
                if (reply.ids.empty())
                    return;

                global_core_api->get_bookmark_folders(reply.ids, [](orion::GetBookmarkFoldersReply reply)
                {
                    VERIFY_LOG(reply.err);
                    std::vector<orion::BookmarkFolderId> deletable_ids;
                    for (const auto& folder : reply.folders)
                    {
                        if (!folder.is_internal)
                            deletable_ids.push_back(folder.id);
                    }
                    if (!deletable_ids.empty())
                    {
                        global_core_api->delete_bookmark_folders(deletable_ids, [](orion::Error err)
                        {
                            VERIFY_LOG(err);
                        });
                    }
                });
            });
        });
    }
};

class ReadingListResetter final :
    public OptionResetter<res::internal_key_remove_reading_list, "remove_reading_list_check_button">
{
public:
    using OptionResetter::OptionResetter;

    /**
     * @brief Remove reading list entries optionally filtered by age.
     */
    virtual void
    reset_option(orion::ProfileId profile_id, int drop_down_index, MainWindow* win) override
    {
        orion::GetReadingListEntryIdsQuery query{
            .profile_id = profile_id,
        };

        if (drop_down_index > 1)
        {
            int64_t seconds_ago = seconds_ago_mapping[drop_down_index];
            int64_t cutoff = static_cast<int64_t>(std::time(nullptr)) - seconds_ago;
            query.added_before = orion::UnixTimeUtc64(cutoff);
        }

        orion::log_debug("Resetting clear reading list (drop_down_index=%d, added_before=%s)",
            drop_down_index,
            drop_down_index > 1 ? std::to_string(query.added_before.value).c_str() : "(all time)"
        );

        global_core_api->get_reading_list_entry_ids(query, [](orion::GetReadingListEntryIdsReply reply)
        {
            VERIFY_LOG(reply.err);
            if (!reply.ids.empty())
            {
                global_core_api->delete_reading_list_entries(reply.ids, [](orion::Error err)
                {
                    VERIFY_LOG(err);
                });
            }
        });
    }
};

class SnapshotResetter final :
    public OptionResetter<res::internal_key_remove_snapshots, "remove_snapshots_check_button">
{
public:
    using OptionResetter::OptionResetter;

    /**
     * @brief Delete the last snapshot for the profile if it matches the filter.
     */
    virtual void
    reset_option(orion::ProfileId profile_id, int drop_down_index, MainWindow* win) override
    {
        orion::log_debug("Resetting remove snapshots (drop_down_index=%d)", drop_down_index);

        global_core_api->get_last_snapshot(profile_id, [profile_id, drop_down_index](orion::GetLastSnapshotReply reply)
        {
            VERIFY_LOG(reply.err);

            if (reply.snapshot.created_at == orion::UnixTimeUtc64(0))
                return;

            if (drop_down_index > 1)
            {
                int64_t seconds_ago = seconds_ago_mapping[drop_down_index];
                int64_t cutoff = static_cast<int64_t>(std::time(nullptr)) - seconds_ago;
                if (reply.snapshot.created_at.value >= cutoff)
                {
                    orion::log_debug("Snapshot is newer than cutoff, skipping delete");
                    return;
                }
            }

            global_core_api->delete_snapshot(profile_id, [](orion::DeleteSnapshotReply reply)
            {
                VERIFY_LOG(reply.err);
            });
        });
    }
};

class DownloadsResetter final :
    public OptionResetter<res::internal_key_remove_downloads, "remove_downloads_check_button">
{
public:
    using OptionResetter::OptionResetter;

    /**
     * @brief Remove downloads optionally filtered by timestamp.
     */
    virtual void
    reset_option(orion::ProfileId profile_id, int drop_down_index, MainWindow* win) override
    {
        if (drop_down_index <= 1) // not_set (0) or all_time (1)
        {
            orion::log_debug("Resetting clear downloads (all time)");
            orion::DeleteDownloadsQuery query{
                .profile_id = profile_id,
            };
            global_core_api->delete_downloads(query, [](orion::DeleteDownloadsReply reply){
                VERIFY_LOG(reply.err);
            });
            return;
        }

        int64_t seconds_ago = seconds_ago_mapping[drop_down_index];
        int64_t cutoff = static_cast<int64_t>(std::time(nullptr)) - seconds_ago;

        orion::log_debug("Resetting clear downloads (drop_down_index=%d, cutoff=%lld)", drop_down_index, (long long)cutoff);

        orion::GetDownloadIdsQuery get_query{
            .profile_id = profile_id,
        };
        global_core_api->get_downloads_by_query(get_query, [cutoff](orion::GetDownloadsReply reply)
        {
            VERIFY_LOG(reply.err);
            std::vector<orion::DownloadId> to_delete;
            for (const auto& dl : reply.downloads)
            {
                if (dl.timestamp.value < cutoff)
                    to_delete.push_back(dl.id);
            }
            if (!to_delete.empty())
            {
                orion::DeleteDownloadsQuery to_delete_query{
                    .ids = to_delete,
                };
                global_core_api->delete_downloads(to_delete_query, [](orion::DeleteDownloadsReply reply)
                {
                    VERIFY_LOG(reply.err);
                });
            }
        });
    }
};

/** 
 * ICloudApp is Remove server sync app data option in dialog.
**/
class ICloudAppDataResetter final :
     public OptionResetter<res::internal_key_remove_icloud_app_data, "remove_icloud_app_data_check_button">
{
public:
    using OptionResetter::OptionResetter;

    static void
    on_toggled_static(GtkCheckButton* button, gpointer user_data)
    {
        /* Ignore user toggles; keep unchecked. */
        gtk_check_button_set_active(button, FALSE);
    }

    static void
    on_map_static(GtkCheckButton* button, gpointer user_data)
    {
        /* Ensure the widget is unchecked and not user-interactable. */
        gtk_check_button_set_active(button, FALSE);
        gtk_widget_set_sensitive(GTK_WIDGET(button), FALSE);
    }

    static void
    register_callbacks(GtkWidgetClass* widget_class)
    {
        gtk_widget_class_bind_template_callback_full(widget_class, "on_remove_icloud_app_data_check_button_toggled", G_CALLBACK(on_toggled_static));
        gtk_widget_class_bind_template_callback_full(widget_class, "on_remove_icloud_app_data_check_button_map", G_CALLBACK(on_map_static));
    }

    /**
     * @brief iCloud-specific operation; not applicable on Linux.
     */
    virtual void
    reset_option(orion::ProfileId profile_id, int drop_down_index, MainWindow* win) override
    {
        orion::log_debug("Resetting clear iCloud app data - NOT IMPLEMENTED: Apple-specific, not applicable on Linux");
    }
};

class SettingsResetter final :
    public OptionResetter<res::internal_key_reset_all_settings, "reset_all_settings_check_button">
{
public:
    using OptionResetter::OptionResetter;

    /**
     * @brief Reset all settings - currently not implemented in core API.
     */
    virtual void
    reset_option(orion::ProfileId profile_id, int drop_down_index, MainWindow* win) override
    {
        orion::log_debug("Resetting all settings for profile_id=%lld (deleting settings rows)", profile_id.value);

        global_core_api->delete_settings(profile_id, [](orion::Error err)
        {
            VERIFY_LOG(err);
        });
    }
};

class CookiesResetter final :
    public OptionResetter<res::internal_key_remove_all_cookies, "remove_all_cookies_check_button">
{
public:
    using OptionResetter::OptionResetter;
#if (defined(ENABLE_WEBKIT) && ENABLE_WEBKIT)
    /**
     * @brief Clear all cookies via WebKit's WebsiteDataManager.
     */
    virtual void
    reset_option(orion::ProfileId profile_id, int drop_down_index, MainWindow* win) override
    {
        orion::log_debug("Resetting clear cookies (drop_down_index=%d ignored - WebKit clears all cookies, no time filter available)", drop_down_index);

        WebExtensionWebViewFactory& factory = main_window_get_current_webview_factory(win);
        WebKitNetworkSession* session = (WebKitNetworkSession*)factory.network_session();
        WebKitWebsiteDataManager* manager = webkit_network_session_get_website_data_manager(session);

        webkit_website_data_manager_clear(
            manager,
            WEBKIT_WEBSITE_DATA_COOKIES,
            0, nullptr,
            [](GObject* source, GAsyncResult* result, gpointer)
            {
                GError* error = nullptr;
                webkit_website_data_manager_clear_finish(WEBKIT_WEBSITE_DATA_MANAGER(source), result, &error);
                if (error)
                {
                    orion::log_debug("Error clearing cookies: %s", error->message);
                    g_error_free(error);
                }
            }, nullptr);
    }
#endif
};

class BrowserCacheResetter final :
    public OptionResetter<res::internal_key_empty_browser_cache, "empty_browser_cache_check_button">
{
public:
    using OptionResetter::OptionResetter;

#if (defined(ENABLE_WEBKIT) && ENABLE_WEBKIT)
    /**
     * @brief Clear WebKit disk and memory caches.
     */
    virtual void
    reset_option(orion::ProfileId profile_id, int drop_down_index, MainWindow* win) override
    {
        orion::log_debug("Resetting clear browser cache (drop_down_index=%d ignored - WebKit clears all cache, no time filter available)", drop_down_index);

        WebExtensionWebViewFactory& factory = main_window_get_current_webview_factory(win);
        WebKitNetworkSession* session = (WebKitNetworkSession*)factory.network_session();
        WebKitWebsiteDataManager* manager = webkit_network_session_get_website_data_manager(session);

        webkit_website_data_manager_clear(
            manager,
            static_cast<WebKitWebsiteDataTypes>(WEBKIT_WEBSITE_DATA_DISK_CACHE | WEBKIT_WEBSITE_DATA_MEMORY_CACHE),
            0, nullptr,
            [](GObject* source, GAsyncResult* result, gpointer)
            {
                GError* error = nullptr;
                webkit_website_data_manager_clear_finish(WEBKIT_WEBSITE_DATA_MANAGER(source), result, &error);
                if (error)
                {
                    orion::log_debug("Error clearing browser cache: %s", error->message);
                    g_error_free(error);
                }
            }, nullptr);
    }
#endif
};

class LocalStorageAndDatabaseResetter final :
    public OptionResetter<res::internal_key_empty_local_storage_and_databases, "empty_local_storage_and_databases_check_button">
{
public:
    using OptionResetter::OptionResetter;
#if (defined(ENABLE_WEBKIT) && ENABLE_WEBKIT)
    /**
     * @brief Clear local storage, IndexedDB and DOM cache via WebKit.
     */
    virtual void
    reset_option(orion::ProfileId profile_id, int drop_down_index, MainWindow* win) override
    {
        orion::log_debug("Resetting clear local storage and database (drop_down_index=%d ignored - WebKit clears all, no time filter available)", drop_down_index);

        WebExtensionWebViewFactory& factory = main_window_get_current_webview_factory(win);
        WebKitNetworkSession* session = (WebKitNetworkSession*)factory.network_session();
        WebKitWebsiteDataManager* manager = webkit_network_session_get_website_data_manager(session);

        webkit_website_data_manager_clear(
            manager,
            static_cast<WebKitWebsiteDataTypes>(
                WEBKIT_WEBSITE_DATA_LOCAL_STORAGE |
                WEBKIT_WEBSITE_DATA_INDEXEDDB_DATABASES |
                WEBKIT_WEBSITE_DATA_DOM_CACHE),
            0, nullptr,
            [](GObject* source, GAsyncResult* result, gpointer)
            {
                GError* error = nullptr;
                webkit_website_data_manager_clear_finish(WEBKIT_WEBSITE_DATA_MANAGER(source), result, &error);
                if (error)
                {
                    orion::log_debug("Error clearing local storage and databases: %s", error->message);
                    g_error_free(error);
                }
            }, nullptr);
    }
#endif
};

class PasswordsResetter final :
    public OptionResetter<res::internal_key_remove_saved_passwords, "remove_saved_passwords_check_button">
{
public:
    using OptionResetter::OptionResetter;

    /**
     * @brief Remove all saved passwords for the profile.
     */
    virtual void
    reset_option(orion::ProfileId profile_id, int drop_down_index, MainWindow* win) override
    {
        orion::log_debug("Resetting clear passwords (drop_down_index=%d ignored - passwords have no timestamp, all will be removed)", drop_down_index);

        orion::GetPasswordItemIdsQuery query{
            .profile_id = profile_id,
        };

        global_core_api->get_password_item_ids(query, [](orion::GetPasswordItemIdsReply reply)
        {
            VERIFY_LOG(reply.err);
            if (!reply.ids.empty())
            {
                global_core_api->delete_password_items(reply.ids, [](orion::Error err)
                {
                    VERIFY_LOG(err);
                });
            }
        });
    }
};

class ExtensionsResetter final :
    public OptionResetter<res::internal_key_remove_all_extensions, "remove_all_extensions_check_button">
{
public:
    using OptionResetter::OptionResetter;

    /**
     * @brief Uninstall all installed extensions for the profile.
     */
    virtual void
    reset_option(orion::ProfileId profile_id, int drop_down_index, MainWindow* win) override
    {
        orion::log_debug("Resetting remove extensions (drop_down_index=%d ignored - extensions have no timestamp, all will be uninstalled)", drop_down_index);

        orion::GetInstalledExtensionIdsQuery query{
            .profile_id = profile_id,
        };

        global_core_api->get_installed_extension_ids(query, [](orion::GetAllInstalledExtensionIdsReply reply)
        {
            VERIFY_LOG(reply.err);
            for (const auto& id : reply.ids)
            {
                orion::UninstallExtensionQuery uquery{ .id = id };
                global_core_api->uninstall_extension(uquery, [](orion::UninstallExtensionReply reply)
                {
                    VERIFY_LOG(reply.err);
                });
            }
        });
    }
};

class CloseWindowsResetter final :
    public OptionResetter<res::internal_key_close_all_windows, "close_all_windows_check_button">
{
public:    
    using OptionResetter::OptionResetter;

    /**
     * @brief Close all windows belonging to the profile and quit the app.
     */
    virtual void
    reset_option(orion::ProfileId profile_id, int drop_down_index, MainWindow* win) override
    {
        orion::log_debug("Resetting close all windows (drop_down_index=%d ignored - windows have no timestamp)", drop_down_index);

        global_core_api->get_window_ids({.profile_id=profile_id}, [profile_id](orion::GetWindowIdsReply reply)
        {
            VERIFY_LOG(reply.err);
            if (reply.ids.empty())
            {
                
                orion::profile::quit(profile_id);
                return;
            }

            auto pending = std::make_shared<std::atomic<int>>(static_cast<int>(reply.ids.size()));
            for (orion::WindowId window_id : reply.ids)
            {
                global_core_api->close_window(window_id, [pending, profile_id](orion::Error err)
                {
                    VERIFY_LOG(err);
                    if (--(*pending) == 0)
                        orion::profile::quit(profile_id);
                });
            }
        });
    }
};

