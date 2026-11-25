#include <core/profiles_db/favicon_cleanup_service.h>
#include <common/src/dispatch.h>
#include <common/src/defer.h>

namespace orion {

static constexpr int64_t PERIOD_SECS = 10 * 60; // 10 minutes (GTK uses 10 seconds, but that's aggressive)
static constexpr int64_t NOT_USED_EXPIRATION_SECS = 60 * 60 * 24 * 30; // 30 days

static Error
prune_favicons(Db* db)
{
    int64_t threshold = static_cast<int64_t>(std::time(nullptr)) - NOT_USED_EXPIRATION_SECS;

    VERIFY(db->exec(R"(
        DELETE FROM favicon_icons WHERE stamp <= ?1;
    )", {threshold}));

    VERIFY(db->exec(R"(
        DELETE FROM favicon_page_urls WHERE icon_id NOT IN (SELECT id FROM favicon_icons);
    )", {}));

    SUCCEED();
}

std::shared_ptr<FaviconCleanupService>
FaviconCleanupService::create(std::shared_ptr<Db> db)
{
    return std::shared_ptr<FaviconCleanupService>(new FaviconCleanupService(db));
}

FaviconCleanupService::FaviconCleanupService(std::shared_ptr<Db> db)
    : m_db(db)
    , m_scheduled_counter{0}
    , m_delay{5} // initial delay 5 seconds
{
}

void
FaviconCleanupService::start()
{
    schedule_next();
}

void
FaviconCleanupService::schedule_next()
{
    defer([this]{ m_scheduled_counter.fetch_sub(1); });
    if (m_scheduled_counter.fetch_add(1) > 0) {
        return;
    }

    dispatch(orion::on_disk_thread(), orion::after(std::chrono::milliseconds(static_cast<int64_t>(m_delay) * 1000)),
        [guard=shared_from_this()]() {
            prune_favicons(guard->m_db.get());
            guard->m_scheduled_counter.fetch_sub(1);
            guard->schedule_next();
        });
    m_delay = PERIOD_SECS;
    m_scheduled_counter.fetch_add(1);
}

} // namespace orion
