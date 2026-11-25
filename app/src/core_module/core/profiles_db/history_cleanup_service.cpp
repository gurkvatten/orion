
#include <core/profiles_db/history_cleanup_service.h>
#include <core/settings.h>
#include <common/src/dispatch.h>
#include <common/src/defer.h>
#include <core/core_api.h>

namespace orion {

static ObserverId observerId = ObserverId(0);

static Error
clear_history_entries(Db* db)
{
    std::string sql = std::format(
R"(delete from history_entries where id in(
	select h.id from history_entries as h
		inner join (--we only want entries that actually do have the cleanup specified
			select profile_id, key, CAST(value as INTEGER) as value
				, case value
					when 1 then unixepoch('now') - 86400
					when 2 then unixepoch('now') - 604800
					when 3 then unixepoch('now') - 1209600
					when 4 then unixepoch('now') - 2592000
					when 5 then unixepoch('now') - 7776000
					when 6 then unixepoch('now') - 15552000
					when 7 then unixepoch('now') - 31536000
					-- make sure nothing else gets deleted
					else 0
				end threashold_at
				from settings
				where key = '{}' and value > 0 and value < 8
			) as s
		on h.profile_id == s.profile_id
		where h.visited_at < s.threashold_at
))", settings::privacy::remove_history_items_mode.key());		
    int64_t rows_affected = 0;
    VERIFY(db->exec(sql, {}, &rows_affected, nullptr));
    
    SUCCEED();    
}

static Error
clear_history_entries_after_app_exit(Db* db)
{   
    std::string sql = std::format(
R"(delete from history_entries where profile_id in(
	select profile_id from settings
		where key = '{}' and value == 8
))", settings::privacy::remove_history_items_mode.key());	
    int64_t rows_affected = 0;
    VERIFY(db->exec(sql, {}, &rows_affected, nullptr));
    
    SUCCEED();    
}

static const int64_t PERIOD_SECS = 24 * 60 * 60; //24 hours

std::shared_ptr<HistoryCleanupService>
HistoryCleanupService::create(orion::CoreApi* api, std::shared_ptr<Db> db)
{
    return std::shared_ptr<HistoryCleanupService>(new HistoryCleanupService(api, db));
}

HistoryCleanupService::HistoryCleanupService(orion::CoreApi* api, std::shared_ptr<Db> db)
    : m_db(db)
    , m_api(api)
    , scheduled_counter{0}
    , m_delay{5} {//initial delay 5ms to make sure we'll run at least once during the app lifetime
}

void
HistoryCleanupService::start()
{
    if (observerId != ObserverId(0)) {//sanity check
        //already started
        return;
    }
    observerId = m_api->add_single_setting_change_observer(settings::privacy::remove_history_items_mode,
            [guard=shared_from_this(),this](orion::SingleSettingChange<settings::RemoveHistoryItemsMode> change) {
                //schedule the run only if the new value can actually modify the db
                int64_t val = static_cast<int64_t>(change.new_value);
                if (val < 1 || val > 7) {
                    return;
                }
                //there's a slight change that this this run upon our setting change will coincide with the scheduled run
                //but hey, it most likekely will never happen during our lifetime :) 
                dispatch(orion::on_disk_thread(), orion::after(std::chrono::milliseconds((int64_t)500))
                , [guard=shared_from_this()](){
                    clear_history_entries(guard->m_db.get());
                });
            });

    schedule_next();
}

void
HistoryCleanupService::after_app_exit()
{
    scheduled_counter.fetch_add(20); //prevent any scheduling
    if (observerId != ObserverId(0)) {
        m_api->single_setting_change().remove_observer(observerId);
        observerId = ObserverId(0);
    }
    //process all profiles that have after_app_exits mode
    //we do it on the same thread since at this point it doesn't really matter.?
    clear_history_entries_after_app_exit(m_db.get());
}

void
HistoryCleanupService::schedule_next()
{
    //this can run on disk thread as well as core api thread
    defer([this]{ scheduled_counter.fetch_sub(1); });
    if (scheduled_counter.fetch_add(1) > 0) {
        //already scheduled
        return;
    }

    dispatch(orion::on_disk_thread(), orion::after(std::chrono::milliseconds((int64_t)m_delay * 1000))
    , [guard=shared_from_this()](){
        clear_history_entries(guard->m_db.get());
        guard->scheduled_counter.fetch_sub(1);
        guard->schedule_next();
    });
    m_delay = PERIOD_SECS;
    //the above defer decrements the scheduled_counter on going out of scope hence add 1 here
    //still better than check sub at every return
    scheduled_counter.fetch_add(1);
}

} // namespace orion