#define ENABLE_VERBOSE_LOGS 0

#include <webextensions/webview_factory_host.h>
#include <util/file_util.h>
#include <common/src/string_util.h>
#include <common/src/defer.h>

#include "private_windows.h"
#include "global_core_api.h"

namespace orion::profile {

static Error
cleanup_private_webkit_cache_data(orion::ProfileId profile_id)
{
    std::vector<std::string> path = WebExtensions::WebExtensionWebViewFactory::get_profile_path_parts(
        profile_id,
        orion::PrivateMode::yes
    );

    std::string dir_path = orion::str::join_strings(path, G_DIR_SEPARATOR_S);
    GFile* dir = g_file_new_for_path(dir_path.c_str());
    defer([dir]{ g_object_unref(dir); });

    if (g_file_test(dir_path.c_str(), G_FILE_TEST_IS_DIR)) {
        GError* error = nullptr;
        if (!orion::delete_recursively(dir, &error)) {
            defer([error]{ g_error_free(error); });
            RAISE(
                orion::ErrorCode::io,
                "failed to delete private profile data for profile_id=%" PRId64 ": %s",
                profile_id.value,
                error ? error->message : "unknown error"
            );
        }
    }
    SUCCEED();
}

// Asynchronously deletes all file system data associated with all private windows in the specified
// profile (in ~/.cache subdirectory), eg., cookies, webview cache, filter lists etc.
// Also deletes all private filter lists from ProfilesDb.
void
cleanup_private_data(orion::ProfileId profile_id, orion::Cb<Error> cb)
{
    orion::log_verbose("profile::cleanup_private_data: profile_id=%lld", profile_id.value);

    global_core_api->async_disk([cb=std::move(cb),profile_id]{
        if (Error err = cleanup_private_webkit_cache_data(profile_id); err) {
            // Important: Always dispatch the callback on the main thread (not disk thread)
            global_core_api->async([cb=std::move(cb),err]{
                cb(WRAP_ERROR(err));
            });
            return;
        }

        // Important: Always dispatch the callback on the main thread (not disk thread)
        global_core_api->async([cb=std::move(cb)]{
            cb(Error::ok());
        });
    });
}

} // namespace orion::profile
