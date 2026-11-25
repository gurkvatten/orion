#include <src/internal/thread_holder.h>
#include <stdexcept>

#ifndef _WIN32
#include <sys/un.h>
#include <signal.h>
#endif

namespace orion::sync {

constexpr int64_t thread_cancel_timeout_secs = 30;


void
ThreadHolder::cancel()
{
    std::unique_lock<std::mutex> lk(m_m);
    if (!m_finished && !m_cancelled.exchange(true)) {//oherwise we've been there already
        //TODO do we need to guard agains "spurious wakups" by an extra loop around this wait?
        if (m_cv.wait_for(lk, std::chrono::seconds(thread_cancel_timeout_secs)) == std::cv_status::timeout
            && m_thread.joinable()) {
#ifdef _WIN32
            // TODO: is pthread_kill required?
            // Is it a fallback in case the socket timeout fails? Or is it for some weird file system timeout scenario? 
            // If needed CancelSynchronousIo on Windows might work here.
#else
            //timeout, send the signal
            pthread_kill(m_thread.native_handle(), SIGUSR1);
#endif
            //abandon thread
            m_thread.detach();
        }
    }
};

#ifndef _WIN32
//stuff to be able to terminate blocked thread POSIX solution
static void sigHandler(int) {} //ewmpty, only to have somethi ng to register
//The sigaction() function requires a struct sigaction as input, which we need to set up and supply with the handler:
static struct sigaction sigActionData;

void
ThreadHolder::set_sigaction()
{
    sigemptyset(&sigActionData.sa_mask);
    sigActionData.sa_handler = &sigHandler;
    sigActionData.sa_flags = SA_INTERRUPT; //abort system call
    sigaction(SIGUSR1, &sigActionData, nullptr); //install the handler for ,hopefully, not that much used signal
};

void
ThreadHolder::clear_sigaction()
{
    sigemptyset(&sigActionData.sa_mask);
    sigActionData.sa_handler = SIG_DFL;
    sigActionData.sa_flags = SA_INTERRUPT;
    sigaction(SIGUSR1, &sigActionData, nullptr);
};
#else
void ThreadHolder::set_sigaction() {}
void ThreadHolder::clear_sigaction() {}
#endif

void
ThreadHolder::signal()
{
    std::lock_guard<std::mutex> lk(m_m);
    m_finished = true;
    m_cv.notify_all();
}

static void
runner_cleanup(ThreadRunner* runner, Error reason)
{
    Error lastError = runner->lastError();
    if (reason || lastError) {
        if (!reason) {
            reason = lastError;
        }
        reason.set_message("Initiating roll back due to: '" + reason.message() + "'");
        runner->rollback(reason);
    }
    runner->cleanup();
};

void
ThreadRunnerHolder::run()
{
    if (!m_thread.joinable()) {//otherwise we've been there
        m_thread = std::thread([this]() {
            ThreadHolder::set_sigaction();
            //catch any exception here
            try {
                if (m_runner->start()) {
                    while (!m_cancelled && m_runner->run());
                    runner_cleanup(m_runner.get(), m_cancelled ? MAKE_ERROR(ErrorCode::cancelled, "Sync process got cancelled by user.") : Error::ok());
                }
            } catch (const std::exception& ex) {
                runner_cleanup(m_runner.get(), MAKE_ERROR(ErrorCode::exception, "Sync threw an exception: %s", ex.what()));
            } catch (...) {
                runner_cleanup(m_runner.get(), MAKE_ERROR(ErrorCode::exception, "Sync threw an unklnown exception"));
            }
            //ThreadHolder::clear_sigaction();
            this->signal();
        });
    }
};

//TODO ThreadHolder and ThreadRunnerHolder could probably go to common

}
