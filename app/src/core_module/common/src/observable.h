#pragma once

#include <functional>
#include <map>
#include <mutex>
#include <src/strong_typedef.h>
#include <src/log.h>
#include <src/thread_type.h>
#include <atomic>
#include <memory>

#include "export.h"

namespace orion {

// Zero means invalid observer ID
DECLARE_STRONG_TYPEDEF(ObserverId, size_t);

ThreadType tls_thread_type();
std::string to_string(ThreadType thread_type);
void observable_notify_internal(ThreadType thread_type, std::function<void()> work);

CORE_MODULE_API extern std::atomic<int64_t> global_next_observable_id; // tracks Observable objects
CORE_MODULE_API extern std::atomic<size_t> global_next_observer_id;    // tracks registered observers
CORE_MODULE_API extern ObserverId global_track_observer_id;            // for debugging purposes

void observable_log_debug(const char* format, ...);
void observable_log_verbose(const char* format, ...);

template <typename... Args>
class Observable {
public:
    using Callback = std::function<void(const Args&...)>;
    using Filter = std::function<bool(const Args&...)>;

    Observable()
    {
        // fetch_add returns the value before the addition
        m_observable_id = global_next_observable_id.fetch_add(1, std::memory_order_relaxed);
        observable_log_debug("observable[id=%zu]: created", m_observable_id);
    }

    friend void observable_log_debug(const char* format, ...);
    friend void observable_log_verbose(const char* format, ...);

    ObserverId
    create_observer()
    {
        // fetch_add returns the value before the addition
        return static_cast<ObserverId>(global_next_observer_id.fetch_add(1, std::memory_order_relaxed));
    }

    ObserverId
    add_observer(Callback cb, Filter filter = nullptr)
    {
        ObserverId id = create_observer();
        add_observer(id, cb, filter);
        return id;
    }

    void
    add_observer(ObserverId id, Callback cb, Filter filter = nullptr)
    {
        observable_log_debug("observable[id=%zu]: add_observer (from=%s observer_id=%zu)",
            m_observable_id,
            to_string(tls_thread_type()).c_str(),
            id.value
        );
        Observer observer{
            .cb = std::move(cb),
            .thread_type = tls_thread_type(),
            .filter = std::move(filter)
        };

        std::lock_guard<std::mutex> lock(m_mutex);
        if (id.value && id.value == global_track_observer_id.value) {
            log_debug("observable[id=%zu]: add_observer (observer_id=%zu): tracking",
                m_observable_id,
                id.value
            );
        }
        m_observers[id] = std::move(observer);
    }

    void
    clear_observer(ObserverId* id)
    {
        remove_observer_and_reset(id);
    }

    // Use clear_observer instead
    void
    remove_observer_and_reset(ObserverId* id)
    {
        if (id == nullptr) {
            orion::log_warning("%s: null observer_id pointer", __func__);
            return;
        }
        remove_observer(*id);
        *id = ObserverId(0);
    }

    void
    remove_observer(ObserverId id) 
    {
        if (id.value == 0) {
            observable_log_verbose("remove_observer: invalid observer_id 0");
            return;
        }

        if (id.value && id.value == global_track_observer_id.value) {
            observable_log_debug("observable[id=%zu]: remove_observer (observer_id=%zu): tracking",
                m_observable_id,
                id.value
            );
        }
        observable_log_debug("observable[id=%zu]: remove_observer (observer_id=%zu)",
            m_observable_id,
            id.value
        );

        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_observers.find(id);
        if (it == m_observers.end()) {
            orion::log_warning("remove_observer: observer_id %zu not found", id.value);
        } else {
            m_observers.erase(it);
        }
    }

    void
    notify(Args... args)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        // iterate over a copy to allow modification during notification
        auto observers_copy = m_observers;
        lock.unlock(); // unlock before dispatching to avoid deadlock

        // Be very careful here: when trying to optimize things using perfect forwarding into a tuple:
        //     auto args_tuple = std::make_tuple(std::forward<Args>(args)...);
        // This would be compilable but it silently breaks things notifying some observers with
        // empty/default arguments instead of the actual arguments.
        using TupleType = decltype(std::make_tuple(std::forward<Args>(args)...));
        auto shared_args_tuple = std::make_shared<TupleType>(std::make_tuple(args...));
        for (const auto& [id, observer] : observers_copy) {
            const auto& cb = observer.cb;
            if (!cb) {
                continue;
            }
            
            if (observer.filter && !observer.filter(args...)) {
                continue; // skip this observer if the filter does not match
            }

            observable_log_debug("observable[id=%zu]: notify (on=%s from=%s observer_id=%zu)", 
                m_observable_id,
                to_string(observer.thread_type).c_str(),
                to_string(tls_thread_type()).c_str(),
                id.value
            );

            // dispatch call must be hidden in implementation
            observable_notify_internal(observer.thread_type, [cb=std::move(cb),args=shared_args_tuple]() {
                std::apply(cb, *args); // invoke the callback with the arguments
            });
        }
    }

    size_t
    observer_count() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_observers.size();
    }

    std::vector<ObserverId>
    observer_ids() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<ObserverId> ids;
        for (const auto& [id, observer] : m_observers) {
            ids.push_back(id);
        }
        return ids;
    }

private:
    struct Observer {
        Callback cb;
        ThreadType thread_type = ThreadType::main;
        Filter filter = nullptr;
    };

    std::map<ObserverId, Observer> m_observers;
    mutable std::mutex m_mutex;
    size_t m_observable_id = 0; // for debugging purposes
};

template <typename T>
static void
check_observers(const orion::Observable<T>& observable, const char* name)
{
    if (observable.observer_ids().empty()) {
        return;
    }
    std::string s;
    for (const auto& id : observable.observer_ids()) {
        if (!s.empty()) {
            s += " ";
        }
        s += std::to_string(id.value);
    }
    orion::log_warning("warning: remaining %s observers (%zu): ids=[%s]",
        name, observable.observer_ids().size(), s.c_str());
}

} // namespace orion