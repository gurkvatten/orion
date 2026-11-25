#include <core/settings.h>
#include <common/src/defer.h>

#include "download_cleanup.h"
#include "app.h"

namespace orion {

void
cleanup_downloads(
    orion::ProfileId profile_id, CleanupDownloadsTrigger trigger, orion::ErrorFn done_cb)
{
    global_core_api->get_setting(profile_id, orion::settings::general::remove_download_items_mode,
    [profile_id,trigger,done_cb=std::move(done_cb)]
    (orion::GetSettingReply<orion::settings::RemoveDownloadItemsMode> reply){
        VERIFY_LOG_OR_DONE_CB(reply.err, done_cb);
        orion::settings::RemoveDownloadItemsMode mode = reply.value;
        orion::Error err = MAKE_ERROR(ErrorCode::not_implemented, "cleanup_downloads: unhandled RemoveDownloadItemsMode");

        switch (reply.value) {
            case orion::settings::RemoveDownloadItemsMode::manually:
                if (trigger == CleanupDownloadsTrigger::manual) {
                    orion::DeleteDownloadsQuery query{
                        .profile_id = profile_id,
                    };
                    global_core_api->delete_downloads(query,
                    [profile_id,done_cb=std::move(done_cb)](orion::DeleteDownloadsReply reply){
                        VERIFY_LOG_OR_DONE_CB(reply.err, done_cb);
                        orion::log_debug(
                            "cleanup_download_items: deleted all downloads: trigger=manual profile_id=%lld num_deleted=%lld",
                            profile_id.value,
                            reply.num_deleted
                        );
                        if (done_cb) done_cb(orion::Error::ok());
                    });
                }
                err = orion::Error::ok();
                break;
            case orion::settings::RemoveDownloadItemsMode::after_exit:
                if (trigger == CleanupDownloadsTrigger::on_quit || trigger == CleanupDownloadsTrigger::on_startup) {
                    orion::DeleteDownloadsQuery query{
                        .profile_id = profile_id,
                    };
                    global_core_api->delete_downloads(query,
                    [profile_id,trigger,done_cb=std::move(done_cb)](orion::DeleteDownloadsReply reply){
                        VERIFY_LOG_OR_DONE_CB(reply.err, done_cb);
                        if (reply.num_deleted > 0) {
                            orion::log_debug(
                                "cleanup_download_items: deleted all downloads: trigger=%s profile_id=%lld num_deleted=%lld",
                                trigger == CleanupDownloadsTrigger::on_quit ? "on_quit" : "on_startup",
                                profile_id.value,
                                reply.num_deleted
                            );
                        }
                        if (done_cb) done_cb(orion::Error::ok());
                    });
                }
                err = orion::Error::ok();
                break;
            case orion::settings::RemoveDownloadItemsMode::after_1_day:
            case orion::settings::RemoveDownloadItemsMode::upon_successful_download:
                // TODO:
                err = orion::Error::ok();
                break;
            case orion::settings::RemoveDownloadItemsMode::not_set:
                break;
        }
        done_cb(err);
    });
}

} // namespace orion