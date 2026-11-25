#pragma once

#include <functional>
#include <assert.h>
#include <type_traits>
#include <utility>
#include <chrono>
#include <mutex>
#include <atomic>
#include <src/run_loop.h>
#include <src/thread_type.h>

#define DISPATCH_LIFETIME_LOGS 0

#if DISPATCH_LIFETIME_LOGS
#define dispatch_lifetime_log(format, ...) orion::log_debug(format, ##__VA_ARGS__)
#else
#define dispatch_lifetime_log(format, ...)
#endif

namespace orion {

struct OnThread {
    ThreadType value;
    explicit OnThread(ThreadType t) : value(t) {} // disables implicit conversion
};

struct After {
    std::chrono::steady_clock::time_point when;
    explicit After(std::chrono::steady_clock::time_point t) : when(t) {} // disables implicit conversion
    After() : when(std::chrono::steady_clock::now()) {}
};

const int MAX_THREAD_TYPE = 4;

std::string to_string(ThreadType thread_type);

class Job;
class RunLoop;
class CompletionHandler;
class UncheckedCompletionHandler;

// returns thread type of the current thread
ThreadType tls_thread_type();

namespace internal {

CompletionHandler& dispatch_with_completion(
    std::function<Error()> work_cb, ThreadType thread_type, After after, std::string name
);
UncheckedCompletionHandler& dispatch_void(
    std::function<void()> work_void_cb, ThreadType thread_type, After after, std::string name
);
void dispatch_error_cb(std::function<void(Error)> error_cb, Error err, ThreadType thread_type);
std::function<void()> ignore_error(std::function<Error()> work_cb);

} // namespace internal

class CompletionHandler {
public:
    CompletionHandler(Job* job) : m_job(job)
    {
        dispatch_lifetime_log("CompletionHandler(): n=%lld", (m_total_count.fetch_add(1) + 1));
    }

    CompletionHandler(const CompletionHandler& h) : m_job(h.m_job)
    {
        dispatch_lifetime_log("CompletionHandler(): n=%lld", (m_total_count.fetch_add(1) + 1));
    }

    ~CompletionHandler()
    {
        dispatch_lifetime_log("~CompletionHandler(): n=%lld", (m_total_count.fetch_sub(1) - 1));
    }

    CompletionHandler(CompletionHandler&&) = delete;
    CompletionHandler& operator=(const CompletionHandler&) = delete;
    CompletionHandler& operator=(CompletionHandler&&) = delete;

    // Can be called with lvalue (will copy the function), temporary (inline lambda),
    // or rvalue reference (moved std::function to avoid copying). This cannot be easily
    // converted to template function (with perfect forwarding) because we need to know
    // full definition of Job class here.
    CompletionHandler& then(std::function<void()> completion_cb);
    CompletionHandler& error(std::function<void(Error)> error_cb);
    // ignores potential error
    CompletionHandler& then_or_error(std::function<void()> completion_cb);
    CompletionHandler& then_or_error(std::function<void(Error)> completion_cb);

private:
    friend CompletionHandler& internal::dispatch_with_completion(
        std::function<Error()> work_cb, ThreadType thread_type, After after, std::string name
    );
    friend CompletionHandler& when_all(std::vector<CompletionHandler> handlers);
    friend CompletionHandler& when_any(std::vector<CompletionHandler> handlers);

    Job* job() const { return m_job; }

    Job* m_job = nullptr; // avoid circular references (Job <-> CompletionHandler)

    static std::atomic<int64_t> m_total_count; // for debug purposes
};

class UncheckedCompletionHandler {
public:
    UncheckedCompletionHandler(Job* job) : m_job(job)
    {
        dispatch_lifetime_log("UncheckedCompletionHandler(): n=%lld", (m_total_count.fetch_add(1) + 1));
    }

    UncheckedCompletionHandler(const UncheckedCompletionHandler& h) : m_job(h.m_job)
    {
        dispatch_lifetime_log("UncheckedCompletionHandler(): n=%lld", (m_total_count.fetch_add(1) + 1));
    }

    ~UncheckedCompletionHandler()
    {
        dispatch_lifetime_log("~UncheckedCompletionHandler(): n=%lld", (m_total_count.fetch_sub(1) - 1));
    }

    UncheckedCompletionHandler(UncheckedCompletionHandler&&) = delete;
    UncheckedCompletionHandler& operator=(const UncheckedCompletionHandler&) = delete;
    UncheckedCompletionHandler& operator=(UncheckedCompletionHandler&&) = delete;

    UncheckedCompletionHandler& then(std::function<void()> completion_cb);

private:
    friend UncheckedCompletionHandler& internal::dispatch_void(
        std::function<void()> work_void_cb, ThreadType work_thread_type, After after, std::string name
    );

    Job* job() const { return m_job; }

    Job* m_job = nullptr; // avoid circular references (Job <-> UncheckedCompletionHandler)

    static std::atomic<int64_t> m_total_count; // for debug purposes
};

class Job {
public:
    template<typename F>
    Job(F&& work_cb, std::string name = "") :
        m_work_thread_type(tls_thread_type()),
        m_work_cb(std::forward<F>(work_cb)),
        m_completion_handler(this),
        m_unchecked_completion_handler(this),
        m_when(std::chrono::milliseconds{0}),
        m_completion_thread_type(tls_thread_type()),
        m_name(name)
    {
        dispatch_lifetime_log("Job(): n=%lld", (m_total_count.fetch_add(1) + 1));
    }

    template<typename F>
    Job(F&& work_cb, ThreadType work_thread_type, std::chrono::steady_clock::time_point when, std::string name = "") :
        m_work_thread_type(work_thread_type),
        m_work_cb(std::forward<F>(work_cb)),
        m_completion_handler(this),
        m_unchecked_completion_handler(this),
        m_when(when),
        m_completion_thread_type(tls_thread_type()),
        m_name(name)
    {
        dispatch_lifetime_log("Job(): n=%lld", (m_total_count.fetch_add(1) + 1));
    }

    Job(
        std::function<void()> work_void_cb, ThreadType thread_type, std::chrono::steady_clock::time_point when,
        std::string name = ""
    ) : m_work_thread_type(thread_type),
        m_work_void_cb(std::move(work_void_cb)),
        m_completion_handler(this),
        m_unchecked_completion_handler(this),
        m_when(when),
        m_completion_thread_type(tls_thread_type()),
        m_name(name)
    {
        dispatch_lifetime_log("Job(): n=%lld", (m_total_count.fetch_add(1) + 1));
    }

    template<typename F>
    Job(F&& error_cb, Error err, ThreadType completion_thread_type, std::string name = "") :
        m_error_cb(std::forward<F>(error_cb)),
        m_err(err),
        m_completion_handler(this),
        m_unchecked_completion_handler(this),
        m_when(std::chrono::milliseconds{0}),
        m_completion_thread_type(completion_thread_type),
        m_name(name)
    {
        dispatch_lifetime_log("Job(): n=%lld", (m_total_count.fetch_add(1) + 1));
    }

    ~Job()
    {
        dispatch_lifetime_log("~Job(): n=%lld", (m_total_count.fetch_sub(1) - 1));
    }

    Job(const Job&) = delete;
    Job(Job&&) = delete;
    Job& operator=(const Job&) = delete;
    Job& operator=(Job&&) = delete;

    ThreadType work_thread_type() const { return m_work_thread_type; }

    const std::function<Error()>& work_cb() { return m_work_cb; }
    const std::function<void()>& work_void_cb() { return m_work_void_cb; }
    const std::function<void()>& completion_cb() { return m_completion_cb; }
    const std::function<void(Error)>& error_cb() { return m_error_cb; }
    void set_completion_thread_type(ThreadType completion_thread_type) { m_completion_thread_type = completion_thread_type; }
    ThreadType completion_thread_type() const { return m_completion_thread_type; }
    std::chrono::steady_clock::time_point when() const { return m_when; }

    const std::vector<std::pair<ThreadType, std::function<void(Error)>>>&
    internal_completion_cbs()
    {
        return m_internal_completion_cbs;
    }

    bool check_unhandled_error() const { return m_check_unhandled_error; }
    void set_check_unhandled_error(bool check) { m_check_unhandled_error = check; }
    void set_name(const std::string& name) { m_name = name; }
    std::string name() const { return m_name; }

    // these methods synchronize using mutex because they read/write internal state
    // that is shared across threads possibly
    Error err();
    void set_err(Error err);
    void reset_work_cb();
    void reset_work_void_cb();

private:
    CompletionHandler& completion_handler() { return m_completion_handler; }
    UncheckedCompletionHandler& unchecked_completion_handler() { return m_unchecked_completion_handler; }
    void set_completion_cb(std::function<void()> cb) { m_completion_cb = std::move(cb); }
    void set_error_cb(std::function<void(Error)> cb) { m_error_cb = std::move(cb); }
    void add_internal_completion_cb(ThreadType thread_type, std::function<void(Error)> cb);

    ThreadType m_work_thread_type;
    std::function<Error()> m_work_cb;
    std::function<void()> m_work_void_cb;
    std::function<void()> m_completion_cb;
    std::function<void(Error)> m_error_cb;
    Error m_err;
    CompletionHandler m_completion_handler;
    UncheckedCompletionHandler m_unchecked_completion_handler;
    std::chrono::steady_clock::time_point m_when;
    ThreadType m_completion_thread_type; // the thread type from which dispatch() was called originally
    std::vector<std::pair<ThreadType, std::function<void(Error)>>> m_internal_completion_cbs;
    std::mutex m_mutex;
    bool m_check_unhandled_error = true;
    bool m_is_when_all_called = false;
    bool m_is_when_any_called = false;
    bool m_is_then_called = false;
    bool m_is_error_called = false;
    std::string m_name; // for debugging purposes

    static std::atomic<int64_t> m_total_count; // for debugging purposes, counts number of Job instances created

    friend class CompletionHandler;
    friend class UncheckedCompletionHandler;
    friend CompletionHandler& internal::dispatch_with_completion(
        std::function<Error()> work_cb, ThreadType thread_type, After after, std::string name
    );
    friend UncheckedCompletionHandler& internal::dispatch_void(
        std::function<void()> work_void_cb, ThreadType work_thread_type, After after, std::string name
    );
    friend CompletionHandler& when_all(std::vector<CompletionHandler> handlers);
    friend CompletionHandler& when_any(std::vector<CompletionHandler> handlers);
};

// checks current thread's type
bool is_main_thread();
bool is_disk_thread();
bool is_network_thread();
bool is_cpu_thread();

OnThread on_thread(ThreadType thread_type);
OnThread on_current_thread();
OnThread on_main_thread();
OnThread on_disk_thread();
OnThread on_network_thread();
OnThread on_cpu_thread();

After after(std::chrono::milliseconds offset);

// template magic below to be able to use both std::function<void()> and std::function<Error()> lambda overloads,
// int template argument is dummy (but is needed)

template<
    typename F,
    typename Ret = typename std::invoke_result_t<F>,
    typename std::enable_if<std::is_same<Ret, void>::value, int>::type = 0
>
UncheckedCompletionHandler&
dispatch(F&& work_void_cb, std::string name = "")
{
    return internal::dispatch_void(std::forward<F>(work_void_cb), tls_thread_type(), after(std::chrono::milliseconds{0}), name);
}

template<
    typename F,
    typename Ret = typename std::invoke_result_t<F>,
    typename std::enable_if<std::is_same<Ret, Error>::value, int>::type = 0
>
CompletionHandler&
dispatch(F&& work_cb, std::string name = "")
{
    return internal::dispatch_with_completion(
        std::forward<F>(work_cb), tls_thread_type(), after(std::chrono::milliseconds{0}), name
    );
}

template<
    typename F,
    typename Ret = typename std::invoke_result_t<F>,
    typename std::enable_if<std::is_same<Ret, void>::value, int>::type = 0
>
UncheckedCompletionHandler&
dispatch(OnThread on_thread, F&& work_void_cb, std::string name = "")
{
    return internal::dispatch_void(
        std::forward<F>(work_void_cb), on_thread.value, after(std::chrono::milliseconds{0}), name
    );
}

template<
    typename F,
    typename Ret = typename std::invoke_result_t<F>,
    typename std::enable_if<std::is_same<Ret, Error>::value, int>::type = 0
>
CompletionHandler&
dispatch(OnThread on_thread, F&& work_cb, std::string name = "")
{
    return internal::dispatch_with_completion(
        std::forward<F>(work_cb), on_thread.value, after(std::chrono::milliseconds{0}), name
    );
}

template<
    typename F,
    typename Ret = typename std::invoke_result_t<F>,
    typename std::enable_if<std::is_same<Ret, void>::value, int>::type = 0
>
UncheckedCompletionHandler&
dispatch(OnThread on_thread, After after, F&& work_void_cb, std::string name = "")
{
    return internal::dispatch_void(std::forward<F>(work_void_cb), on_thread.value, after, name);
}

template<
    typename F,
    typename Ret = typename std::invoke_result_t<F>,
    typename std::enable_if<std::is_same<Ret, Error>::value, int>::type = 0
>
CompletionHandler&
dispatch(OnThread on_thread, After after, F&& work_cb, std::string name = "")
{
    return internal::dispatch_with_completion(std::forward<F>(work_cb), on_thread.value, after, name);
}

CompletionHandler& when_all(std::vector<CompletionHandler> handlers);
CompletionHandler& when_any(std::vector<CompletionHandler> handlers);

} // namespace orion