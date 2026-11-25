#include "timer.h"
#include <thread>
#include <atomic>
#include <condition_variable>

namespace orion::tm {

class TimerImp : public Timer {
public:
    virtual void start(Cb&& cb, std::chrono::milliseconds delay, bool repetitive) override {
        if (is_running()) {
            return;
        }
        m_stop = false;
        //this will cause delete of the potentially abandoned std::thread
        m_thread = std::shared_ptr<std::thread>(new std::thread([this, cb, delay, repetitive](){
            do {
                std::unique_lock<std::mutex> lk(m_m);
                if (m_stop) {
                    break;
                }
                if (m_cv.wait_for(lk, delay) == std::cv_status::timeout) {
                    lk.release();
                    if (!cb()) {
                        break;
                    }
                } else if (m_stop) {
                    break;
                }
            } while (repetitive);
        }));
    }

    virtual bool is_running() const override {
        return m_thread != nullptr && m_thread->joinable();
    }

    virtual void stop() override {
        std::lock_guard<std::mutex> lk(m_m);
        m_stop = true;
        m_cv.notify_all();
    }

    TimerImp() = default;
    ~TimerImp() = default;

private:
    std::atomic<bool> m_stop = false;
    std::shared_ptr<std::thread> m_thread;
    std::mutex m_m;
    std::condition_variable m_cv;
};

std::shared_ptr<Timer> Timer::create_shared() {
    return std::shared_ptr<Timer>(new TimerImp());
}

Timer* Timer::create() {
    return new TimerImp();
}
}