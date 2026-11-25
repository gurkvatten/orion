#define ENABLE_VERBOSE_LOGS 0

#include <src/run_loop.h>
#include <uv.h>
#include <assert.h>
#include <queue>
#include <src/defer.h>
#include <mutex>
#include <condition_variable>

namespace orion {

std::atomic<int> global_dispatch_arg_count = 0;
std::atomic<int> global_run_loop_count = 0;

static std::atomic<int> g_queued_jobs = 0;

struct ThreadData {
    RunLoop* run_loop;
};

static ThreadData&
thread_data()
{
    static thread_local ThreadData thread_data_storage;
    return thread_data_storage;
}

class RunLoopRepository {
public:
    std::shared_ptr<RunLoop>
    add(uv_thread_t thread, ThreadType thread_type)
    {
        std::shared_ptr<RunLoop> run_loop = RunLoop::create(thread_type);
        std::unique_lock<std::mutex> lk(m_mux);
        m_run_loops.push_back(run_loop);
        m_threads.push_back(thread);
        return run_loop;
    }

    void
    remove(RunLoop* run_loop)
    {
        std::unique_lock<std::mutex> lk(m_mux);
        m_run_loops.erase(std::remove_if(m_run_loops.begin(), m_run_loops.end(), [run_loop](std::weak_ptr<RunLoop>& it) {
            auto run_loop_ptr = it.lock();
            return run_loop_ptr.get() == run_loop;
        }), m_run_loops.end());
    }

    std::vector<std::shared_ptr<RunLoop>>
    all_run_loops()
    {
        std::unique_lock<std::mutex> lk(m_mux);
        std::vector<std::shared_ptr<RunLoop>> all;
        for(auto& run_loop_ptr : m_run_loops) {
            auto run_loop = run_loop_ptr.lock();
            assert(run_loop);
            all.push_back(run_loop);
        }
        return all;
    }

    std::vector<uv_thread_t>
    all_platform_threads()
    {
        std::unique_lock<std::mutex> lk(m_mux);
        return m_threads;
    }

    std::shared_ptr<RunLoop>
    any(ThreadType thread_type)
    {
        std::unique_lock<std::mutex> lk(m_mux);
        for(auto run_loop_ptr : m_run_loops) {
            auto run_loop = run_loop_ptr.lock();
            assert(run_loop);
            if(run_loop->thread_type() == thread_type) {
                return run_loop;
            }
        }
        return nullptr;
    }
private:
    std::mutex m_mux;
    std::vector<std::weak_ptr<RunLoop>> m_run_loops;
    std::vector<uv_thread_t> m_threads;
};

static RunLoopRepository&
run_loop_repository()
{
    static RunLoopRepository instance;
    return instance;
}

static ThreadType g_background_thread_types[] = {
    // do not start special thread for main/ui thread type
    ThreadType::disk,
    ThreadType::network,
    ThreadType::cpu,
};

std::string
current_thread_name()
{
    // Max thread name length on different platforms:
    // - Linux: 16 bytes (including null terminator)
    // - macOS: 64 bytes (including null terminator)
    // - Windows: 32,767 WCHAR (technically, fully Unicode UTF-16)
    // - Android: 16 bytes (including null terminator)
    char name[64] = {0};

    auto t = uv_thread_self();
    int result = uv_thread_getname(&t, name, sizeof(name));
    if (result != 0) {
        orion::log_warning("uv_thread_getname() failed: %s", uv_strerror(result));
        return std::string("<unknown>");
    }
    return std::string(name);
}

std::shared_ptr<RunLoop>
RunLoop::create(ThreadType thread_type)
{
    log_verbose("RunLoop::create() [called on: %s] run loop thread type: %s",
        current_thread_name().c_str(), // do not use tls_thread_type() here as it is not initialized yet
        to_string(thread_type).c_str()
    );

    return std::make_shared<RunLoop>(thread_type);
}

RunLoop::RunLoop(ThreadType thread_type)
{
    global_run_loop_count.fetch_add(1, std::memory_order_relaxed);

    // Note that we cannot use tls_thread_type() here because it is not initialized yet
    m_thread_type = thread_type;

    log_verbose("RunLoop::RunLoop() [called on: %s] run loop thread type: %s",
        current_thread_name().c_str(), // do not use tls_thread_type() here as it is not initialized yet
        to_string(thread_type).c_str()
    );

    uv_loop_init(&m_loop);
}

// Note that this destructor may be called on a different thread than the corresponding constructor
RunLoop::~RunLoop()
{
    global_run_loop_count.fetch_sub(1, std::memory_order_relaxed);

    log_verbose("RunLoop::~RunLoop() [called on: %s] run loop thread type: %s",
        to_string(tls_thread_type()).c_str(),
        to_string(m_thread_type).c_str() // access to m_thread_type is safe here because it is atomic
    );
}

RunLoop&
RunLoop::current()
{
    return *thread_data().run_loop;
}

ThreadType
RunLoop::thread_type()
{
    return m_thread_type;
}

void
enqueue_thread_job(std::shared_ptr<Job> job)
{
    ThreadType target_thread_type;
    if (job->work_cb() || job->work_void_cb()) {
        target_thread_type = job->work_thread_type();
    } else {
        target_thread_type = job->completion_thread_type();
    }

    log_verbose("enqueue_thread_job(): %s -> %s",
        to_string(tls_thread_type()).c_str(), to_string(target_thread_type).c_str()
    );

    // Note: we cannot send data to a different thread calling uv_async_send()
    // directly (using for example uv_async_t.data member) - we have to use
    // some kind of synchronization primitive (eg., mutex), and in combination
    // with a queue, this provides the needed thread-safe tool to communicate
    // with a different thread. Then we can notify the thread's run loop calling
    // uv_async_send(). So, uv_async_send() is just a notification (signal).

    auto run_loop = run_loop_repository().any(target_thread_type);
    if(!run_loop) {
        log_error("Unable to enqueue job. Target thread doesn't exist.");
        return;
    }
    run_loop->enqueue_thread_job(job);
}

void
RunLoop::enqueue_thread_job(std::shared_ptr<Job> job)
{
    std::unique_lock<std::mutex> lk(m_queue_mutex);
    if (!m_stop_queue) {
        g_queued_jobs.fetch_add(1, std::memory_order_relaxed);
        m_job_queue.push_back(job); // push to the thread's queue
        wakeup(); // send a signal to the thread
    }
}

void
log_no_perform(std::shared_ptr<Job> job)
{
    log_verbose("perform_job_or_enqueue_on_thread[%s]: not performing job: work=%s completion=%s name=%s",
        to_string(tls_thread_type()).c_str(),
        to_string(job->work_thread_type()).c_str(),
        to_string(job->completion_thread_type()).c_str(),
        job->name().c_str()
    );
}

void
perform_job_or_enqueue_on_thread(RunLoop* run_loop, std::shared_ptr<Job> job)
{
    // note: when the run loop stop is requested, we do not want to perform jobs (or their completion callbacks)
    // that should be performed on the main thread

    auto err = Error();
    auto is_completed = true;

    if (job->work_cb()) {
        log_verbose("perform_job_or_enqueue_on_thread[%s]: work", to_string(tls_thread_type()).c_str());
        if (job->work_thread_type() == tls_thread_type()) {
            if (!run_loop->m_is_stop_requested || job->work_thread_type() != ThreadType::main) {
                err = job->work_cb()(); // invoke work callback
                job->set_err(err); // this is needed for internal completion handler
                if (err) {
                    if (job->error_cb()) {
                        assert((int)job->completion_thread_type() >= 0 && (int)job->completion_thread_type() < MAX_THREAD_TYPE);
                        if (!run_loop->m_is_stop_requested || job->completion_thread_type() != ThreadType::main) {
                            // cannot std::move error_cb() here (would cause a compile error), not sure why
                            internal::dispatch_error_cb(job->error_cb(), err, job->completion_thread_type());
                        } else {
                            log_no_perform(job);
                        }
                    } else if (job->check_unhandled_error()) {
                        log_error("unhandled error in dispatch on %s (%s): %s",
                            to_string(tls_thread_type()).c_str(), err.code_name().c_str(), err.to_string().c_str()
                        );
                    }
                } else {
                    if (job->completion_cb()) {
                        if (!run_loop->m_is_stop_requested || job->completion_thread_type() != ThreadType::main) {
                            // cannot std::move completion_cb() here (would cause a compile error), not sure why
                            internal::dispatch_void(job->completion_cb(), job->completion_thread_type(), after(std::chrono::milliseconds{0}), "");
                        } else {
                            log_no_perform(job);
                        }
                    }
                }
            } else {
                log_no_perform(job);
            }
        } else {
            // dispatch the work on a different thread's run loop
            if (!run_loop->m_is_stop_requested || job->work_thread_type() != ThreadType::main) {
                enqueue_thread_job(job);
            } else {
                log_no_perform(job);
            }
            is_completed = false;
        }
    } else if (job->work_void_cb()) {
        log_verbose("perform_job_or_enqueue_on_thread[%s]: work_void", to_string(tls_thread_type()).c_str());
        if (job->work_thread_type() == tls_thread_type()) {
            if (!run_loop->m_is_stop_requested || job->work_thread_type() != ThreadType::main) {
                job->work_void_cb()(); // invoke work callback
            } else {
                log_no_perform(job);
            }
            if (job->completion_cb()) {
                if (job->completion_thread_type() == tls_thread_type()) {
                    if (!run_loop->m_is_stop_requested || job->completion_thread_type() != ThreadType::main) {
                        job->completion_cb()(); // invoke completion callback immediately
                    } else {
                        log_no_perform(job);
                    }
                } else {
                    job->reset_work_void_cb(); // mark the work as done
                    assert(!job->work_void_cb());
                    if (!run_loop->m_is_stop_requested || job->completion_thread_type() != ThreadType::main) {
                        enqueue_thread_job(job);
                    } else {
                        log_no_perform(job);
                    }
                    is_completed = false;
                }
            }
        } else {
            // dispatch the work on a different thread's run loop
            if (!run_loop->m_is_stop_requested || job->work_thread_type() != ThreadType::main) {
                enqueue_thread_job(job);
            } else {
                log_no_perform(job);
            }
            is_completed = false;
        }
    } else if (job->completion_cb()) {
        log_verbose("perform_job_or_enqueue_on_thread[%s]: then", to_string(tls_thread_type()).c_str());
        if (job->completion_thread_type() == tls_thread_type()) {
            if (!run_loop->m_is_stop_requested || job->completion_thread_type() != ThreadType::main) {
                job->completion_cb()(); // invoke completion callback
            } else {
                log_no_perform(job);
            }
        } else {
            // dispatch the completion on a different thread's run loop
            if (!run_loop->m_is_stop_requested || job->completion_thread_type() != ThreadType::main) {
                enqueue_thread_job(job);
            } else {
                log_no_perform(job);
            }
            is_completed = false;
        }
    } else if (job->error_cb()) {
        log_verbose("perform_job_or_enqueue_on_thread[%s]: error", to_string(tls_thread_type()).c_str());
        if (job->completion_thread_type() == tls_thread_type()) {
            if (!run_loop->m_is_stop_requested || job->completion_thread_type() != ThreadType::main) {
                job->error_cb()(job->err()); // invoke error callback
            } else {
                log_no_perform(job);
            }
        } else {
            // dispatch the error completion on a different thread's run loop
            if (!run_loop->m_is_stop_requested || job->completion_thread_type() != ThreadType::main) {
                enqueue_thread_job(job);
            } else {
                log_no_perform(job);
            }
            is_completed = false;
        }
    }

    if (is_completed && !job->internal_completion_cbs().empty()) {
        for (const auto& pair : job->internal_completion_cbs()) {
            assert((int)job->completion_thread_type() >= 0 && (int)job->completion_thread_type() < MAX_THREAD_TYPE);
            auto& thread_type = pair.first;
            auto& cb = pair.second;
            if (!run_loop->m_is_stop_requested || thread_type != ThreadType::main) {
                internal::dispatch_error_cb(cb, job->err(), thread_type);
            } else {
                log_no_perform(job);
            }
        }
    }
}

// Baton pattern: https://docs.libuv.org/en/v1.x/guide/utilities.html#baton
struct UiTimer {
    IterationCb iteration_cb;
    void* iteration_cb_user_data = nullptr;
    std::chrono::milliseconds iteration_period;
    uv_loop_t* loop = nullptr;
    RunLoop* run_loop = nullptr;
};

void
ui_timer_cb(uv_timer_t* ui_timer)
{
    auto arg = (UiTimer*)ui_timer->data;
    //log_verbose("ui_timer_cb(): iteration_cb=%p user_data=%p", arg->iteration_cb, arg->iteration_cb_user_data);

    if (arg->iteration_cb == nullptr) {
        log_verbose("ui_timer_cb(): no iteration callback set, nothing to do");
        return; // no iteration callback set, nothing to do
    }

    if (!arg->iteration_cb(arg->iteration_cb_user_data)) {
        // ignore the return value of the iteration callback at the moment,
        // in the future, this can be used to signal the run loop to stop
        log_verbose("ui_timer_cb(): ui iteration cb returned 0");
    }
}

void
RunLoop::async_cb(uv_async_t* async)
{
    decltype(m_job_queue) job_queue;
    std::unique_lock<std::mutex> lk(m_queue_mutex);
    job_queue.swap(m_job_queue);
    lk.unlock();

    while(!job_queue.empty()) {
        auto& job = job_queue.front();
        perform_job_or_enqueue_on_thread(this, job);
        job_queue.pop_front();
        g_queued_jobs.fetch_sub(1);
    }

    if (m_is_stop_requested) {
        // check if all queues on all threads are empty

        bool all_empty = false;
        if(0 == g_queued_jobs.load(std::memory_order_relaxed)) {
            std::atomic_thread_fence(std::memory_order_acquire);
            all_empty = true;
        }

        if (all_empty) {
            for(auto& run_loop : run_loop_repository().all_run_loops()) {
                run_loop->stop_queue();
                run_loop->wakeup();
            }
        }

        if (all_empty) {
            log_verbose("idle_cb() [%s]: all queues are empty, stopping run loop", 
                to_string(tls_thread_type()).c_str()
            );

            if (is_main_thread())
                m_pump_stopped.store(true, std::memory_order_relaxed);

            uv_stop(&m_loop); // stop the libuv event loop
        }
    }
}

struct ThreadLaunchData {
    ThreadType thread_type;
    bool ready = false;
    std::mutex m_launch_mux;
    std::condition_variable m_launch_cv;
};

void
RunLoop::init()
{
    uv_async_init(&m_loop, &m_async, [](uv_async_t* async) { ((RunLoop*)async->data)->async_cb(async); });
    m_async.data = this;
}

void
RunLoop::run()
{
    // start thread run loop
    uv_run(&m_loop, UV_RUN_DEFAULT);
    stop();
}

bool RunLoop::pump() {
    uv_run(&m_loop, UV_RUN_NOWAIT);
    if (m_pump_stopped.load(std::memory_order_relaxed)) {
        stop();
        if (m_on_stop_main_loop_cb)
            m_on_stop_main_loop_cb();
        return false;
    }
    return true;
}

void RunLoop::wakeup() {
    if (!m_is_stopped.load(std::memory_order_acquire)) {
        uv_async_send(&m_async);
        if (m_wakeup_hook)
            m_wakeup_hook();
    }
}

void
thread_launch(void* arg)
{
    ThreadLaunchData& thread_launch_data = *(ThreadLaunchData*)arg;
    std::unique_lock<std::mutex> lk(thread_launch_data.m_launch_mux);

    auto thread_type = thread_launch_data.thread_type;
    auto thread_type_name = to_string(thread_type);
    uv_thread_setname(to_string(thread_type).c_str());

    auto run_loop = run_loop_repository().add(uv_thread_self(), thread_type);
    thread_data().run_loop = run_loop.get();
    run_loop->init();

    thread_launch_data.ready = true;
    thread_launch_data.m_launch_cv.notify_all();
    lk.unlock();

    run_loop->run();

    orion::log_verbose("exiting run loop for thread: %s", thread_type_name.c_str());

    run_loop_repository().remove(run_loop.get());
}

std::shared_ptr<RunLoop>
RunLoop::init_main_loop()
{
    auto thread_type = ThreadType::main;
    uv_thread_setname(to_string(thread_type).c_str());
    auto run_loop = run_loop_repository().add(uv_thread_self(), thread_type);
    thread_data().run_loop = run_loop.get();
    run_loop->init();

    // background thread setup
    for (auto thread_type : g_background_thread_types) {
        ThreadLaunchData thread_launch_data;
        thread_launch_data.thread_type = thread_type;
        std::unique_lock<std::mutex> lk(thread_launch_data.m_launch_mux);
        uv_thread_t thread;
        uv_thread_create(&thread, thread_launch, &thread_launch_data);
        while(!thread_launch_data.ready) {
            thread_launch_data.m_launch_cv.wait(lk);
        }
    }

    return run_loop;
}

void
RunLoop::start_main(RunLoop* run_loop)
{
    UiTimer arg{}; // note: this variable must still be alive when uv_run() is called below
    
    if (run_loop->m_iteration_cb) {
        // UI (for example GLib + GTK)
        uv_timer_init(&run_loop->m_loop, &run_loop->m_ui_timer);
        arg.loop = &run_loop->m_loop;
        arg.iteration_cb = run_loop->m_iteration_cb;
        arg.iteration_cb_user_data = run_loop->m_iteration_cb_user_data;
        arg.run_loop = run_loop;
        run_loop->m_ui_timer.data = &arg;
        log_verbose("RunLoop::start_main() [%s]: starting UI timer with period %ldms (cb=%p)",
            to_string(tls_thread_type()).c_str(),
            run_loop->m_iteration_period.count(),
            run_loop->m_iteration_cb
        );
        uv_timer_start(&run_loop->m_ui_timer, ui_timer_cb, 0, run_loop->m_iteration_period.count());
    }

    log_verbose("RunLoop::start_main() [%s]: starting main run loop",
        to_string(tls_thread_type()).c_str()
    );

    run_loop->run();

    log_verbose("RunLoop::start_main() [%s]: main run loop stopped",
        to_string(tls_thread_type()).c_str()
    );

    if (run_loop->m_on_stop_main_loop_cb) {
        run_loop->m_on_stop_main_loop_cb();
    }
}

Error
RunLoop::request_stop_main(std::function<void()> cb)
{
    if (!is_main_thread()) {
        RAISE(ErrorCode::invalid_thread, "RunLoop::request_stop_main() must be called from the main thread");
    }

    log_verbose("main run loop stop requested");

    if (m_is_stop_requested) {
        // already requested to stop, nothing to do
        SUCCEED();
    }
    m_is_stop_requested = true;

    m_on_stop_main_loop_cb = cb;

    for(auto& run_loop : run_loop_repository().all_run_loops()) {
        if(run_loop->is_main_thread()) {
            continue;
        }
        run_loop->request_stop();
        run_loop->wakeup();
    }

    SUCCEED();
}

void
RunLoop::stop()
{
    if (m_is_stopped) {
        return; // already stopped
    }

    log_verbose("RunLoop::stop() [%s]: stopping run loop", to_string(m_thread_type).c_str());

    if (m_thread_type == ThreadType::main) {
        // background thread cleanup
        for(auto& thread : run_loop_repository().all_platform_threads()) {
            uv_thread_t self = uv_thread_self();
            if(uv_thread_equal(&self, &thread) != 0) {
                continue;
            }
            uv_thread_join(&thread);
        }

        if (m_iteration_cb) {
            // stop the UI timer
            uv_timer_stop(&m_ui_timer);
            uv_close((uv_handle_t*)&m_ui_timer, NULL);
        }
    }

    // We have to close each libuv handle before closing the loop.
    // Set m_is_stopped before uv_close() so concurrent wakeup() calls on this
    // loop from other threads see it and skip uv_async_send() on the closing handle.
    m_is_stopped = true;
    uv_close((uv_handle_t*)&m_async, nullptr);

    // This correctly stops, closes, and deallocates remaining timers and their libuv handles (if any)
    clear_dispatch_args();

    const int max_iterations = 1000; // heuristic to detect infinite loop (eg., if there are still handles left in the loop)
    int iterations = 0;
    
    while (uv_loop_close(&m_loop) == UV_EBUSY) {
        if (iterations == max_iterations) {
            orion::log_error("RunLoop::stop() [%s]: uv_loop_close() returned UV_EBUSY after %d iterations, unclosed libuv handles may be present",
                to_string(m_thread_type).c_str(), iterations
            );
            
            // Walk through all remaining libuv handles and log them
            uv_walk(&m_loop, [](uv_handle_t* handle, void* arg) {
                log_warning("RunLoop::stop() [%s]: unclosed handle: %s",
                    to_string(tls_thread_type()).c_str(),
                    uv_handle_type_name(handle->type)
                );
            }, nullptr);

            break; // give up trying
        }
        ++iterations;

        uv_run(&m_loop, UV_RUN_ONCE);
    }

    log_verbose("RunLoop::stop() [%s]: run loop stopped", to_string(m_thread_type).c_str());
}

void
RunLoop::set_iteration(IterationCb cb, void* user_data, std::chrono::milliseconds period)
{
    assert(m_iteration_cb == nullptr); // can be set only once
    log_verbose("RunLoop::set_iteration() [%s]: setting iteration callback with period %ldms (cb=%p)",
        to_string(tls_thread_type()).c_str(), period.count(), cb
    );
    m_iteration_cb = cb;
    m_iteration_cb_user_data = user_data;
    m_iteration_period = period;
}

IterationCb
RunLoop::get_iteration() const
{
    return m_iteration_cb;
}

void*
RunLoop::get_iteration_user_data() const
{
    return m_iteration_cb_user_data;
}

bool
RunLoop::run_iteration()
{
    if (m_iteration_cb) {
        return m_iteration_cb(m_iteration_cb_user_data);
    }
    return false;
}

void
dispatch_close_cb(uv_handle_t* handle)
{
    auto timer = (uv_timer_t*)handle;
    auto arg = (DispatchArg*)timer->data;
    delete arg;
}

void
dispatch_timer_cb(uv_timer_t* timer)
{
    auto arg = (DispatchArg*)timer->data;
    log_verbose("dispatch timer[%s %d]: after %ldms",
        to_string(tls_thread_type()).c_str(), arg->timer_counter, uv_now(timer->loop) - arg->timer_start
    );
    assert(arg->job);

    auto run_loop = arg->run_loop.lock();
    assert(run_loop);
    perform_job_or_enqueue_on_thread(run_loop.get(), arg->job);

    uv_timer_stop(timer);
    // Do not close the timer handle here

    run_loop->remove_timer_dispatch_arg(arg);
}

void
RunLoop::add_timer_dispatch_arg(std::unique_ptr<DispatchArg, DispatchArgDeleter> arg)
{
    if (dispatch_arg_container.find(arg.get()) != dispatch_arg_container.end()) {
        log_error("RunLoop::add_timer_dispatch_arg(): arg already exists in DispatchArg container");
    }
    dispatch_arg_container[arg.get()] = std::move(arg);
}

size_t
RunLoop::remove_timer_dispatch_arg(DispatchArg* arg)
{
    if (dispatch_arg_container.find(arg) == dispatch_arg_container.end()) {
        log_error("RunLoop::remove_timer_dispatch_arg(): arg does not exist in DispatchArg container");
    }
    return dispatch_arg_container.erase(arg);
}

void
RunLoop::clear_dispatch_args()
{
    dispatch_arg_container.clear();
}

void
RunLoop::dispatch(std::shared_ptr<Job> job)
{
    auto after = std::chrono::duration_cast<std::chrono::milliseconds>(job->when() - std::chrono::steady_clock::now());

    if (after.count() <= 0) {
        // the work is due immediately
        if (!m_stop_queue) {
            std::unique_lock<std::mutex> lk(m_queue_mutex);
            g_queued_jobs.fetch_add(1, std::memory_order_relaxed);
            m_job_queue.push_back(job);
            wakeup();
        }
        return;
    }

    // if the stop is requested, then stop dispatching timers completely
    if (m_is_stop_requested) {
        log_debug("RunLoop::dispatch() [%s]: stop was requested, not dispatching timer job: work=%s completion=%s name=%s",
            to_string(tls_thread_type()).c_str(),
            to_string(job->work_thread_type()).c_str(),
            to_string(job->completion_thread_type()).c_str(),
            job->name().c_str()
        );
        return; // do not dispatch jobs after stop was requested
    }

    // We need the ctor to be called -> use new instead of malloc
    std::unique_ptr<DispatchArg, DispatchArgDeleter> arg(new DispatchArg());

    // We need to store the pointer to the job (not its callbacks) because both completion callback
    // and error callback may be set later:
    //   dispatch([]{})
    //     .then([]{})  // sets completion handler after dispatch() call returned
    //     .error([]{}) // sets error handler after dispatch() call returned
    arg->job = job;

    auto run_loop = shared_from_this();
    
    uv_timer_init(&m_loop, &arg->timer);
    arg->timer.data = arg.get();
    arg->timer_counter = ++m_timer_counter;
    arg->timer_start = uv_now(&m_loop);
    arg->run_loop = run_loop;

    // This causes the timing of the timer below to be more precise,
    // especially if a lot of work (or long sleep) happened in the same
    // event loop iteration right before the dispatch call.
    uv_update_time(&m_loop);

    uv_timer_start(&arg->timer, dispatch_timer_cb, after.count(), 0);

    run_loop->add_timer_dispatch_arg(std::move(arg));
}

ThreadType
tls_thread_type()
{
    return RunLoop::current().thread_type();
}

RunLoop*
tls_run_loop()
{
    return &RunLoop::current();
}

bool
RunLoop::is_main_thread() const
{
    return m_thread_type == ThreadType::main;
}

bool
RunLoop::is_disk_thread() const
{
    return m_thread_type == ThreadType::disk;
}

bool
RunLoop::is_network_thread() const
{
    return m_thread_type == ThreadType::network;
}

bool
RunLoop::is_cpu_thread() const
{
    return m_thread_type == ThreadType::cpu;
}

} // namespace orion
