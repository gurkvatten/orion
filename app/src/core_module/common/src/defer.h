#pragma once

#include <functional>

/* Defer usage:
 *
 *   defer([]{
 *       // this is called automatically when defer's scope ends
 *   });
 */

#define DEFER_CONCAT_(x, y) x##y
#define DEFER_CONCAT(x, y) DEFER_CONCAT_(x, y)
#define defer auto DEFER_CONCAT(defer__, __LINE__) = orion::Defer

namespace orion {

template <typename F>
class Defer {
public:
    explicit Defer(F fn) : m_fn(std::move(fn)), m_active(true) {}
    ~Defer()
    {
        if (m_active) {
            m_fn();
        }
    }
    
    Defer(const Defer&) = delete;
    Defer& operator=(const Defer&) = delete;
    Defer(Defer&&) = delete;
    Defer& operator=(Defer&&) = delete; // Usually not needed to assign

    void cancel() { m_active = false; }

private:
    F m_fn;
    bool m_active;
};

} // namespace orion