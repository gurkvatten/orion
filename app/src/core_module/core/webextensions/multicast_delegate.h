#pragma once

#include <memory>
#include <vector>
#include <functional>

namespace WebExtensions {

template<typename... Args>
class MulticastDelegate {
public:
    typedef std::function<void(Args...)> Func;
    
    void
    operator+=(std::shared_ptr<Func>& bind)
    {
        m_binds.push_back(bind);
    }

    void
    operator()(Args... args)
    {
        decltype(m_binds) binds;
        binds.swap(m_binds);
        for(auto& bind : binds) {
            auto ref = bind.lock();
            if(ref) {
                (*ref)(args...);
                m_binds.push_back(std::move(bind));
            }
        }
    }
private:
    std::vector<std::weak_ptr<Func>> m_binds;
};

} // namespace WebExtensions

using WebExtensions::MulticastDelegate;
