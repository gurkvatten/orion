#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <vector>

#include <common/src/observable.h>
#include <core/dto/filter_list_dto.h>

namespace orion {

class CoreApi;

class FilterListUpdateService : public std::enable_shared_from_this<FilterListUpdateService> {
public:
    using UpdateHandler = std::function<void(std::vector<FilterListDto>)>;

    static std::shared_ptr<FilterListUpdateService> create(CoreApi* core_api);

    void start();
    void stop();
    void set_update_handler(UpdateHandler handler);

private:
    explicit FilterListUpdateService(CoreApi* core_api);

    void schedule_next();
    void update_stale_filter_lists();
    void update_stale_filter_lists(ProfileId profile_id);

    CoreApi* m_core_api; // Relies on the run loop exiting before CoreApi is destroyed.
    UpdateHandler m_update_handler;
    std::atomic<int> m_scheduled_counter;
    uint64_t m_delay;
    bool m_started = false;
    std::atomic<bool> m_stopped{false};
    ObserverId m_setting_observer_id{};
};

} // namespace orion
