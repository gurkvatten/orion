#pragma once
#include <memory>
#include <core/db/db.h>

namespace orion {

class CoreApi;

/*
 * Manages cleanup of history entries for profiles.
 * The class manages all profiles that need periodic cleanup.
 * 
 * Alternatively we could have a simpler class and run as many instances as many profiles needs 
 * a periodic clean up. Not really sure what is better option.
 * 
 * an instance of this class is created in CoreApi::init and kept alive as long as CoreApi is alive.
 * it clerans up history for all profiles using single sql statement reflecting possible 
 * different setting for each prifiles
 * 
 * on_after_exit method as called from request_stop_run_loop as the first thing
 *
*/
class HistoryCleanupService : public std::enable_shared_from_this<HistoryCleanupService>{
public:
    //this could've been unique_ptr if we didn't safeguard asynch callbacks with shared ptr ref guard
    static std::shared_ptr<HistoryCleanupService> create(orion::CoreApi* api, std::shared_ptr<Db> db);

    void start();
    void after_app_exit();

private:
    HistoryCleanupService(orion::CoreApi* api, std::shared_ptr<Db> db);

    void schedule_next();
 
    std::shared_ptr<Db> m_db;
    orion::CoreApi* m_api;
    std::atomic<int> scheduled_counter;//this is here to address very unlikely but not an impossible scenario
    uint64_t m_delay;
};

} // namespace orion