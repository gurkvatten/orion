#pragma once
#include <../common/src/error.h>
#include <thread>
#include <condition_variable>
#include <atomic>

namespace orion::sync {


class ThreadHolder {

public:
    ThreadHolder() : m_cancelled(false), m_finished(false) {};
    ~ThreadHolder() {
        cancel();
    };

    virtual void run() = 0;
    void wait() {
        m_thread.join();
    }
    virtual const Error& lastError() const = 0;
    virtual void cleanup() = 0;

    void cancel();

    static void set_sigaction();
    static void clear_sigaction();

protected:
    void signal();

protected:
    std::atomic<bool> m_cancelled;
    std::atomic<bool> m_finished;
    std::thread m_thread; //there's also jthread that little simplifies cancel non blocking, however it's linked to its destructor

private:
    std::mutex m_m;
    std::condition_variable m_cv;
};

class ThreadRunner {
public:
    virtual bool start() {return true;};
    virtual bool run() = 0; //this could slow down the call but it simplifies the model & it's more about io anyway

    const Error& lastError() const { return m_lastError; };
    virtual void cleanup() = 0;
    virtual void rollback(const Error& reason) = 0;

protected:
    Error m_lastError;
};

class ThreadRunnerHolder : public ThreadHolder {
public:
    explicit ThreadRunnerHolder(ThreadRunner* runner) :
        m_runner(std::shared_ptr<ThreadRunner>(runner))
        {}
    ~ThreadRunnerHolder() = default;

    virtual void run();

    virtual const Error& lastError() const { return m_runner->lastError(); };
    virtual void cleanup() { m_runner->cleanup(); };

private:
    std::shared_ptr<ThreadRunner> m_runner;
};

//TODO ThreadHolder and ThreadRunnerHolder could probably go to common

}