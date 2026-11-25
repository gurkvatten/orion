#pragma once

#include <chrono>
#include <functional>
#include <memory>

namespace orion::tm {

class Timer {
public:
    using Cb = std::function<bool()>;
public:
    static std::shared_ptr<Timer> create_shared();
    static Timer* create();

    virtual void start(Cb&& cb, std::chrono::milliseconds delay, bool repetitive) = 0;
    virtual bool is_running() const = 0;
    virtual void stop() = 0;

};
} //end orion::tm