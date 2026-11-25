#include <core/profiles_db/filter_list_update_service.h>

#include <common/src/defer.h>
#include <common/src/dispatch.h>
#include <common/src/time_util.h>
#include <core/core_api.h>
#include <core/settings.h>

namespace orion {

static constexpr int64_t INITIAL_DELAY_SECS = 5;
static constexpr int64_t PERIOD_SECS = 24 * 60 * 60;
static constexpr int64_t UPDATE_INTERVAL_SECS = 24 * 60 * 60;

std::shared_ptr<FilterListUpdateService>
FilterListUpdateService::create(CoreApi* core_api)
{
    return std::shared_ptr<FilterListUpdateService>(new FilterListUpdateService(core_api));
}

FilterListUpdateService::FilterListUpdateService(CoreApi* core_api)
    : m_core_api(core_api)
    , m_scheduled_counter{0}
    , m_delay{INITIAL_DELAY_SECS}
{
}

void
FilterListUpdateService::start()
{
    if (m_started) return;
    m_started = true;

    m_setting_observer_id = m_core_api->add_single_setting_change_observer(
        settings::privacy::auto_update_content_blockers,
        [guard=shared_from_this()](const SingleSettingChange<bool>& change) {
            if (!guard->m_stopped.load() && change.new_value) {
                guard->update_stale_filter_lists(change.profile_id);
            }
        });
    schedule_next();
}

void
FilterListUpdateService::stop()
{
    m_stopped.store(true);
    if (m_setting_observer_id != ObserverId{}) {
        m_core_api->single_setting_change().remove_observer(m_setting_observer_id);
        m_setting_observer_id = {};
    }
}

void
FilterListUpdateService::set_update_handler(UpdateHandler handler)
{
    m_update_handler = std::move(handler);
}

void
FilterListUpdateService::update_stale_filter_lists()
{
    if (m_stopped.load()) return;

    m_core_api->get_profile_ids([guard=shared_from_this()](GetProfileIdsReply reply) {
        VERIFY_LOG(reply.err);
        if (reply.err || guard->m_stopped.load()) return;

        for (ProfileId profile_id : reply.ids) {
            guard->update_stale_filter_lists(profile_id);
        }
    });
}

void
FilterListUpdateService::update_stale_filter_lists(ProfileId profile_id)
{
    if (m_stopped.load()) return;

    m_core_api->get_setting(
        profile_id,
        settings::privacy::auto_update_content_blockers,
        [guard=shared_from_this(),profile_id](GetSettingReply<bool> reply) {
            VERIFY_LOG(reply.err);
            if (reply.err || guard->m_stopped.load() || !reply.value) return;

            GetFilterListsQuery query{
                .profile_id = profile_id,
            };
            guard->m_core_api->get_filter_lists(
                query,
                [guard](GetFilterListsReply reply) {
                    VERIFY_LOG(reply.err);
                    if (reply.err || guard->m_stopped.load()) return;

                    const UnixTimeUtc64 now = tm::unix_time_utc64_now();
                    std::vector<FilterListDto> stale_filter_lists;
                    for (FilterListDto& filter_list : reply.filter_lists) {
                        if (!filter_list.is_enabled || filter_list.url.empty()) continue;
                        if (filter_list.filter_type != FilterListType::builtin
                            && filter_list.filter_type != FilterListType::user) {
                            continue;
                        }
                        if (filter_list.updated_at.value > 0
                            && now.value - filter_list.updated_at.value < UPDATE_INTERVAL_SECS) {
                            continue;
                        }
                        stale_filter_lists.push_back(std::move(filter_list));
                    }

                    if (stale_filter_lists.empty()) return;
                    orion::log_debug(
                        "filter_lists: automatically updating stale filter lists: count=%zu",
                        stale_filter_lists.size());
                    dispatch(on_main_thread(), [guard,filter_lists=std::move(stale_filter_lists)]() mutable {
                        if (!guard->m_stopped.load() && guard->m_update_handler) {
                            guard->m_update_handler(std::move(filter_lists));
                        }
                    });
                });
        });
}

void
FilterListUpdateService::schedule_next()
{
    if (m_stopped.load()) return;

    defer([this] { m_scheduled_counter.fetch_sub(1); });
    if (m_scheduled_counter.fetch_add(1) > 0) return;

    ThreadType thread_type = tls_thread_type();
    dispatch(
        on_disk_thread(),
        after(std::chrono::milliseconds(static_cast<int64_t>(m_delay) * 1000)),
        [guard=shared_from_this(),thread_type] {
            if (!guard->m_stopped.load()) {
                guard->update_stale_filter_lists();
            }
            guard->m_scheduled_counter.fetch_sub(1);

            if (!guard->m_stopped.load()) {
                dispatch(on_thread(thread_type), [guard] {
                    guard->schedule_next();
                });
            }
        });
    m_delay = PERIOD_SECS;
    m_scheduled_counter.fetch_add(1);
}

} // namespace orion
