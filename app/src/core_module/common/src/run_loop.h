#pragma once

#include <functional>
#include <src/error.h>
#include <src/dispatch.h>
#include <uv.h>
#include <src/thread_type.h>
#include <deque>
#include <queue>
#include <memory>
#include <atomic>
#include <set>
#include <map>

namespace orion {

extern std::atomic<int> global_dispatch_arg_count;
extern std::atomic<int> global_run_loop_count;

// Do not declare IterationCb as std::function - this would enable capturing lambdas
// and that could lead to (soft) memory leaks and issues with shared pointers (circular
// referenced objects never being destroyed).
typedef bool (*IterationCb)(void* user_data);

class Job;
class RunLoop;

void dispatch_close_cb(uv_handle_t* handle);

// Platform-independent, returns name of the current thread, calls just
// uv_thread_self() and uv_thread_getname() internally (does not use TLS).
std::string current_thread_name();

// Baton pattern: https://docs.libuv.org/en/v1.x/guide/utilities.html#baton
struct DispatchArg {
    DispatchArg()
    {
        global_dispatch_arg_count.fetch_add(1, std::memory_order_relaxed);
    }

    ~DispatchArg()
    {
        global_dispatch_arg_count.fetch_sub(1, std::memory_order_relaxed);
    }

    uv_idle_t idler;
    uv_timer_t timer;
    std::shared_ptr<Job> job;
    int timer_counter;    // for debugging
    uint64_t timer_start; // for debugging
    std::weak_ptr<RunLoop> run_loop;
};

struct DispatchArgDeleter {
    void
    operator()(DispatchArg* arg) 
    {
        if (arg) {
            // It is safer to stop the timer (potentially again) before closing it,
            // multiple calls to uv_timer_stop() should be safe.
            uv_timer_stop(&arg->timer);

            // Note that uv_close does not close the handle synchronously (immediately),
            // it just schedules the handle to be closed on next event loop iteration.
            // So we cannot delete the Dispatch object here immediately and we have
            // to use our dispatch_close_cb (again, asynchronously called) callback.
            uv_close((uv_handle_t*)&arg->timer, dispatch_close_cb);
        }
    }
};

class Job;

class RunLoop : public std::enable_shared_from_this<RunLoop>{
public:
    ~RunLoop();

public:
    static std::shared_ptr<RunLoop> create(ThreadType thread_type);
    static RunLoop& current();
    RunLoop(ThreadType thread_type);
    ThreadType thread_type();
    void init();
    void run();
    void wakeup();
    void join();

    void set_iteration(IterationCb cb, void* user_data, std::chrono::milliseconds period);
    IterationCb get_iteration() const;
    void* get_iteration_user_data() const;
    bool run_iteration();

    // Optional hook called from wakeup() whenever a background thread posts work
    // to this loop. Useful for integrating with a foreign event loop (e.g. Win32
    // message loop) that needs to be nudged to call pump() promptly.
    // Must be set before any background threads start posting work.
    void set_wakeup_hook(std::function<void()> hook) { m_wakeup_hook = std::move(hook); }

    // Creates and initializes the main run loop.
    static std::shared_ptr<RunLoop> init_main_loop();

    // Starts the main run loop. When the run loop stops, it is deleted.
    static void start_main(RunLoop* run_loop);

    // Non-blocking alternative to start_main() for environments where the caller
    // drives the run loop externally (e.g. a UI framework timer).
    // Call repeatedly from the UI thread instead of start_main().
    // Returns false once the loop has fully stopped (stop() called internally),
    // after which the on-stop callback has already fired — do not pump again.
    bool pump();

    Error request_stop_main(std::function<void()> cb);

    void dispatch(std::shared_ptr<Job> job);

    // Use std::unique_ptr to force std::move() of DispatchArg
    void add_timer_dispatch_arg(std::unique_ptr<DispatchArg, DispatchArgDeleter> arg);
    size_t remove_timer_dispatch_arg(DispatchArg* arg);
    void clear_dispatch_args();

    bool is_main_thread() const;
    bool is_disk_thread() const;
    bool is_network_thread() const;
    bool is_cpu_thread() const;

private:
    void stop();
    void stop_queue() { m_stop_queue = true; }
    void request_stop() { m_is_stop_requested = true; }

private:
    void enqueue_thread_job(std::shared_ptr<Job> job);
    void async_cb(uv_async_t* async);

private:
    RunLoop() = delete;

    IterationCb m_iteration_cb = nullptr;
    void* m_iteration_cb_user_data = nullptr;
    std::chrono::milliseconds m_iteration_period;
    uv_loop_t m_loop;
    uv_async_t m_async;
    int m_timer_counter = 0;
    std::deque<std::shared_ptr<Job>> m_job_queue;
    std::mutex m_queue_mutex;
    std::atomic<bool> m_is_stop_requested = false;
    std::atomic<bool> m_is_stopped{false};
    std::atomic<bool> m_pump_stopped{false}; // set in async_cb before uv_stop() on main loop
    std::atomic<ThreadType> m_thread_type;
    uv_timer_t m_ui_timer;                        // only relevant for the main run loop
    bool m_stop_queue = false;                    // used to stop the run loop when all queues are empty
    std::function<void()> m_on_stop_main_loop_cb; // callback to be called when the main loop stops
    std::function<void()> m_wakeup_hook;           // optional; called inside wakeup() when not stopped

    // Enables correct stop, close, and deallocation of timers and their libuv handles.
    std::map<DispatchArg*, std::unique_ptr<DispatchArg, DispatchArgDeleter>> dispatch_arg_container;

private:
    friend void thread_launch(void* thread_launch_data);
    friend void init_main_loop();
    friend void ui_timer_cb(uv_timer_t* ui_timer);
    friend void enqueue_thread_job(std::shared_ptr<Job> job);
    friend void perform_job_or_enqueue_on_thread(RunLoop* run_loop, std::shared_ptr<Job> job);
};

void ui_timer_cb(uv_timer_t* ui_timer);
void enqueue_thread_job(std::shared_ptr<Job> job);
void perform_job_or_enqueue_on_thread(RunLoop* run_loop, std::shared_ptr<Job> job);
RunLoop* tls_run_loop();
ThreadType tls_thread_type();

} // namespace orion