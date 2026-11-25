#pragma once

#include <memory>

#include <core/core_api.h>

namespace WebExtensions {

class WebExtensionsRuntimeImpl;

class AlarmsImpl : public v2::Alarms {
public:
    void set_profile_world(std::weak_ptr<orion::bridge::ProfileWorld> profile_world);
    void clear(v2::Promise<bool> promise, const std::string* name) override;
    void clearAll(v2::Promise<bool> promise) override;
    void create(v2::Promise<void> promise, const std::string* name, const v2::AlarmCreateInfo& alarm_info) override;
    void get(v2::Promise<v2::Alarm> promise, const std::string* name) override;
    void getAll(v2::Promise<std::vector<v2::Alarm>> promise) override;
private:
    struct AlarmImpl : v2::AlarmStorage {
        std::string extension_id;
        bool valid = true;
        bool scheduled = false;
    };
    void triggerAlarm(const std::shared_ptr<AlarmImpl>& alarm);
    void pruneAlarms();
    void scheduleTimer();
    void updateNowTime();
private:
    typedef std::map<std::string, std::shared_ptr<AlarmImpl>> Alarms;
    struct ExtensionAlarms {
        Alarms alarms;
    };
    typedef std::map<std::string, ExtensionAlarms> ExtensionsAlarmMap;
    ExtensionsAlarmMap m_extensions_alarms_map;
    std::chrono::milliseconds m_now;
    std::weak_ptr<orion::bridge::ProfileWorld> m_profile_world;
};

} // namespace WebExtensions
