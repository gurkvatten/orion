#define ENABLE_OBSERVABLE_DEBUG_LOGS 0
#define ENABLE_OBSERVABLE_VERBOSE_LOGS 0
#define CORE_MODULE_DLL_EXPORTS

#include <src/observable.h>
#include <src/dispatch.h>
#include <stdarg.h>

namespace orion {

std::atomic<int64_t> global_next_observable_id = 1;  // initialize static member variable
std::atomic<size_t> global_next_observer_id = 1;     // initialize static member variable

// For debugging purposes: set this to a specific (leaking) non-zero observer ID and add
// breakpoint in Observable<T>::add_observer
ObserverId global_track_observer_id = ObserverId(0);

void
observable_notify_internal(ThreadType thread_type, std::function<void()> work)
{
    dispatch(on_thread(thread_type), work);
}

void
observable_log_debug(const char* format, ...)
{
#if ENABLE_OBSERVABLE_DEBUG_LOGS
	va_list args{};
	va_start(args, format);
    // orion::log_debug call would not work properly with va_list argument, call orion::log directly instead
	orion::log("D", format, args);
	va_end(args);
#endif
}

void
observable_log_verbose(const char* format, ...)
{
#if ENABLE_OBSERVABLE_VERBOSE_LOGS
	va_list args{};
	va_start(args, format);
    // orion::log_verbose call would not work properly with va_list argument, call orion::log directly instead
	orion::log("V", format, args);
	va_end(args);
#endif
}

} // namespace orion