#include <src/dispatch.h>
#include <memory>
#include <assert.h>
#include <src/run_loop.h>

namespace orion {

namespace internal {

CompletionHandler&
dispatch_with_completion(std::function<Error()> work_cb, ThreadType work_thread_type, After after, std::string name)
{
    assert(tls_run_loop());
    auto job = std::make_shared<Job>(std::move(work_cb), work_thread_type, after.when, name);
    job->set_completion_thread_type(tls_thread_type());
    auto& handler = job->completion_handler();
    tls_run_loop()->dispatch(job);
    return handler;
}

UncheckedCompletionHandler&
dispatch_void(std::function<void()> work_void_cb, ThreadType work_thread_type, After after, std::string name)
{
    assert(tls_run_loop());
    auto job = std::make_shared<Job>(std::move(work_void_cb), work_thread_type, after.when, name);
    job->set_completion_thread_type(tls_thread_type());
    auto& handler = job->unchecked_completion_handler();
    tls_run_loop()->dispatch(job);
    return handler;
}

void
dispatch_error_cb(std::function<void(Error)> error_cb, Error err, ThreadType thread_type)
{
    auto job = std::make_shared<Job>(std::move(error_cb), err, thread_type);
    tls_run_loop()->dispatch(job);
}

// converts Error() function into void() function
std::function<void()>
ignore_error(std::function<Error()> work_cb)
{
    return std::move([work_cb = std::move(work_cb)]{
        work_cb(); // ignore Error result
    });
}

} // namespace internal

std::atomic<int64_t> Job::m_total_count{0};
std::atomic<int64_t> CompletionHandler::m_total_count{0};
std::atomic<int64_t> UncheckedCompletionHandler::m_total_count{0};

CompletionHandler&
CompletionHandler::then(std::function<void()> completion_cb)
{
    assert(m_job);
    assert(!m_job->m_is_then_called && "redundant then() call on the same dispatch()");
    m_job->m_is_then_called = true;
    m_job->set_completion_cb(std::move(completion_cb));
    return *this;
}

CompletionHandler&
CompletionHandler::error(std::function<void(Error)> error_cb)
{
    assert(m_job);
    assert(!m_job->m_is_error_called && "redundant error() call on the same dispatch()");
    m_job->m_is_error_called = true;
    m_job->set_error_cb(std::move(error_cb));
    return *this;
}

CompletionHandler&
CompletionHandler::then_or_error(std::function<void()> completion_cb)
{
    assert(m_job);
    assert(!m_job->m_is_then_called && !m_job->m_is_error_called && "redundant then_or_error() on the same dispatch()");
    m_job->m_is_then_called = true;
    m_job->m_is_error_called = true;
    m_job->add_internal_completion_cb(m_job->completion_thread_type(), [completion_cb = std::move(completion_cb)](Error err){
        // ignore a potential error
        completion_cb();
    });
    m_job->set_check_unhandled_error(false);
    return *this;
}

CompletionHandler&
CompletionHandler::then_or_error(std::function<void(Error)> completion_cb)
{
    assert(m_job);
    assert(!m_job->m_is_then_called && !m_job->m_is_error_called && "redundant then_or_error() on the same dispatch()");
    m_job->m_is_then_called = true;
    m_job->m_is_error_called = true;
    m_job->add_internal_completion_cb(m_job->completion_thread_type(), [completion_cb = std::move(completion_cb)](Error err){
        // ignore a potential error
        completion_cb(err);
    });
    m_job->set_check_unhandled_error(false);
    return *this;
}

UncheckedCompletionHandler&
UncheckedCompletionHandler::then(std::function<void()> completion_cb)
{
    assert(m_job);
    assert(!m_job->completion_cb() && "multiple then() calls on the same dispatch()");
    m_job->set_completion_cb(std::move(completion_cb));
    return *this;
}

bool
is_main_thread()
{
    return tls_thread_type() == ThreadType::main;
}

bool
is_disk_thread()
{
    return tls_thread_type() == ThreadType::disk;
}

bool
is_network_thread()
{
    return tls_thread_type() == ThreadType::network;
}

bool
is_cpu_thread()
{
    return tls_thread_type() == ThreadType::cpu;
}

std::string
to_string(ThreadType thread_type)
{
    switch (thread_type) {
    case ThreadType::main:
        return "main";
    case ThreadType::disk:
        return "disk";
    case ThreadType::network:
        return "network";
    case ThreadType::cpu:
        return "cpu";
    default:
        return "unknown";
    }
}

OnThread
on_thread(ThreadType thread_type)
{
    return OnThread(thread_type);
}

OnThread
on_current_thread()
{
    return OnThread(tls_thread_type());
}

OnThread
on_main_thread()
{
    return OnThread(ThreadType::main);
}

OnThread
on_disk_thread()
{
    return OnThread(ThreadType::disk);
}

OnThread
on_network_thread()
{
    return OnThread(ThreadType::network);
}

OnThread
on_cpu_thread()
{
    return OnThread(ThreadType::cpu);
}

After
after(std::chrono::milliseconds offset)
{
    return After(std::chrono::steady_clock::now() + offset);
}

void
Job::add_internal_completion_cb(ThreadType thread_type, std::function<void(Error)> cb)
{
    // no mutex needed because we should be still in the same run loop iteration
    m_internal_completion_cbs.emplace_back(std::make_pair(thread_type, std::move(cb)));
}

Error
Job::err()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_err;
}

void
Job::set_err(Error err)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_err = err;
}

void
Job::reset_work_cb()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_work_cb = std::function<Error()>{};
}

void
Job::reset_work_void_cb()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_work_void_cb = std::function<void()>{};
}

CompletionHandler&
when_all(std::vector<CompletionHandler> handlers)
{
    auto total_error = std::make_shared<Error>();
    auto total_done = std::make_shared<size_t>();
    auto n = handlers.size();
    
    auto total_job = std::make_shared<Job>([total_error]{
        VERIFY(*total_error);
        SUCCEED();
    });

    for (const auto& handler : handlers) {
        auto job = handler.job();
        assert(!job->m_is_when_all_called && !job->m_is_when_any_called && "multiple when_all() or when_any() calls on the same dispatch()");
        job->m_is_when_all_called = true;
        job->add_internal_completion_cb(tls_thread_type(), [total_error, total_done, n, total_job, job](Error err){
            // note: no synchronization (eg., mutex) needed - we know that we are on the same thread
            // as the original when_all() call

            // record only the first error that happens
            if (err && !(*total_error)) {
                *total_error = err;
            }
            *total_done = *total_done + 1;
            assert(*total_done <= n);
            if (*total_done == n) {
                if (*total_error) {
                    dispatch(total_job->work_cb())
                        .error(total_job->error_cb());
                } else {
                    dispatch(total_job->work_cb())
                        .then(total_job->completion_cb());
                }
            }
        });
    }
    return total_job->completion_handler();
}

CompletionHandler&
when_any(std::vector<CompletionHandler> handlers)
{
    auto total_error = std::make_shared<Error>();
    auto total_done = std::make_shared<bool>();

    auto total_job = std::make_shared<Job>([total_error]{
        VERIFY(*total_error);
        SUCCEED();
    });

    for (const auto& handler : handlers) {
        auto job = handler.job();
        assert(!job->m_is_when_all_called && !job->m_is_when_any_called && "multiple when_any() or when_all() calls on the same dispatch()");
        job->m_is_when_any_called = true;
        job->add_internal_completion_cb(tls_thread_type(), [total_error, total_job, total_done](Error err){
            // note: no synchronization (eg., mutex) needed - we know that we are on the same thread
            // as the original when_any() call

            if (*total_done) {
                // we already handled another dispatch() completion which was faster - ignore this one
                return;
            }

            *total_error = err;
            *total_done = true;

            if (err) {
                dispatch(total_job->work_cb())
                    .error(total_job->error_cb());
            } else {
                dispatch(total_job->work_cb())
                    .then(total_job->completion_cb());
            }
        });
    }
    return total_job->completion_handler();
}

} // namespace orion