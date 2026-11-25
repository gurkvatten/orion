#include <common/src/dispatch.h>
#include <common/src/defer.h>
#include <common/src/time_util.h>

#include <core/profiles_db/download_cleanup_service.h>
#include <core/dto/download_dto.h>
#include <core/profiles_db/profiles_db.h>
#include <core/settings.h>
#include <core/core_api.h>

namespace orion {

static constexpr int64_t INITIAL_DELAY_SECS = 5; // 5 seconds
static constexpr int64_t PERIOD_SECS = 10 * 60; // 10 minutes
static constexpr int64_t DAY_SECS = 24 * 60 * 60; // 1 day

void
DownloadCleanupService::prune_downloads()
{
    orion::log_verbose("DownloadCleanupService::prune_downloads: starting");

    std::vector<ProfileId> profile_ids;

    m_core_api->get_profile_ids([core_api = m_core_api](GetProfileIdsReply reply){

        VERIFY_LOG(reply.err);

        for(const auto& profile_id : reply.ids)
        {
            core_api->get_setting(profile_id, settings::general::remove_download_items_mode,
            [core_api, profile_id](GetSettingReply<settings::RemoveDownloadItemsMode> reply){
                
                VERIFY_LOG(reply.err);

                UnixTimeUtc64 threshold = UnixTimeUtc64(INT64_MIN);

                switch(reply.value)
                {
                    case settings::RemoveDownloadItemsMode::after_1_day:
                        threshold = tm::unix_time_utc64_now().value - DAY_SECS;
                        break;
                    case settings::RemoveDownloadItemsMode::not_set:
                    case settings::RemoveDownloadItemsMode::after_exit:
                    case settings::RemoveDownloadItemsMode::upon_successful_download:
                    case settings::RemoveDownloadItemsMode::manually:
                        break;
                }

                if(threshold == UnixTimeUtc64(INT64_MIN)) return;
                
                DeleteDownloadsQuery query{
                    .profile_id = profile_id,
                    .older_than_timestamp = UnixTimeUtc64(threshold),
                };

                core_api->delete_downloads(query, [profile_id, threshold](DeleteDownloadsReply reply){
                    VERIFY_LOG(reply.err);
                    orion::log_debug("DownloadCleanupService::prune_downloads: profile_id=%lld deleted %lld downloads older than %lld",
                        profile_id.value, reply.num_deleted, threshold.value);
                });
            });
        }
    });
}

std::shared_ptr<DownloadCleanupService>
DownloadCleanupService::create(CoreApi* core_api)
{
    return std::shared_ptr<DownloadCleanupService>(new DownloadCleanupService(core_api));
}

DownloadCleanupService::DownloadCleanupService(CoreApi* core_api)
    : m_core_api(core_api)
    , m_scheduled_counter{0}
    , m_delay{INITIAL_DELAY_SECS}
{
}

void
DownloadCleanupService::start()
{
    schedule_next();
}

void
DownloadCleanupService::schedule_next()
{
    defer([this]{ m_scheduled_counter.fetch_sub(1); });
    if (m_scheduled_counter.fetch_add(1) > 0) {
        return;
    }

    // Mark the original thread
    orion::ThreadType thread_type = orion::tls_thread_type();

    dispatch(orion::on_disk_thread(), orion::after(std::chrono::milliseconds(static_cast<int64_t>(m_delay) * 1000)),
        [guard=shared_from_this(),thread_type]{
            guard->prune_downloads();
            guard->m_scheduled_counter.fetch_sub(1);

            // Dispatch back to the original thread
            dispatch(orion::on_thread(thread_type), [guard]{
                guard->schedule_next();
            });
        });
    m_delay = PERIOD_SECS;
    m_scheduled_counter.fetch_add(1);
}

} // namespace orion
