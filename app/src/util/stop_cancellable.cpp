#include "stop_cancellable.h"

namespace orion {

std::shared_ptr<StopCancellable>
StopCancellable::create()
{
    return std::shared_ptr<StopCancellable>(new StopCancellable(g_cancellable_new()));
}

std::shared_ptr<StopCancellable>
StopCancellable::wrap(GCancellable* existing)
{
    if (!existing) return nullptr;
    return std::shared_ptr<StopCancellable>(new StopCancellable(g_object_ref(existing)));
}

StopCancellable::~StopCancellable()
{
    if (m_cancellable) {
        if (m_handler_id) g_cancellable_disconnect(m_cancellable, m_handler_id);
        g_object_unref(m_cancellable);
    }
}

void
StopCancellable::on_cancelled(GCancellable*, gpointer user_data)
{
    static_cast<StopCancellable*>(user_data)->m_stop_source.request_stop();
}

StopCancellable::StopCancellable(GCancellable* cancellable)
    : m_cancellable(cancellable)
{
    m_handler_id = g_cancellable_connect(
        m_cancellable,
        G_CALLBACK(on_cancelled),
        this, // user_data
        nullptr // destroy func
    );
}

} // namespace orion