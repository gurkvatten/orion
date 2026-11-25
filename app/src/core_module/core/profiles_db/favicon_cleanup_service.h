#pragma once
#include <memory>
#include <core/db/db.h>

namespace orion {

class CoreApi;

class FaviconCleanupService : public std::enable_shared_from_this<FaviconCleanupService> {
public:
    static std::shared_ptr<FaviconCleanupService> create(std::shared_ptr<Db> db);

    void start();

private:
    FaviconCleanupService(std::shared_ptr<Db> db);

    void schedule_next();

    std::shared_ptr<Db> m_db;
    std::atomic<int> m_scheduled_counter;
    uint64_t m_delay;
};

} // namespace orion
