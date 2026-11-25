#include <core/webkit_bridge/API/bridge.h>

#include <utility>

namespace orion::bridge {

HostAttachment::HostAttachment(std::function<void()> detach)
    : m_detach(std::move(detach))
{
}

HostAttachment::~HostAttachment()
{
    reset();
}

HostAttachment::HostAttachment(HostAttachment&& other) noexcept
    : m_detach(std::move(other.m_detach))
{
    other.m_detach = nullptr;
}

HostAttachment&
HostAttachment::operator=(HostAttachment&& other) noexcept
{
    if(this == &other) return *this;
    reset();
    m_detach = std::move(other.m_detach);
    other.m_detach = nullptr;
    return *this;
}

HostAttachment&
HostAttachment::operator=(std::nullptr_t)
{
    reset();
    return *this;
}

void
HostAttachment::reset()
{
    if(!m_detach) return;
    auto detach = std::move(m_detach);
    m_detach = nullptr;
    detach();
}

} // namespace orion::bridge
