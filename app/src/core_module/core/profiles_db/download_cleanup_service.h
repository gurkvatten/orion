#pragma once
#include <memory>
#include <atomic>

namespace orion {

class CoreApi;

class DownloadCleanupService : public std::enable_shared_from_this<DownloadCleanupService> {
public:
    static std::shared_ptr<DownloadCleanupService> create(CoreApi* profiles_db);

    void start();

private:
    DownloadCleanupService(CoreApi* core_api);

    void schedule_next();
    void prune_downloads();

    CoreApi* m_core_api; //relies on runloop exiting before CoreApi gets destroyed
    std::atomic<int> m_scheduled_counter;
    uint64_t m_delay;
};

} // namespace orion
