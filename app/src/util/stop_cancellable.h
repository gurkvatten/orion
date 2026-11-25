#pragma once

#include <gio/gio.h>
#include <stop_token>
#include <memory>

namespace orion {

class StopCancellable {
public:
    // Factory functions - preferred way to create
    static std::shared_ptr<StopCancellable> create();

    static std::shared_ptr<StopCancellable> wrap(GCancellable* existing);

    ~StopCancellable();

    StopCancellable(const StopCancellable&) = delete;
    StopCancellable& operator=(const StopCancellable&) = delete;

    std::stop_token get_stop_token() const noexcept { return this->m_stop_source.get_token(); }

    GCancellable* get() const noexcept { return m_cancellable; }

    void cancel() noexcept { g_cancellable_cancel(m_cancellable); }

    bool is_cancelled() const noexcept { return m_stop_source.stop_requested(); }

private:
    GCancellable* m_cancellable = nullptr;
    std::stop_source m_stop_source{};
    gulong m_handler_id = 0;

    static void on_cancelled(GCancellable*, gpointer user_data);

    // Private constructor
    explicit StopCancellable(GCancellable* cancellable);
};

} // namespace orion