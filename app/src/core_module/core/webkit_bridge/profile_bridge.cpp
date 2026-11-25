#include <core/webkit_bridge/API/bridge.h>

#include <core/core_api.h>
#include <common/src/coroutine.h>

#include <cassert>
#include <initializer_list>
#include <utility>

namespace orion::bridge {

ProfileBridge::ProfileBridge(CoreApi& core_api, ProfileId profile_id)
    : m_core_api(core_api)
    , m_profile_id(profile_id)
{
}

void
ProfileBridge::initialize_worlds()
{
    m_main_world = std::shared_ptr<ProfileWorld>(new ProfileWorld(
        *this, m_core_api, m_profile_id, PrivateMode::no));
    m_private_world = std::shared_ptr<ProfileWorld>(new ProfileWorld(
        *this, m_core_api, m_profile_id, PrivateMode::yes));
    // Worlds must be shared-owned before their child bridges take weak references.
    for(const auto& world : {m_main_world, m_private_world}) {
        world->m_web_view_factory_bridge = std::shared_ptr<WebViewFactoryBridge>(
            new WebViewFactoryBridge(*world));
    }
}

ProfileBridge::~ProfileBridge()
{
    m_host = nullptr;
    if(m_main_world) m_main_world->detach();
    if(m_private_world) m_private_world->detach();
}

ProfileId
ProfileBridge::profile_id() const
{
    return m_profile_id;
}

CoreApi&
ProfileBridge::core_api() const
{
    return m_core_api;
}

HostAttachment
ProfileBridge::attach(ProfileHost& host)
{
    assert(!m_host || m_host == &host);
    m_host = &host;
    for(const auto& world : {m_main_world, m_private_world}) {
        world->model_repository().hydrate();
    }
    std::weak_ptr<ProfileBridge> weak_bridge = weak_from_this();
    return HostAttachment([weak_bridge, host = &host] {
        if(auto bridge = weak_bridge.lock()) bridge->detach(*host);
    });
}

void
ProfileBridge::detach(ProfileHost& host)
{
    if(m_host != &host) return;
    m_host = nullptr;
    m_core_api.remove_profile_bridge(*this);
}

std::weak_ptr<ProfileWorld>
ProfileBridge::main_world() const
{
    return m_main_world;
}

std::weak_ptr<ProfileWorld>
ProfileBridge::private_world() const
{
    return m_private_world;
}

std::weak_ptr<ProfileWorld>
ProfileBridge::world(PrivateMode private_mode) const
{
    return private_mode == PrivateMode::yes
        ? m_private_world
        : m_main_world;
}

void
ProfileBridge::schedule_commit(ProfileWorld& world) {
    dispatch([weak_bridge=weak_from_this(),weak_world=world.weak_from_this()](){
        if(auto bridge = weak_bridge.lock()) {
            if(auto world = weak_world.lock()) {
                bridge->commit(*world);
            }
        }
    });
}

void
ProfileBridge::commit(ProfileWorld& world) {
    if(!m_host) return;
    world.commit(*m_host);
}

} // namespace orion::bridge
