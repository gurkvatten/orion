#include <core/webextensions/private/alarms.h>
#include <core/webextensions/runtime_impl.h>
#include <common/src/dispatch.h>
#include <common/src/cast.h>

namespace WebExtensions {

void
AlarmsImpl::set_profile_world(
    std::weak_ptr<orion::bridge::ProfileWorld> profile_world)
{
    m_profile_world = profile_world;
}

void
AlarmsImpl::clear(v2::Promise<bool> promise, const std::string* name)
{
    const std::string& extension_id = promise.extension_id();
    ExtensionAlarms& extension_alarms = m_extensions_alarms_map[extension_id];
    auto it = extension_alarms.alarms.find(name ? *name : "");
    if(it == extension_alarms.alarms.end()) {
        promise.resolve(false);
        return;
    }
    it->second->valid = false;
    extension_alarms.alarms.erase(it);
    promise.resolve(true);
}

void
AlarmsImpl::clearAll(v2::Promise<bool> promise)
{
    const std::string& extension_id = promise.extension_id();
    ExtensionAlarms& extension_alarms = m_extensions_alarms_map[extension_id];
    for(auto& [name, alarm] : extension_alarms.alarms) {
        alarm->valid = false;
    }
    m_extensions_alarms_map.erase(extension_id);
    promise.resolve(true);
}

void
AlarmsImpl::create(
    v2::Promise<void> promise, const std::string* name, const v2::AlarmCreateInfo& alarm_info)
{
    if(alarm_info.when && alarm_info.delayInMinutes) {
        promise.reject("AlarmCreateInfo cannot contain both when and delayInMinutes");
        return;
    }

    updateNowTime();
    double scheduled_time = 0;
    if(alarm_info.when) {
        scheduled_time = *alarm_info.when;
    }
    else if(alarm_info.delayInMinutes) {
        scheduled_time = m_now.count() + *alarm_info.delayInMinutes * 60.0 * 1000.0;
    }
    else if(alarm_info.periodInMinutes) {
        scheduled_time = m_now.count() + *alarm_info.periodInMinutes * 60.0 * 1000.0;
    }
    else {
        promise.reject("AlarmCreateInfo requires when, delayInMinutes, or periodInMinutes");
        return;
    }

    const std::string& extension_id = promise.extension_id();
    ExtensionAlarms& extension_alarms = m_extensions_alarms_map[extension_id];
    const std::string alarm_name = name ? *name : "";
    auto& alarm = *(extension_alarms.alarms[alarm_name] = std::make_shared<AlarmImpl>());
    alarm.extension_id = extension_id;
    alarm.name = alarm_name;
    alarm.scheduledTime = scheduled_time;
    if(alarm_info.periodInMinutes) {
        alarm.periodInMinutes = *alarm_info.periodInMinutes;
    }
    alarm.persistAcrossSessions = alarm_info.persistAcrossSessions
        ? *alarm_info.persistAcrossSessions
        : true;
    alarm.valid = true;
    scheduleTimer();
    promise.resolve();
}

void
AlarmsImpl::get(v2::Promise<v2::Alarm> promise, const std::string* name)
{
    const std::string& extension_id = promise.extension_id();
    ExtensionAlarms& extension_alarms = m_extensions_alarms_map[extension_id];
    auto it = extension_alarms.alarms.find(name ? *name : "");
    if(it == extension_alarms.alarms.end()) {
        promise.resolve();
        return;
    }
    const v2::Alarm alarm = v2::from_storage(*it->second);
    promise.resolve(alarm);
}

void
AlarmsImpl::getAll(v2::Promise<std::vector<v2::Alarm>> promise)
{
    const std::string& extension_id = promise.extension_id();
    std::vector<v2::Alarm> alarms;
    ExtensionAlarms& extension_alarms = m_extensions_alarms_map[extension_id];
    for(const auto& [name, alarm] : extension_alarms.alarms) {
        alarms.push_back(v2::from_storage(*alarm));
    }
    promise.resolve(alarms);
}

void
AlarmsImpl::triggerAlarm(const std::shared_ptr<AlarmImpl>& alarm)
{
    // alarm is null when the weak_ptr timer hint outlived the alarm
    // (cleared/erased before the timer fired).
    if(!alarm) {
        return;
    }
    if(alarm->valid && alarm->scheduled) {
        const v2::Alarm alarm_view = v2::from_storage(*alarm);
        onAlarm.notify_extension(alarm->extension_id, alarm_view);
        if(alarm->periodInMinutes) {
            alarm->scheduledTime = m_now.count() + *alarm->periodInMinutes * 60.0 * 1000.0;
        }
        else {
            alarm->valid = false;
        }
    }
    alarm->scheduled = false;
}

void
AlarmsImpl::pruneAlarms()
{
    ExtensionsAlarmMap extensions_alarms_map;
    for(auto& [extension_id, extension_alarms] : m_extensions_alarms_map) {
        Alarms alarms;
        for(const auto& [name, alarm] : extension_alarms.alarms) {
            if(!alarm->valid) {
                continue;
            }
            alarms[name] = alarm;
        }
        if(!alarms.empty()) {
            extensions_alarms_map[extension_id].alarms = alarms;
        }
    }
    m_extensions_alarms_map = extensions_alarms_map;
}

void
AlarmsImpl::scheduleTimer()
{
    std::shared_ptr<AlarmImpl> earliestAlarm;
    for(const auto& [extension_id, extension_alarms] : m_extensions_alarms_map) {
        for(const auto& [name, alarm] : extension_alarms.alarms) {
            if(!alarm->valid) {
                continue;
            }
            if(!earliestAlarm || alarm->scheduledTime < earliestAlarm->scheduledTime) {
                earliestAlarm = alarm;
            }
        }
    }
    if(!earliestAlarm || earliestAlarm->scheduled) {
        return;
    }
    double waitFor = earliestAlarm->scheduledTime - m_now.count();
    waitFor = std::max(0.0, waitFor);
    earliestAlarm->scheduled = true;
    std::weak_ptr<AlarmImpl> hint = earliestAlarm;
    auto profile_world = m_profile_world;
    dispatch(orion::on_main_thread(), orion::after(std::chrono::milliseconds((int64_t)waitFor)), [profile_world, hint]() {
        auto world = profile_world.lock();
        std::shared_ptr<AlarmImpl> alarm = hint.lock();
        if(!alarm) return;
        auto* runtime = world
            ? static_cast<WebExtensionsRuntimeImpl*>(world->runtime())
            : nullptr;
        if(!runtime) return;
        auto* alarms = static_cast<AlarmsImpl*>(runtime);
        alarms->updateNowTime();
        alarms->triggerAlarm(alarm);
        alarms->pruneAlarms();
        alarms->scheduleTimer();
    });
}

void
AlarmsImpl::updateNowTime()
{
    m_now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
}

} // namespace WebExtensions
