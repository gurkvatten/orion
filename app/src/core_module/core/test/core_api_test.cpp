#define ENABLE_CORE_API_TEST !NDEBUG && 1
#define ENABLE_DEFAULT_PROFILE_CHANGE_TEST 0
#define ENABLE_VERBOSE_LOGS 0
#define ENABLE_VERBATIM_LOGS 0

#include <ranges>
#include <algorithm>
#include <random>
#include <limits>
#include <atomic>
#include <filesystem>
#include <ctime>
#include <iomanip>
#include <sstream>

#include <core/test/core_api_test.h>
#include <core/core_api.h>
#include <common/src/defer.h>
#include <core/settings.h>
#include <core/test/done.h>
#include <common/src/coroutine.h>
#include <common/src/string_util.h>

#if ENABLE_VERBATIM_LOGS
#define log_verbatim(format, ...) orion::log_debug( \
    "%s(%d): " format,
    std::filesystem::path(__FILE__).filename().c_str(), \
    __LINE__, \
    ##__VA_ARGS__)
#else
#define log_verbatim(format, ...)
#endif

using namespace std::chrono_literals;

namespace orion {

class Tracker {
public:
    Tracker()
    {
        log_verbatim("Tracker(): n=%lld", (m_count.fetch_add(1) + 1));
    }
    
    ~Tracker()
    {
        log_verbatim("~Tracker(): n=%lld", (m_count.fetch_sub(1) - 1));
    }

    Tracker(const Tracker&)
    {
        log_verbatim("Tracker(const Tracker&): n=%lld", (m_count.fetch_add(1) + 1));
    }

    Tracker& operator=(const Tracker&) = delete;
    Tracker(Tracker&&) = delete;
    Tracker& operator=(Tracker&&) = delete;

private:
    static std::atomic<int64_t> m_count;
};

std::atomic<int64_t> Tracker::m_count(0);

static void run_all_tests_impl(CoreApi* core_api, ProfileId profile_id, std::shared_ptr<Done> done);

void
test_memory_leaks(CoreApi* core_api, ProfileId profile_id, std::shared_ptr<Done> done)
{
    Tracker tracker;

    core_api->async([tracker, done]{
        log_verbatim("leak_test() work");
        done->ok();
    });
}

void
test_bookmarks(CoreApi* core_api, ProfileId profile_id, std::shared_ptr<Done> done_all)
{
    auto folder_observer_id = core_api->bookmark_folders_change().add_observer([](const BookmarkFoldersChange& change) {
        log_verbatim("bookmark folders changed: created %zu, updated %zu, deleted %zu",
            change.created.size(), change.updated.size(), change.deleted.size()
        );
    });
    auto bookmark_observer_id = core_api->bookmarks_change().add_observer([](const BookmarksChange& change) {
        log_verbatim("bookmarks changed: created %zu, updated %zu, deleted %zu",
            change.created.size(), change.updated.size(), change.deleted.size()
        );
    });

    auto done = std::make_shared<Done>(12, core_api,
    [done_all,folder_observer_id,bookmark_observer_id,core_api](orion::Error err) {
        DONE_CHECK(err, done_all);
        core_api->bookmarks_change().remove_observer(bookmark_observer_id);
        core_api->bookmark_folders_change().remove_observer(folder_observer_id);
        log_verbatim("test_bookmarks done");
        VERIFY_LOG(done_all->ok());
    });

    core_api->get_bookmark_folder_ids(profile_id, [done,core_api](orion::GetBookmarkFolderIdsReply reply) {
        DONE_CHECK(reply.err, done);
        log_verbatim("got %zu bookmark folder ids", reply.ids.size());
        
        core_api->get_bookmark_folders(reply.ids, [done](orion::GetBookmarkFoldersReply reply) {
            DONE_CHECK(reply.err, done);
            log_verbatim("got %zu bookmark folders", reply.folders.size());
            VERIFY_LOG(done->ok());
        });
    });

    {
        GetBookmarkIdsQuery query{
            .profile_id = profile_id,
        };

        // Old callback style:
        core_api->get_bookmark_ids(query, [done,core_api](orion::GetBookmarkIdsReply reply) {
            DONE_CHECK(reply.err, done);
            log_verbatim("got %zu bookmark ids", reply.ids.size());
            
            core_api->get_bookmarks(GetBookmarksQuery{.ids=reply.ids},
            [done,core_api](orion::GetBookmarksReply reply) {
                DONE_CHECK(reply.err, done);
                log_verbatim("got %zu bookmarks", reply.bookmarks.size());
                VERIFY_LOG(done->ok());
            });
        });

        // // New coroutine style:
        // [core_api](GetBookmarkIdsQuery query)->async_task<Error>{
        //     auto ids_reply = API_AWAIT(core_api, get_bookmark_ids, query);
        //     if (ids_reply.err) {
        //         co_return ids_reply.err;
        //     }
        //     log_verbose("got %zu bookmark ids", ids_reply.ids.size());
        //     auto bookmarks_reply = API_AWAIT(core_api, get_bookmarks, GetBookmarksQuery{.ids=ids_reply.ids});
        //     if (bookmarks_reply.err) {
        //         co_return bookmarks_reply.err;
        //     }
        //     log_verbose("got %zu bookmarks", bookmarks_reply.bookmarks.size());
        //     co_return Error::ok();
        // }(query).then([done](Error err) {
        //     DONE_CHECK(err, done);
        //     VERIFY_LOG(done->ok());
        // });
    }
    {
        GetBookmarkIdsQuery query{};
        query.profile_id = profile_id;
        core_api->get_bookmark_ids(query, [profile_id,done,core_api](orion::GetBookmarkIdsReply reply) {
            DONE_CHECK(reply.err, done);
            log_verbatim("got %zu bookmark ids before creation", reply.ids.size());
            
            core_api->get_bookmarks(GetBookmarksQuery{.ids=reply.ids},
            [profile_id,done,core_api](orion::GetBookmarksReply reply) {
                DONE_CHECK(reply.err, done);
                log_verbatim("got %zu bookmarks before creation", reply.bookmarks.size());
                
                orion::CreateBookmarkQuery query{};
                query.profile_id = profile_id;
                query.name = "Test Bookmark";
                query.address = "https://example.com";
                query.invocation_keyword = "example";
                query.parent_folder_id = BookmarkFolderId(0); // create at the root level
                core_api->create_bookmark(query, [done,core_api](orion::CreateBookmarkReply reply) {
                    DONE_CHECK(reply.err, done);
                    log_verbatim("created bookmark");

                    core_api->delete_bookmarks({reply.bookmark_id}, [done,core_api](orion::Error err) {
                        DONE_CHECK(err, done);
                        log_verbatim("deleted bookmark");
                        VERIFY_LOG(done->ok());
                    });
                });
            });
        });
    }
    {
        orion::CreateBookmarkFolderQuery query{};
        query.profile_id = profile_id;
        query.name = "Test Folder";
        query.parent_folder_id = BookmarkFolderId(0); // create at the root level
        core_api->create_bookmark_folder(query, [profile_id,done,core_api](orion::CreateBookmarkFolderReply reply) {
            DONE_CHECK(reply.err, done);
            log_verbatim("created bookmark folder");

            orion::CreateBookmarkFolderQuery query{};
            query.profile_id = profile_id;
            query.name = "Test Folder 2";
            query.parent_folder_id = reply.folder_id; // create inside the first folder

            core_api->create_bookmark_folder(query,
            [profile_id,parent_folder_id=reply.folder_id,done,core_api](orion::CreateBookmarkFolderReply reply) {
                DONE_CHECK(reply.err, done);
                log_verbatim("created bookmark folder inside another folder");

                core_api->delete_bookmark_folders({reply.folder_id},
                [parent_folder_id,done,core_api](orion::Error err) {
                    DONE_CHECK(err, done);
                    log_verbatim("deleted bookmark folder %lld", parent_folder_id.value);
                    VERIFY_LOG(done->ok());
                });
            });
        });
    }
    {
        orion::CreateBookmarkFolderQuery query{};
        query.profile_id = profile_id;
        query.name = "Test Folder";
        query.parent_folder_id = BookmarkFolderId(0); // create at the root level
        core_api->create_bookmark_folder(query, [profile_id,done,core_api](orion::CreateBookmarkFolderReply reply) {
            DONE_CHECK(reply.err, done);
            log_verbatim("created bookmark folder");

            orion::CreateBookmarkFolderQuery query{};
            query.profile_id = profile_id;
            query.name = "Test Folder (2nd level)";
            query.parent_folder_id = reply.folder_id; // create inside the first folder
            core_api->create_bookmark_folder(query,
            [folder_id=reply.folder_id,done,core_api](orion::CreateBookmarkFolderReply reply) {
                DONE_CHECK(reply.err, done);
                log_verbatim("created bookmark folder inside another folder");

                orion::UpdateBookmarkFolderQuery query{};
                query.id = folder_id;
                query.name = "Test Folder (self-reference)";
                query.parent_folder_id = folder_id;
                query.update_name = true;
                query.update_parent_folder_id = true;
                core_api->update_bookmark_folder(query, [folder_id,done,core_api](orion::Error err) {
                    if (err.code() == orion::ErrorCode::circular_reference) {
                        log_verbatim("circular reference detected, skipping update of folder inside itself");
                        VERIFY_LOG(done->ok());
                        return;
                    }
                    DONE_CHECK(err, done); // this should not happen
                });

                query.id = folder_id;
                query.name = "Test Folder (circular reference)";
                query.parent_folder_id = reply.folder_id; // this should create a circular reference
                query.update_name = true;
                query.update_parent_folder_id = true;
                core_api->update_bookmark_folder(query, [folder_id,done,core_api](orion::Error err) {
                    if (err.code() == orion::ErrorCode::circular_reference) {
                        log_verbatim("circular reference detected, skipping update of folder inside itself");
                        VERIFY_LOG(done->ok());
                        return;
                    }
                    DONE_CHECK(err, done); // this should not happen
                });

                query.id = folder_id;
                query.name = "Test Folder (changed)";
                query.parent_folder_id = BookmarkFolderId(0); // move to root level
                query.update_name = true;
                query.update_parent_folder_id = false;
                core_api->update_bookmark_folder(query, [folder_id, done, core_api](orion::Error err) {
                    DONE_CHECK(err, done);
                    log_verbatim("updated bookmark folder");
                    
                    orion::UpdateBookmarkFolderQuery query{};
                    query.id = folder_id;
                    query.name = "Test Folder";
                    query.update_name = true;
                    core_api->update_bookmark_folder(query, [folder_id,done,core_api](orion::Error err) {
                        DONE_CHECK(err, done);
                        log_verbatim("updated bookmark folder");

                        core_api->delete_bookmark_folders({folder_id}, [done,core_api](orion::Error err) {
                            DONE_CHECK(err, done);
                            log_verbatim("deleted bookmark folder");
                            VERIFY_LOG(done->ok());
                        });
                    });
                });
            });
        });
    }
    {
        // test: create a bookmark folder in Bookmarks Bar, then update it to move it to the root level, then delete it
        orion::UpdateBookmarkFolderQuery query{};
        query.id = orion::BookmarkFolderId(161616511); // random not existing folder id
        core_api->update_bookmark_folder(query, [done,core_api](orion::Error err) {
            if (err.code() == orion::ErrorCode::not_found) {
                VERIFY_LOG(done->ok());
                return;
            }
            DONE_ASSERT(
                done,
                err.code() == orion::ErrorCode::not_found,
                "update of non-existing bookmark folder should fail"
            );
        });

        core_api->get_bookmark_folder_id_by_type(profile_id, orion::BookmarkFolderType::bookmarks_bar,
        [profile_id,done,core_api](orion::GetBookmarkFolderIdByTypeReply reply) {
            DONE_CHECK(reply.err, done);
            orion::CreateBookmarkFolderQuery query{};
            query.profile_id = profile_id;
            query.name = "Test Bookmarks Bar Folder";
            query.parent_folder_id = reply.folder_id.value; // create inside the Bookmarks Bar folder
            core_api->create_bookmark_folder(query, [done,core_api](orion::CreateBookmarkFolderReply reply) {
                DONE_CHECK(reply.err, done);
                log_verbatim("created bookmark folder in Bookmarks Bar");

                orion::UpdateBookmarkFolderQuery update_query{};
                update_query.id = reply.folder_id;
                update_query.name = "Test Bookmarks Bar Folder (moved to root)";
                update_query.parent_folder_id = BookmarkFolderId(0); // move to root level
                update_query.update_name = true;
                update_query.update_parent_folder_id = true;
                core_api->update_bookmark_folder(update_query,
                [folder_id=reply.folder_id, done, core_api](orion::Error err) {
                    DONE_CHECK(err, done);
                    log_verbatim("updated bookmark folder to move it to root level");

                    core_api->delete_bookmark_folders({folder_id}, [done,core_api](orion::Error err) {
                        DONE_CHECK(err, done);
                        log_verbatim("deleted bookmark folder");
                        VERIFY_LOG(done->ok());
                    });
                });
            });
        });
    }
    {
        // test: create a bookmark in Bookmarks Bar, then update it to move it to the root level, then delete it
        orion::UpdateBookmarkQuery query{};
        query.id = orion::BookmarkId(161616511); // random not existing folder id
        core_api->update_bookmark(query, [done,core_api](orion::Error err) {
            if (err.code() == orion::ErrorCode::not_found) {
                VERIFY_LOG(done->ok());
                return;
            }
            DONE_ASSERT(
                done,
                err.code() == orion::ErrorCode::not_found,
                "update of non-existing bookmark should fail"
            );
        });

        core_api->get_bookmark_folder_id_by_type(profile_id, orion::BookmarkFolderType::bookmarks_bar,
        [profile_id,done,core_api](orion::GetBookmarkFolderIdByTypeReply reply) {
            DONE_CHECK(reply.err, done);
            orion::CreateBookmarkQuery query{};
            query.profile_id = profile_id;
            query.name = "Test Bookmark in Bookmarks Bar Folder";
            query.parent_folder_id = reply.folder_id.value; // create inside the Bookmarks Bar folder
            core_api->create_bookmark(query, [done,core_api](orion::CreateBookmarkReply reply) {
                DONE_CHECK(reply.err, done);
                log_verbatim("created bookmark in Bookmarks Bar");

                orion::UpdateBookmarkQuery update_query{};
                update_query.id = reply.bookmark_id;
                update_query.name = "Test Bookmark in Bookmarks Bar Folder (moved to root)";
                update_query.parent_folder_id = BookmarkFolderId(0); // move to root level
                update_query.update_name = true;
                update_query.update_parent_folder_id = true;
                core_api->update_bookmark(update_query,
                [bookmark_id=reply.bookmark_id,done,core_api](orion::Error err) {
                    DONE_CHECK(err, done);
                    log_verbatim("updated bookmark to move it to root level");

                    core_api->delete_bookmarks({bookmark_id}, [done,core_api](orion::Error err) {
                        DONE_CHECK(err, done);
                        log_verbatim("deleted bookmark folder");
                        VERIFY_LOG(done->ok());
                    });
                });
            });
        });
    }
    {
        GetRecentBookmarkIdsQuery query{};
        query.profile_id = profile_id;
        core_api->get_recent_bookmark_ids(query, [done,core_api](orion::GetRecentBookmarkIdsReply reply) {
            DONE_CHECK(reply.err, done);
            log_verbatim("got %zu recent bookmark ids", reply.ids.size());

            VERIFY_LOG(done->ok());
        });
    }
}

void
test_history_entries(CoreApi* core_api, ProfileId profile_id, std::shared_ptr<Done> done_all)
{
    auto observer_id = core_api->history_entry_days_change().add_observer(
    [](const orion::HistoryEntryDaysChange& change) {
        log_verbatim("history entry days changed: created %zu, updated %zu, deleted %zu",
            change.created.size(), change.updated.size(), change.deleted.size()
        );
    });

    auto done = std::make_shared<Done>(2, core_api, [done_all,observer_id,core_api](orion::Error err) {
        DONE_CHECK(err, done_all);
        core_api->history_entry_days_change().remove_observer(observer_id);
        log_verbatim("test_history_entries done");
        VERIFY_LOG(done_all->ok());
    });

    orion::CreateHistoryEntryQuery query;
    query.profile_id = profile_id;
    query.address = "kagi.com";
    query.website = "Kagi";
    query.visited_at = static_cast<UnixTimeUtc64>(1751211808); // 2025-06-29 15:43:28 UTC

    core_api->create_history_entry(query, [done,core_api](orion::CreateHistoryEntryReply reply){
        DONE_CHECK(reply.err, done);
        core_api->delete_history_entries({reply.id}, [done,core_api](orion::Error err) {
            DONE_CHECK(err, done);
            log_verbatim("deleted history entry");
            VERIFY_LOG(done->ok());
        });
    });

    {
        GetHistoryEntryIdsQuery query{};
        query.profile_id = profile_id;
        query.visited_after = UnixTimeUtc64(1751241600);
        query.visited_before = UnixTimeUtc64(1751241601);
        core_api->get_history_entry_ids(query, [done,core_api](orion::GetHistoryEntryIdsReply reply) {
            DONE_CHECK(reply.err, done);
            log_verbatim("got %zu history entry ids", reply.ids.size());
            core_api->get_history_entries(reply.ids, [done,core_api](orion::GetHistoryEntriesReply reply) {
                DONE_CHECK(reply.err, done);
                log_verbatim("got %zu history entries", reply.entries.size());
                VERIFY_LOG(done->ok());
            });
        });
    }
}

// Returns YYYY-MM-DD string (in local time) for the given unix timestamp
static std::string
unix_timestamp_to_local_date_string(UnixTimeUtc64 timestamp)
{
    std::time_t time = static_cast<std::time_t>(timestamp);
    std::tm local_tm = *std::localtime(&time);
    std::ostringstream oss;
    oss << std::put_time(&local_tm, "%Y-%m-%d");
    return oss.str();
}

void
test_profiles(CoreApi* core_api, ProfileId profile_id, std::shared_ptr<Done> done_all)
{
    auto done = std::make_shared<Done>(2, core_api, [done_all](orion::Error err) {
        DONE_CHECK(err, done_all);
        log_verbatim("test_profiles done");
        VERIFY_LOG(done_all->ok());
    });

    {
        core_api->get_default_profile_id([done,core_api](orion::GetDefaultProfileIdReply reply) {
            DONE_CHECK(reply.err, done);

            GetNonemptyHistoryDaysQuery query{
                .profile_id = reply.profile_id,
            };

            core_api->get_nonempty_history_days(query, [done,core_api](orion::GetNonemptyHistoryDaysReply reply) {
                DONE_CHECK(reply.err, done);
                log_verbatim("got %zu non-empty history days", reply.days.size());
                for (const auto& day : reply.days) {
                    // Print start of day in local time as YYYY-MM-DD
                    std::string date_str = unix_timestamp_to_local_date_string(day.start_of_day);
                    log_verbatim("day: %s", date_str.c_str());
                }
                VERIFY_LOG(done->ok());
            });
        });
    }
#if ENABLE_DEFAULT_PROFILE_CHANGE_TEST
    {
        core_api->get_default_profile_id(
        [done,profile_id,core_api](orion::GetDefaultProfileIdReply reply) {
            DONE_CHECK(reply.err, done);
            auto original_profile_id = reply.profile_id.value();
            core_api->set_default_profile(profile_id,
            [original_profile_id,profile_id,done,core_api](orion::Error err) {
                DONE_CHECK(err, done);
                log_verbatim("set default profile from %lld to %lld", original_profile_id.value(), profile_id.value());
                core_api->get_default_profile_id(
                [original_profile_id,done,core_api](orion::GetDefaultProfileIdReply reply) {
                    DONE_CHECK(reply.err, done);
                    log_verbatim("default profile id is %lld", reply.profile_id->value());
                    core_api->set_default_profile(original_profile_id,
                    [original_profile_id,done,core_api](orion::Error err) {
                        DONE_CHECK(err, done);
                        log_verbatim("set default profile back to %lld", original_profile_id.value());
                        VERIFY_LOG(done->ok());
                    });
                });
            });
        });
    }
#else
    VERIFY_LOG(done->ok());
#endif // ENABLE_DEFAULT_PROFILE_CHANGE_TEST
}

void
test_profiles_update(CoreApi* core_api, ProfileId profile_id, std::shared_ptr<Done> done_all)
{    
    auto observer_id = core_api->profiles_change().add_observer([](const ProfilesChange& change) {
        log_verbatim("profiles changed: created %zu, updated %zu, deleted %zu",
            change.created.size(), change.updated.size(), change.deleted.size()
        );
    });

    auto done = std::make_shared<Done>(1, core_api, [done_all,observer_id,core_api](orion::Error err) {
        DONE_CHECK(err, done_all);
        core_api->profiles_change().remove_observer(observer_id);
        log_verbatim("test_profiles_update done");
        VERIFY_LOG(done_all->ok());
    });

    orion::CreateProfileQuery query{
        .name = "Core API Test Profile",
        .is_test = true,
    };

    core_api->create_profile(query, [profile_id,done,core_api](orion::CreateProfileReply reply) {
        DONE_CHECK(reply.err, done);
        log_verbatim("created profile %lld", reply.id.value);

        core_api->get_default_profile_id(
        [profile_id,new_profile_id=reply.id,done,core_api](orion::GetDefaultProfileIdReply reply) {
            DONE_CHECK(reply.err, done);
            log_verbatim("default profile id is %lld", reply.profile_id.value);

#if ENABLE_DEFAULT_PROFILE_CHANGE_TEST
            core_api->set_default_profile(new_profile_id,
            [original_default_id=reply.profile_id,new_profile_id,done,core_api](orion::Error err) {
                DONE_CHECK(err, done);
                log_verbatim("set profile %lld as default", new_profile_id.value());
            
                orion::UpdateProfileQuery query{};
                query.id = new_profile_id;
                query.name = "Updated Profile Name";
                query.icon_name = "updated_icon";
                query.update_icon_name = true;
                core_api->update_profile(query, [original_default_id,new_profile_id,done,core_api](orion::Error err) {
                    DONE_CHECK(err, done);
                    log_verbatim("updated profile %lld", new_profile_id.value());

                    core_api->set_default_profile(original_default_id,
                    [original_default_id,new_profile_id,done,core_api](orion::Error err) {
                        DONE_CHECK(err, done);
                        log_verbatim("set profile %lld as default again", original_default_id.value());

                        orion::DeleteProfilesQuery query{
                            .ids = {new_profile_id},
                        };
                        core_api->delete_profiles(query, [done](orion::Error err) {
                            DONE_CHECK(err, done);
                            log_verbatim("deleted profile");
                            VERIFY_LOG(done->ok());
                        });
                    });
                });
            });
#else
            orion::DeleteProfilesQuery query{
                .ids = {new_profile_id},
            };
            core_api->delete_profiles(query, [done](orion::Error err) {
                DONE_CHECK(err, done);
                log_verbatim("deleted profile");
                VERIFY_LOG(done->ok());
            });
#endif // ENABLE_DEFAULT_PROFILE_CHANGE_TEST
        });
    });
}

void
test_reading_list_entries(CoreApi* core_api, ProfileId profile_id, std::shared_ptr<Done> done_all)
{
    auto done = std::make_shared<Done>(2, core_api, [done_all](orion::Error err) {
        DONE_CHECK(err, done_all);
        log_verbatim("test_reading_list_entries done");
        VERIFY_LOG(done_all->ok());
    });

    {
        orion::GetReadingListEntryIdsQuery query{};
        query.profile_id = profile_id;
        core_api->get_reading_list_entry_ids(query, [done,core_api](orion::GetReadingListEntryIdsReply reply) {
            DONE_CHECK(reply.err, done);
            log_verbatim("got %zu reading list entry ids", reply.ids.size());

            core_api->get_reading_list_entries(reply.ids, [done,core_api](orion::GetReadingListEntriesReply reply) {
                DONE_CHECK(reply.err, done);
                log_verbatim("got %zu reading list entries", reply.entries.size());
                VERIFY_LOG(done->ok());
            });
        });
    }
    {
        orion::CreateReadingListEntryQuery query{};
        query.profile_id = profile_id;
        query.title = "Test Reading List Entry";
        query.address = "https://example.com";
        query.referrer = "https://referrer.com";
        query.description = "This is a test reading list entry.";
        core_api->create_reading_list_entry(query, [done,core_api](orion::CreateReadingListEntryReply reply) {
            DONE_CHECK(reply.err, done);
            log_verbatim("created reading list entry %lld", reply.id.value);

            core_api->get_reading_list_entries({reply.id},
            [new_id=reply.id,done,core_api](orion::GetReadingListEntriesReply reply) {
                DONE_CHECK(reply.err, done);
                log_verbatim("got %zu reading list entries after creation", reply.entries.size());
                DONE_ASSERT(done, reply.entries.size() == 1, "should have exactly one reading list entry after creation");
                DONE_ASSERT(
                    done,
                    reply.entries.front().description == "This is a test reading list entry.",
                    "description should match"
                );

                UpdateReadingListEntryQuery query{};
                query.id = new_id;
                query.is_unread = false;
                query.is_saved_offline = true;
                query.referrer = "Updated referrer of test reading list entry";
                query.update_is_unread = true;
                query.update_is_saved_offline = true;
                query.update_referrer = true;
                core_api->update_reading_list_entry(query, [new_id,done,core_api](orion::Error err) {
                    DONE_CHECK(err, done);
                    log_verbatim("updated reading list entry %lld", new_id.value);

                    core_api->get_reading_list_entries({new_id},
                    [new_id,done,core_api](orion::GetReadingListEntriesReply reply) {
                        DONE_CHECK(reply.err, done);
                        log_verbatim("got %zu reading list entries after update", reply.entries.size());
                        DONE_ASSERT(done, reply.entries.size() == 1);
                        DONE_ASSERT(done, !reply.entries.front().is_unread);
                        DONE_ASSERT(done, reply.entries.front().is_saved_offline);
                        DONE_ASSERT(done, reply.entries.front().title == "Test Reading List Entry");
                        DONE_ASSERT(done, reply.entries.front().description == "This is a test reading list entry.");
                        DONE_ASSERT(done, reply.entries.front().referrer == "Updated referrer of test reading list entry");

                        core_api->delete_reading_list_entries({new_id}, [done](orion::Error err) {
                            DONE_CHECK(err, done);
                            log_verbatim("deleted reading list entry");
                            VERIFY_LOG(done->ok());
                        });
                    });
                });
            });
        });
    }
}

void
test_windows_and_tabs(CoreApi* core_api, ProfileId profile_id, std::shared_ptr<Done> done_all)
{
    auto window_observer_id = core_api->windows_change().add_observer([](const WindowsChange& change) {
        log_verbatim("windows changed: created %zu, updated %zu, deleted %zu",
            change.created.size(), change.updated.size(), change.deleted.size()
        );
    });

    auto tab_observer_id = core_api->tabs_change().add_observer([](const TabsChange& change) {
        log_verbatim("tabs changed: created %zu, updated %zu, deleted %zu, updated_name %d",
            change.created.size(), change.updated.size(), change.deleted.size(), change.updated_name
        );
    });

    auto done = std::make_shared<Done>(2, core_api,
    [done_all,window_observer_id,tab_observer_id,core_api](orion::Error err) {
        DONE_CHECK(err, done_all);
        core_api->windows_change().remove_observer(window_observer_id);
        core_api->tabs_change().remove_observer(tab_observer_id);
        log_verbatim("test_windows_and_tabs done");
        VERIFY_LOG(done_all->ok());
    });

    {
        core_api->get_window_ids({.profile_id=profile_id}, [profile_id,done](orion::GetWindowIdsReply reply) {
            DONE_CHECK(reply.err, done);
            log_verbatim("got %zu window ids", reply.ids.size());
            DONE_ASSERT(done, reply.ids.empty(), "windows should be empty for new profile");
            VERIFY_LOG(done->ok());
        });
    }
    {
        // test cascade delete of tabs in a window
        CreateWindowQuery query{};
        query.profile_id = profile_id;
        query.name = "Test Window";
        query.icon_name = "test_icon";
        query.keyboard_shortcut = "Ctrl+Shift+T";
        query.is_suspended = false;
        query.is_tab_group = false;
        log_verbose("create_window: profile_id=%lld name='%s'", query.profile_id.value, query.name.c_str());
        // leave seq_num unset to append it at the end
        core_api->create_window(query, [done,core_api](orion::CreateWindowReply reply) {
            DONE_CHECK(reply.err, done);
            log_verbose("created window: id=%lld", reply.id.value);

            core_api->get_windows({reply.id}, [window_id=reply.id,done,core_api](orion::GetWindowsReply reply) {
                DONE_CHECK(reply.err, done);
                log_verbose("got %zu windows", reply.windows.size());
                DONE_ASSERT(done, reply.windows.size() == 1);
                DONE_ASSERT(done, reply.windows.front().name == "Test Window");

                UpdateWindowQuery query{};
                query.id = window_id;
                query.name = "Updated Test Window";
                query.selected_tab_id = TabId(0);
                core_api->update_window(query, [window_id,done,core_api](orion::Error err) {
                    DONE_CHECK(err, done);
                    log_verbatim("updated window %lld", window_id.value);

                    CreateTabQuery query{};
                    query.window_id = window_id;
                    query.name = "Test Tab in Window";
                    query.icon_name = "test_icon";
                    query.is_pinned = false;
                    query.is_suspended = false;
                    query.uri = "https://example.com";
                    query.session_state = {};
                    // leave seq_num unset to append it at the end
                    core_api->create_tab(query, [window_id,done,core_api](orion::CreateTabReply reply) {
                        DONE_CHECK(reply.err, done);
                        log_verbatim("created tab %lld in window %lld", reply.id.value, reply.id.value);

                        UpdateTabQuery query{};
                        query.id = reply.id;
                        query.name = "Updated Test Tab in Window";
                        query.update_name = true;
                        core_api->update_tab(query, [window_id,done,tab_id=reply.id,core_api](orion::Error err) {
                            DONE_CHECK(err, done);
                            log_verbatim("updated tab %lld in window %lld", tab_id.value, window_id.value);

                            core_api->get_tabs({tab_id}, [window_id,done,core_api](orion::GetTabsReply reply) {
                                DONE_CHECK(reply.err, done);
                                log_verbatim("got %zu tabs in window %lld", reply.tabs.size(), window_id.value);
                                DONE_ASSERT(done, reply.tabs.size() == 1);
                                DONE_ASSERT(done, reply.tabs.front().name == "Updated Test Tab in Window");

                                CreateTabQuery query{};
                                query.window_id = window_id;
                                query.name = "Test Tab 2 in Window";
                                // leave seq_num unset to append it at the end
                                core_api->create_tab(query, [window_id,done,core_api](orion::CreateTabReply reply) {
                                    DONE_CHECK(reply.err, done);
                                    log_verbatim("created tab %lld in window %lld", reply.id.value, window_id.value);
                                    
                                    core_api->delete_windows({window_id}, [window_id,done,core_api](orion::Error err) {
                                        DONE_CHECK(err, done);
                                        log_verbatim("deleted window %lld", window_id.value);
                                        VERIFY_LOG(done->ok());
                                    });
                                });
                            });
                        });
                    });
                });
            });
        });
    }
}

void
test_cascade_delete_of_bookmarks_in_folders(CoreApi* core_api, ProfileId profile_id, std::shared_ptr<Done> done_all)
{
    auto folder_id = std::make_shared<BookmarkFolderId>();
    auto bookmark_id = std::make_shared<BookmarkId>();

    auto folder_observer_id = core_api->bookmark_folders_change().add_observer(
    [folder_id](const BookmarkFoldersChange& change) {
        if (!change.deleted.empty() && change.deleted.front() == *folder_id) {
            log_verbatim("bookmark folder %lld deleted", folder_id->value);
        }
    });

    auto bookmark_observer_id = core_api->bookmarks_change().add_observer(
    [bookmark_id](const BookmarksChange& change) {
        if (!change.deleted.empty() && change.deleted.front() == *bookmark_id) {
            log_verbatim("bookmark %lld deleted", bookmark_id->value);
        }
    });

    auto done = std::make_shared<Done>(1, core_api,
    [done_all,bookmark_observer_id,folder_observer_id,core_api](orion::Error err) {
        DONE_CHECK(err, done_all);
        core_api->bookmarks_change().remove_observer(bookmark_observer_id);
        core_api->bookmark_folders_change().remove_observer(folder_observer_id);
        log_verbatim("test_cascade_delete_of_bookmarks_in_folders done");
        VERIFY_LOG(done_all->ok());
    });

    CreateBookmarkFolderQuery query{};
    query.profile_id = profile_id;
    query.name = "Test Folder for Delete";
    query.parent_folder_id = BookmarkFolderId(0); // create at the root level
    core_api->create_bookmark_folder(query,
    [profile_id,done,folder_id,bookmark_id,core_api](orion::CreateBookmarkFolderReply reply) {
        DONE_CHECK(reply.err, done);
        log_verbatim("created bookmark folder %lld in profile %lld", reply.folder_id.value, profile_id.value);
        *folder_id = reply.folder_id.value;

        orion::CreateBookmarkQuery bookmark_query{};
        bookmark_query.profile_id = profile_id;
        bookmark_query.name = "Test Bookmark in Folder";
        bookmark_query.address = "https://example.com";
        bookmark_query.invocation_keyword = "example";
        bookmark_query.parent_folder_id = reply.folder_id; // create inside the folder
        core_api->create_bookmark(bookmark_query,
        [profile_id,folder_id=reply.folder_id,bookmark_id,done,core_api](orion::CreateBookmarkReply reply) {
            DONE_CHECK(reply.err, done);
            log_verbatim("created bookmark %lld in folder %lld", reply.bookmark_id.value, folder_id.value);
            *bookmark_id = reply.bookmark_id.value;

            core_api->delete_bookmark_folders({folder_id}, [profile_id,folder_id,done,core_api](orion::Error err) {
                DONE_CHECK(err, done);
                log_verbatim("deleted bookmark folder %lld and its bookmarks", folder_id.value);
                VERIFY_LOG(done->ok());
            });
        });
    });
}

void
test_cascade_delete_of_bookmark_folders(CoreApi* core_api, ProfileId profile_id, std::shared_ptr<Done> done_all)
{
    auto done = std::make_shared<Done>(1, core_api, [done_all](orion::Error err) {
        DONE_CHECK(err, done_all);
        log_verbatim("test_cascade_delete_of_bookmark_folders done");
        VERIFY_LOG(done_all->ok());
    });

    // test cascade delete of bookmark folders in profile
    CreateProfileQuery query{
        .name = "Test Profile for Bookmark Folder Delete",
        .icon_name = "test_icon",
        .keyboard_shortcut = "Ctrl+Shift+P",
        .is_test = true,
    };

    core_api->create_profile(query, [done,core_api](orion::CreateProfileReply reply) {
        DONE_CHECK(reply.err, done);
        log_verbatim("created profile %lld", reply.id.value);
        CreateBookmarkFolderQuery folder_query{};
        folder_query.profile_id = reply.id;
        folder_query.name = "Test Folder for Delete";
        folder_query.parent_folder_id = BookmarkFolderId(0); // create at the root level
        core_api->create_bookmark_folder(folder_query,
        [profile_id=reply.id,done,core_api](orion::CreateBookmarkFolderReply folder_reply) {
            DONE_CHECK(folder_reply.err, done);
            log_verbatim(
                "created bookmark folder %lld in profile %lld",
                folder_reply.folder_id.value, profile_id.value
            );
            orion::DeleteProfilesQuery query{
                .ids = {profile_id},
            };
            core_api->delete_profiles(query, [profile_id,done,core_api](orion::Error err) {
                DONE_CHECK(err, done);
                log_verbatim("deleted profile %lld and its bookmark folders", profile_id.value);
                core_api->get_bookmark_folder_ids(profile_id,
                [done,core_api](orion::GetBookmarkFolderIdsReply reply) {
                    DONE_CHECK(reply.err, done);
                    log_verbatim("got %zu bookmark folder ids after profile delete", reply.ids.size());
                    if (!reply.ids.empty()) {
                        DONE_CHECK(
                            MAKE_ERROR(
                                orion::ErrorCode::assert,
                                "bookmark folders should be deleted when profile is deleted"
                            ),
                            done
                        );
                    }
                    VERIFY_LOG(done->ok());
                });
            });
        });
    });
}

void
test_settings(CoreApi* core_api, ProfileId profile_id, std::shared_ptr<Done> done_all)
{
    auto settings_observer_id = core_api->settings_change().add_observer([](const SettingsChange& change) {
        for (const auto& setting : change.updated) {
            (void)setting; // suppress unused variable warning
            log_verbatim(
                "setting changed: profile_id=%lld key=%s type=%s default_value=%s old_value=%s new_value=%s",
                setting.profile_id.value,
                setting.key.c_str(),
                setting.type.c_str(),
                setting.default_value.c_str(),
                setting.old_value.c_str(),
                setting.new_value.c_str()
            );
        }
    });

    auto single_setting_observer_id = core_api->add_single_setting_change_observer(
        orion::settings::test::subrealm::size,
    [](SingleSettingChange<int64_t> change) {
        log_verbatim(
            "single setting changed: profile_id=%lld key=%s default_value=%lld old_value=%lld new_value=%lld",
            change.profile_id.value,
            change.key.c_str(),
            change.default_value,
            change.old_value,
            change.new_value
        );
    });
    
    auto done = std::make_shared<Done>(8, core_api,
    [done_all,settings_observer_id,single_setting_observer_id,core_api](orion::Error err) {
        DONE_CHECK(err, done_all);
        core_api->settings_change().remove_observer(settings_observer_id);
        core_api->single_setting_change().remove_observer(single_setting_observer_id);
        log_verbatim("test_settings done");
        VERIFY_LOG(done_all->ok());
    });

    core_api->set_setting(profile_id, orion::settings::test::subrealm::size, 42,
    [profile_id,done,core_api](orion::SetSettingReply<int64_t> reply) {
        DONE_CHECK(reply.err, done);
        log_verbatim(
            "set setting: key=%s type=%s default_value=%lld old_value=%lld new_value=%lld",
            reply.setting->key().c_str(),
            reply.setting->type().c_str(),
            reply.default_value,
            reply.old_value,
            reply.new_value
        );
        core_api->get_setting(profile_id, orion::settings::test::subrealm::size,
        [profile_id,done,core_api](orion::GetSettingReply<int64_t> reply) {
            DONE_CHECK(reply.err, done);
            log_verbatim(
                "get setting: key=%s type=%s default_value=%lld value=%lld",
                reply.setting->key().c_str(),
                reply.setting->type().c_str(),
                reply.default_value,
                reply.value
            );
            DONE_ASSERT(done, reply.value == 42, "setting value should be 42");

            core_api->reset_setting(profile_id, orion::settings::test::subrealm::size,
            [profile_id,done,core_api](auto reply) {
                DONE_CHECK(reply.err, done);
                log_verbatim(
                    "reset setting: key=%s type=%s default_value=%lld old_value=%lld new_value=%lld",
                    reply.setting->key().c_str(),
                    reply.setting->type().c_str(),
                    reply.default_value,
                    reply.old_value,
                    reply.new_value
                );
                core_api->get_setting(profile_id, orion::settings::test::subrealm::size,
                [done](orion::GetSettingReply<int64_t> reply) {
                    DONE_CHECK(reply.err, done);
                    log_verbatim(
                        "get setting: key=%s type=%s default_value=%lld value=%lld",
                        reply.setting->key().c_str(),
                        reply.setting->type().c_str(),
                        reply.default_value,
                        reply.value
                    );
                    DONE_ASSERT(
                        done,
                        reply.value == orion::settings::test::subrealm::size.default_value(),
                        "setting value should be reset to default"
                    );
                    VERIFY_LOG(done->ok());
                });
            });
        });
    });

    core_api->set_setting(profile_id, orion::settings::test::subrealm::family, "DejaVu",
    [profile_id,done,core_api](auto reply) {
        DONE_CHECK(reply.err, done);
        log_verbatim(
            "set setting: key=%s type=%s default_value=%s old_value=%s new_value=%s",
            reply.setting->key().c_str(),
            reply.setting->type().c_str(),
            reply.default_value.c_str(),
            reply.old_value.c_str(),
            reply.new_value.c_str()
        );
        core_api->get_setting(profile_id, orion::settings::test::subrealm::family, [done](auto reply) {
            DONE_CHECK(reply.err, done);
            log_verbatim(
                "get setting: key=%s type=%s default_value=%s value=%s",
                reply.setting->key().c_str(),
                reply.setting->type().c_str(),
                reply.default_value.c_str(),
                reply.value.c_str()
            );
            DONE_ASSERT(done, reply.value == "DejaVu", "setting value should be 'DejaVu'");
            VERIFY_LOG(done->ok());
        });
    });

    core_api->set_setting(profile_id, orion::settings::test::progress, 0.75,
    [profile_id,done,core_api](auto reply) {
        DONE_CHECK(reply.err, done);
        log_verbatim(
            "set setting: key=%s type=%s default_value=%f old_value=%f new_value=%f",
            reply.setting->key().c_str(),
            reply.setting->type().c_str(),
            reply.default_value,
            reply.old_value,
            reply.new_value
        );
        core_api->get_setting(profile_id, orion::settings::test::progress,
        [done](orion::GetSettingReply<double> reply) {
            DONE_CHECK(reply.err, done);
            log_verbatim(
                "get setting: key=%s type=%s default_value=%f value=%f",
                reply.setting->key().c_str(),
                reply.setting->type().c_str(),
                reply.default_value,
                reply.value
            );
            DONE_ASSERT(done, reply.value == 0.75, "setting value should be 0.75");
            VERIFY_LOG(done->ok());
        });
    });

    core_api->set_setting(
        profile_id, orion::settings::general::app_opens_with, orion::settings::AppOpensWith::new_private_window,
    [profile_id,done,core_api](auto reply) {
        DONE_CHECK(reply.err, done);
        log_verbatim(
            "set setting: key=%s type=%s default_value=%lld old_value=%lld new_value=%lld",
            reply.setting->key().c_str(),
            reply.setting->type().c_str(),
            reply.default_value,
            reply.old_value,
            reply.new_value
        );
        core_api->get_setting(profile_id, orion::settings::general::app_opens_with,
        [done](orion::GetSettingReply<orion::settings::AppOpensWith> reply) {
            DONE_CHECK(reply.err, done);
            log_verbatim(
                "get setting: key=%s type=%s default_value=%lld value=%lld",
                reply.setting->key().c_str(),
                reply.setting->type().c_str(),
                reply.default_value,
                reply.value
            );
            DONE_ASSERT(
                done,
                reply.value == orion::settings::AppOpensWith::new_private_window,
                "setting value should be new_private_window"
            );
            VERIFY_LOG(done->ok());
        });
    });

    core_api->set_setting(
        profile_id, orion::settings::general::new_windows_open_with, orion::settings::NewWindowsOpenWith::same_page,
    [profile_id,done,core_api](auto reply) {
        DONE_CHECK(reply.err, done);
        log_verbatim(
            "set setting: key=%s type=%s default_value=%lld old_value=%lld new_value=%lld",
            reply.setting->key().c_str(),
            reply.setting->type().c_str(),
            reply.default_value,
            reply.old_value,
            reply.new_value
        );
        core_api->get_setting(profile_id, orion::settings::general::new_windows_open_with,
        [done](orion::GetSettingReply<orion::settings::NewWindowsOpenWith> reply) {
            DONE_CHECK(reply.err, done);
            log_verbatim(
                "get setting: key=%s type=%s default_value=%lld value=%lld",
                reply.setting->key().c_str(),
                reply.setting->type().c_str(),
                reply.default_value,
                reply.value
            );
            DONE_ASSERT(
                done, 
                reply.value == orion::settings::NewWindowsOpenWith::same_page,
                "setting value should be same_page"
            );
            VERIFY_LOG(done->ok());
        });
    });

    core_api->set_setting(profile_id, orion::settings::test::show_line_numbers, true,
    [profile_id,done,core_api](auto reply) {
        DONE_CHECK(reply.err, done);
        log_verbatim(
            "set setting: key=%s type=%s default_value=%lld old_value=%lld new_value=%lld",
            reply.setting->key().c_str(),
            reply.setting->type().c_str(),
            reply.default_value,
            reply.old_value,
            reply.new_value
        );
        core_api->get_setting(profile_id, orion::settings::test::show_line_numbers,
        [done](auto reply) {
            DONE_CHECK(reply.err, done);
            log_verbatim(
                "get setting: key=%s type=%s default_value=%lld value=%lld",
                reply.setting->key().c_str(),
                reply.setting->type().c_str(),
                reply.default_value,
                reply.value
            );
            DONE_ASSERT(done, reply.value, "setting value should be true");
            VERIFY_LOG(done->ok());
        });
    });

    core_api->set_setting(profile_id, orion::settings::test::updated_at, UnixTimeUtc64(111),
    [profile_id,done,core_api](SetSettingReply<UnixTimeUtc64> reply) {
        DONE_CHECK(reply.err, done);
        log_verbatim(
            "set setting: key=%s type=%s default_value=%lld old_value=%lld new_value=%lld",
            reply.setting->key().c_str(),
            reply.setting->type().c_str(),
            reply.default_value.value,
            reply.old_value.value,
            reply.new_value.value
        );
        core_api->get_setting(profile_id, orion::settings::test::updated_at, [done](auto reply) {
            DONE_CHECK(reply.err, done);
            log_verbatim(
                "get setting: key=%s type=%s default_value=%lld value=%lld",
                reply.setting->key().c_str(),
                reply.setting->type().c_str(),
                reply.default_value.value,
                reply.value.value
            );
            DONE_ASSERT(done, reply.value == UnixTimeUtc64(111), "setting value should be 111");
            VERIFY_LOG(done->ok());
        });
    });

    core_api->set_setting(profile_id, orion::settings::test::font_size, orion::FontSize(17),
    [profile_id,done,core_api](auto reply) {
        DONE_CHECK(reply.err, done);
        log_verbatim(
            "set setting: key=%s type=%s default_value=%lld old_value=%lld new_value=%lld",
            reply.setting->key().c_str(),
            reply.setting->type().c_str(),
            reply.default_value,
            reply.old_value,
            reply.new_value
        );
        core_api->get_setting(profile_id, orion::settings::test::font_size, [done](auto reply) {
            DONE_CHECK(reply.err, done);
            log_verbatim("get setting: key=%s type=%s default_value=%lld value=%lld",
                reply.setting->key().c_str(), reply.setting->type().c_str(), reply.default_value, reply.value
            );
            DONE_ASSERT(done, reply.value == orion::FontSize(17), "setting value should be FontSize(17)");
            VERIFY_LOG(done->ok());
        });
    });
}

void
test_setting_introspection(CoreApi* core_api, ProfileId profile_id, std::shared_ptr<Done> done)
{
    auto inspect_setting = [](const settings::ISetting* s) {
        log_verbatim("introspect setting: key=%s type=%s default_value=%s",
            s->key().c_str(), s->type().c_str(), s->default_value_as_string().c_str()
        );
    };

    log_verbatim(
        "introspect setting: realm=%s key_count=%d",
        orion::settings::test::realm_name().c_str(),
        orion::settings::test::all().size()
    );
    DONE_ASSERT(done, orion::settings::test::all().size() == 5, "test realm should have 5 settings");

    log_verbatim(
        "introspect setting: realm=%s key_count=%d",
        orion::settings::test::subrealm::realm_name().c_str(),
        orion::settings::test::subrealm::all().size()
    );
    DONE_ASSERT(done, orion::settings::test::subrealm::all().size() == 2, "test subrealm should have 2 settings");

    log_verbatim(
        "introspect setting: realm=%s key_count=%d",
        orion::settings::general::realm_name().c_str(),
        orion::settings::general::all().size()
    );

    for (const auto s : orion::settings::test::all()) {
        inspect_setting(s);
    }

    for (const auto s : orion::settings::test::subrealm::all()) {
        inspect_setting(s);
    }

    for (const auto s : orion::settings::general::all()) {
        inspect_setting(s);
    }
    VERIFY_LOG(done->ok());
}

void
test_callback_and_notify_order(CoreApi* core_api, ProfileId profile_id, std::shared_ptr<Done> done_all)
{
    std::string test_name = "Test Callback Bookmark #1";
    auto bookmark_id = std::make_shared<BookmarkId>();

    auto observer_id = core_api->bookmarks_change().add_observer(
    [test_name,bookmark_id](const BookmarksChange& change) {
        {
            auto it = std::find_if(change.created.begin(), change.created.end(),
            [test_name,change](const orion::BookmarkDto& b) {
                return b.name == test_name;
            });
            if (it != change.created.end()) {
                log_verbatim("%s: expected order: 3 (create notify)", test_name.c_str());
            }
        }
        {
            auto it = std::find_if(change.deleted.begin(), change.deleted.end(),
            [bookmark_id,change](const orion::BookmarkId& id) {
                return id == *bookmark_id;
            });
            if (it != change.deleted.end()) {
                log_verbatim("%s: expected order: 5 (delete notify)", test_name.c_str());
            }
        }
    });

    auto done = std::make_shared<Done>(1, core_api, [done_all,observer_id,core_api](orion::Error err) {
        DONE_CHECK(err, done_all);
        core_api->bookmarks_change().remove_observer(observer_id);
        log_verbatim("test_callback_and_notify_order done");
        VERIFY_LOG(done_all->ok());
    });

    CreateBookmarkQuery query{};
    query.profile_id = profile_id;
    query.name = test_name;
    core_api->create_bookmark(query,
    [profile_id,test_name,bookmark_id,done,core_api](orion::CreateBookmarkReply reply) {
        DONE_CHECK(reply.err, done);
        *bookmark_id = reply.bookmark_id.value;
        log_verbatim("%s: expected order: 2 (create callback)", test_name.c_str());
        core_api->delete_bookmarks({reply.bookmark_id}, [test_name,done,core_api](orion::Error err) {
            DONE_CHECK(err, done);
            log_verbatim("%s: expected order: 4 (delete callback)", test_name.c_str());
            VERIFY_LOG(done->ok());
        });
    });
    log_verbatim("%s: expected order: 1 (create dispatch)", test_name.c_str());
}

void
test_extensions(CoreApi* core_api, ProfileId profile_id, std::shared_ptr<Done> done_all)
{
    auto observer_id = core_api->extensions_change().add_observer([](const ExtensionsChange& change) {
        log_verbatim("extensions changed: created %zu, updated %zu, deleted %zu",
            change.created.size(), change.updated.size(), change.deleted.size()
        );
    });

    auto done = std::make_shared<Done>(1, core_api, [done_all,observer_id,core_api](Error err) {
        DONE_CHECK(err, done_all);
        core_api->extensions_change().remove_observer(observer_id);
        log_verbatim("test_extensions done");
        VERIFY_LOG(done_all->ok());
    });

    InstallExtensionQuery query{};
    query.profile_id = profile_id;
    query.sku = orion::ExtensionSku("com.kagi.orion:firefox:test-extension");
    query.type = orion::ExtensionType::firefox;

    core_api->install_extension(query,
    [profile_id,done,sku=query.sku.value,core_api](orion::InstallExtensionReply reply) {
        DONE_CHECK(reply.err, done);
        log_verbatim("installed extension %s for profile %lld", sku.c_str(), profile_id.value);

        InstallExtensionQuery query{};
        query.profile_id = profile_id;
        query.sku = orion::ExtensionSku("com.kagi.orion:chrome:test-extension-2");
        query.type = orion::ExtensionType::chrome;

        core_api->install_extension(query, [profile_id,sku,done,core_api](orion::InstallExtensionReply reply) {
            DONE_CHECK(reply.err, done);
            log_verbatim("installed extension %s for profile %lld", sku.c_str(), profile_id.value);

            UpdateExtensionQuery query{};
            query.id = reply.id;
            
            query.version = "1.0.1";
            query.has_options = true;

            query.update_version = true;
            query.update_has_options = true;

            core_api->update_extension(query,
            [profile_id,sku,done,core_api](orion::UpdateExtensionReply reply) {
                DONE_CHECK(reply.err, done);
                log_verbatim("updated extension %s for profile %lld",
                    sku.c_str(), profile_id.value
                );
            
                GetInstalledExtensionIdsQuery query{};
                query.profile_id = profile_id;
                core_api->get_installed_extension_ids(query,
                [done,profile_id,sku,core_api](orion::GetAllInstalledExtensionIdsReply reply) {
                    DONE_CHECK(reply.err, done);
                    log_verbatim("got %zu installed extension ids", reply.ids.size());

                    DONE_ASSERT(done, reply.ids.size() >= 2); // storage_areas extension may also be installed
                    ExtensionId id = reply.ids.front();
                    DONE_ASSERT(done, std::find(reply.ids.begin(), reply.ids.end(), id) != reply.ids.end());

                    GetInstalledExtensionsQuery query{};
                    query.ids = reply.ids;

                    core_api->get_installed_extensions(query,
                    [done,profile_id,sku,id,core_api](orion::GetInstalledExtensionsReply reply) {
                        DONE_CHECK(reply.err, done);
                        log_verbatim("got %zu installed extensions", reply.extensions.size());

                        // storage_areas extension may also be installed
                        DONE_ASSERT(done, reply.extensions.size() >= 2);

                        DONE_ASSERT(
                            done,
                            reply.extensions.front().sku == orion::ExtensionSku("com.kagi.orion:chrome:test-extension-2")
                        );
                        DONE_ASSERT(done, reply.extensions.front().version == "1.0.1");
                        DONE_ASSERT(done, reply.extensions.front().has_options);

                        UninstallExtensionQuery query{};
                        query.id = id;

                        core_api->uninstall_extension(query,
                        [done,profile_id,sku,id,core_api](orion::UninstallExtensionReply reply) {
                            DONE_CHECK(reply.err, done);
                            log_verbatim("uninstalled extension %s", id.value.c_str());

                            core_api->async([done] {
                                VERIFY_LOG(done->ok());
                            });
                        });
                    });
                });
            });
        });
    });
}

void
test_extension_storage_areas(CoreApi* core_api, ProfileId profile_id, std::shared_ptr<Done> done_all)
{
    auto done = std::make_shared<Done>(1, core_api, [done_all](orion::Error err) {
        DONE_CHECK(err, done_all);
        log_verbatim("test_extension_storage_areas done");
        VERIFY_LOG(done_all->ok());
    });

    InstallExtensionQuery query{
        .profile_id = profile_id,
        .sku = ExtensionSku("com.kagi.orion:firefox:text-extension-storage-areas"),
        .type = ExtensionType::firefox,
    };

    core_api->install_extension(query, [done,core_api](InstallExtensionReply reply){
        DONE_CHECK(reply.err, done);
        ExtensionId extension_id = reply.id;

        std::vector<ExtensionStorageAreas::KeyValue> kvs{
            {"test_key_2", "updated_test_value_2"},
            {"test_key_3", "test_value_3"}
        };
        core_api->extension_storage_areas_set(ExtensionStorageAreas::SetQuery{
            .extension_id = extension_id,
            .storage_id = "test_storage_area",
            .key_values = kvs,
        }, [=](ExtensionStorageAreas::MutationReply mutation_reply){
            DONE_CHECK(mutation_reply.err, done);

            std::vector<ExtensionStorageAreas::KeyValue> kvs{
                {"test_key_1", "test_value_1"},
                {"test_key_2", "test_value_2"}
            };
            core_api->extension_storage_areas_get(ExtensionStorageAreas::GetQuery{
                .extension_id = extension_id,
                .storage_id = "test_storage_area",
                .key_values = kvs,
            }, [=](ExtensionStorageAreas::GetReply reply){
                DONE_CHECK(reply.err, done);

                DONE_ASSERT(done, reply.key_values.size() == 2);
                DONE_ASSERT(done, reply.key_values[0].key == "test_key_1");
                DONE_ASSERT(done, reply.key_values[0].value == "test_value_1");
                DONE_ASSERT(done, reply.key_values[1].key == "test_key_2");
                DONE_ASSERT(done, reply.key_values[1].value == "updated_test_value_2");

                core_api->extension_storage_areas_get_keys(ExtensionStorageAreas::GetKeysQuery{
                    .extension_id = extension_id,
                    .storage_id = "test_storage_area",
                }, [=](ExtensionStorageAreas::GetKeysReply reply){
                    DONE_CHECK(reply.err, done);

                    DONE_ASSERT(done, reply.keys.size() == 2);
                    DONE_ASSERT(
                        done, std::find(reply.keys.begin(), reply.keys.end(), "test_key_2") != reply.keys.end()
                    );
                    DONE_ASSERT(
                        done, std::find(reply.keys.begin(), reply.keys.end(), "test_key_3") != reply.keys.end()
                    );

                    core_api->extension_storage_areas_get_all(ExtensionStorageAreas::GetAllQuery{
                        .extension_id = extension_id,
                        .storage_id = "test_storage_area",
                    }, [=](ExtensionStorageAreas::GetAllReply reply){
                        DONE_CHECK(reply.err, done);

                        DONE_ASSERT(done, reply.key_values.size() == 2);
                        for (const auto& kv : reply.key_values) {
                            if (kv.key == "test_key_2") {
                                DONE_ASSERT(done, kv.value == "updated_test_value_2");
                            } else if (kv.key == "test_key_3") {
                                DONE_ASSERT(done, kv.value == "test_value_3");
                            } else {
                                DONE_ASSERT(done, false, "unexpected key in get_all result");
                            }
                        }

                        core_api->extension_storage_areas_remove(ExtensionStorageAreas::RemoveQuery{
                            .extension_id = extension_id,
                            .storage_id = "test_storage_area",
                            .keys = {"test_key_2"},
                        }, [=](ExtensionStorageAreas::MutationReply mutation_reply){
                            DONE_CHECK(mutation_reply.err, done);

                            core_api->extension_storage_areas_get_all(ExtensionStorageAreas::GetAllQuery{
                                .extension_id = extension_id,
                                .storage_id = "test_storage_area",
                            }, [=](ExtensionStorageAreas::GetAllReply reply){
                                DONE_CHECK(reply.err, done);

                                DONE_ASSERT(done, reply.key_values.size() == 1);
                                DONE_ASSERT(done, reply.key_values[0].key == "test_key_3");
                                DONE_ASSERT(done, reply.key_values[0].value == "test_value_3");

                                core_api->extension_storage_areas_clear(ExtensionStorageAreas::ClearQuery{
                                    .extension_id = extension_id,
                                    .storage_id = "test_storage_area",
                                }, [=](ExtensionStorageAreas::MutationReply mutation_reply){
                                    DONE_CHECK(mutation_reply.err, done);

                                    core_api->extension_storage_areas_get_all(ExtensionStorageAreas::GetAllQuery{
                                        .extension_id = extension_id,
                                        .storage_id = "test_storage_area",
                                    }, [=](ExtensionStorageAreas::GetAllReply reply){
                                        DONE_CHECK(reply.err, done);

                                        DONE_ASSERT(done, reply.key_values.empty(),
                                            "storage area should be empty after clear"
                                        );

                                        core_api->async([done] {
                                            VERIFY_LOG(done->ok()); // successful end of this test
                                        });
                                    });
                                });
                            });
                        });
                    });
                });
            });
        });
    });
}

class StringUtilsTester {

public:
    explicit StringUtilsTester(std::shared_ptr<Done> done)
        : m_done(std::move(done))
    {
    }

    void test_strip_diacritics() {
        test_strip_diacritics_empty_string();
        test_strip_diacritics_no_diacritics();
        test_strip_diacritics_only_diacritics();
        test_strip_diacritics_mixed_characters();
        test_strip_diacritics_unicode_characters();
    }

private:

    void assert_strip_diacritics(const std::string& input, const std::string& expected) {
        DONE_ASSERT(
            m_done,
            str::strip_diacritics(input) == expected,
            "strip_diacritics result should match expected value"
        );
    }

    void test_strip_diacritics_empty_string() {
        assert_strip_diacritics("", "");
    }

    void test_strip_diacritics_no_diacritics() {
        assert_strip_diacritics("Plain ASCII 123!", "Plain ASCII 123!");
    }

    void test_strip_diacritics_only_diacritics() {
        assert_strip_diacritics("a\xCC\x81" "e\xCC\x8Ci\xCC\x82", "aei");
    }

    void test_strip_diacritics_mixed_characters() {
        assert_strip_diacritics("P\xC5\x99\xC3\xADli\xC5\xA1 \xC5\xBElu\xC5\xA5ou\xC4\x8Dk\xC3\xBD k\xC5\xAF\xC5\x88", "Prilis zlutoucky kun");

    }

    void test_strip_diacritics_unicode_characters() {
        assert_strip_diacritics("\xE6\x9D\xB1\xE4\xBA\xAC \xF0\x9F\x98\x80", "\xE6\x9D\xB1\xE4\xBA\xAC \xF0\x9F\x98\x80");
    }

    std::shared_ptr<Done> m_done;
};

void
test_string_utils(CoreApi*, ProfileId, std::shared_ptr<Done> done)
{
    StringUtilsTester tester(done);
    tester.test_strip_diacritics();
    VERIFY_LOG(done->ok());
}

void
test_core_api(CoreApi* core_api, std::chrono::steady_clock::duration timeout, Cb<Error> done_cb)
{
#if ENABLE_CORE_API_TEST
    log_verbatim("running core api test");

    auto is_completed = std::make_shared<bool>(false);
    core_api->async_after(std::chrono::duration_cast<std::chrono::milliseconds>(timeout),
    [is_completed,timeout,done_cb]{
        if (!*is_completed) {
            // This can happen when the Done object(s) and their expect counts are not properly
            // matched with the number of operations performed.
            if (done_cb) {
                Error err = MAKE_ERROR(
                    ErrorCode::timeout,
                    "core api test did not complete in time (timeout %llds)",
                    std::chrono::duration_cast<std::chrono::seconds>(timeout).count()
                );
                done_cb(err);
            }
        }
    });

    // Store the original default profile ID to restore it later (if needed)
    core_api->get_default_profile_id([=](orion::GetDefaultProfileIdReply reply) {
        VERIFY_LOG(reply.err);
        log_verbatim("default profile id is %lld", reply.profile_id.value);
        auto original_default_profile_id = reply.profile_id;

        // Cleanup old test profiles that might remain if something went wrong during previous tests
        core_api->delete_test_profiles([=](orion::Error err) {
            VERIFY_LOG(err);
            log_verbatim("deleted old test profiles");

            CreateProfileQuery create_profile_query{
                .name = "Core API Test Profile #1",
                .icon_name = "icon_name_test",
                .keyboard_shortcut = "Ctrl+Shift+Alt+P",
                .is_test = true,
            };
            
            core_api->create_profile(create_profile_query, [=](orion::CreateProfileReply reply) {
                VERIFY_LOG(reply.err);
                log_verbatim("created profile %lld", reply.id.value);
                const auto& profile_id = reply.id;

                auto done = std::make_shared<Done>(core_api, [=](orion::Error err) {
                    VERIFY_LOG(err);
                    log_verbatim("core api test done: %s", err.to_string().c_str());
                    auto observer_id = core_api->profiles_change().add_observer([](const ProfilesChange& change) {
                        log_verbatim("profiles changed: created %zu, updated %zu, deleted %zu",
                            change.created.size(), change.updated.size(), change.deleted.size()
                        );
                    });

                    orion::DeleteProfilesQuery query{
                        .ids = {profile_id},
                    };
                    core_api->delete_profiles(query, [=](orion::Error err) {
                        VERIFY_LOG(err);
                        log_verbatim("deleted profile %lld", profile_id.value);

                        // Dispatch the observer removal asynchronously to ensure the notification
                        // about the deletion is processed (that it is not skipped). Currently,
                        // observer notifications are always dispatched synchronously only after
                        // the reply callback is called.
                        core_api->async([observer_id,core_api]{
                            core_api->profiles_change().remove_observer(observer_id);
                        });

                        core_api->get_default_profile_id([=](orion::GetDefaultProfileIdReply reply) {
                            VERIFY_LOG(reply.err);
                            
                            auto completion_lambda = [=]{
                                // We have a lot of async operations, so we want to dispatch this using async()
                                // just to ensure it is the last message that is logged
                                core_api->async([=]{
                                    *is_completed = true;

                                    if (done_cb) {
                                        done_cb(err);
                                    }

                                    // Cleanup old test profiles that might remain if something goes
                                    // wrong during the test
                                    core_api->delete_test_profiles([](orion::Error err) {
                                        VERIFY_LOG(err);
                                        log_verbatim("deleted old test profiles");
                                    });
                                });
                            };

                            if (original_default_profile_id != reply.profile_id) {
                                // The default profile ID has changed, so restore it
                                core_api->set_default_profile(original_default_profile_id, [=](orion::Error err) {
                                    VERIFY_LOG(err);
                                    log_verbatim("core api test done: ensure default profile is original %lld",
                                        original_default_profile_id.value
                                    );
                                    completion_lambda();
                                });
                            } else {
                                completion_lambda();
                            }
                        });
                    });
                });

                run_all_tests_impl(core_api, profile_id, done);
            });
        });
    });
#endif // ENABLE_CORE_API_TEST
}

static void
run_all_tests_impl(CoreApi* core_api, ProfileId profile_id, std::shared_ptr<Done> done)
{
    using TestFunc = std::function<void(CoreApi*, ProfileId, std::shared_ptr<Done>)>;
    std::vector<TestFunc> test_funcs{
        test_memory_leaks,
        test_bookmarks,
        test_history_entries,
        test_profiles,
        test_profiles_update,
        test_reading_list_entries,
        test_windows_and_tabs,
        test_cascade_delete_of_bookmark_folders,
        test_cascade_delete_of_bookmarks_in_folders,
        test_settings,
        test_setting_introspection,
        test_callback_and_notify_order,
        test_extensions,
        test_extension_storage_areas,
        test_string_utils
    };

    // Set the expected count to the number of individual tests below which will be run
    done->set_expect_count(test_funcs.size());

    for (auto& func : test_funcs) {
        func(core_api, profile_id, done);
    }
}

} // namespace orion
