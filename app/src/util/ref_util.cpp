#define ENABLE_VERBOSE_LOGS 0

#include <gtk/gtk.h>
#include <memory>

#include <common/src/log.h>

#include "ref_util.h"


namespace orion::gtk {

WeakRefWrapper::WeakRefWrapper()
{
    
}

WeakRefWrapper::WeakRefWrapper(gpointer object)
{
    g_weak_ref_set(&ref, object);
}

WeakRefWrapper::WeakRefWrapper(const WeakRefWrapper& other)
{
    auto ptr = other.lock();
    g_weak_ref_set(&ref, ptr.get());
}

WeakRefWrapper::WeakRefWrapper(WeakRefWrapper&& other)
{
    auto ptr = other.lock();
    g_weak_ref_set(&ref, ptr.get());
    g_weak_ref_set(&other.ref, NULL);
}

WeakRefWrapper&
WeakRefWrapper::operator=(const WeakRefWrapper& other)
{
    auto ptr = other.lock();
    g_weak_ref_set(&ref, ptr.get());
    return *this;
}

WeakRefWrapper&
WeakRefWrapper::operator=(WeakRefWrapper&& other)
{
    auto ptr = other.lock();
    g_weak_ref_set(&ref, ptr.get());
    g_weak_ref_set(&other.ref, NULL);
    return *this;
}

WeakRefWrapper::~WeakRefWrapper()
{
    g_weak_ref_set(&ref, NULL);
}

gpointer
WeakRefWrapper::get() const
{
    return g_weak_ref_get(&ref);
}

GObjectPtr
WeakRefWrapper::lock() const
{
    return GObjectPtr(static_cast<GObject*>(g_weak_ref_get(&ref)));
}

void
WeakRefWrapper::reset(gpointer object)
{
    g_weak_ref_set(&ref, G_OBJECT(object));
}


StrongRefWrapper::StrongRefWrapper(gpointer object)
{
    orion::log_verbose("StrongRefWrapper(gpointer): object=%p", object);
    if(object)
    {
        ptr = g_object_ref(object);
    }
    else
    {
        ptr = nullptr;
    }
}

StrongRefWrapper::StrongRefWrapper(const StrongRefWrapper& other)
{
    orion::log_verbose("StrongRefWrapper(const StrongRefWrapper&): other.ptr=%p", other.ptr);
    if(other.ptr)
    {
        ptr = g_object_ref(other.ptr);
    }
    else
    {
        ptr = nullptr;
    }
}

StrongRefWrapper::StrongRefWrapper(StrongRefWrapper&& other)
{
    orion::log_verbose("StrongRefWrapper(StrongRefWrapper&&): other.ptr=%p", other.ptr);
    ptr = other.ptr;
    other.ptr = nullptr;
}

StrongRefWrapper&
StrongRefWrapper::operator=(const StrongRefWrapper& other)
{
    orion::log_verbose("StrongRefWrapper::operator=(const StrongRefWrapper&): other.ptr=%p", other.ptr);
    if(this == &other)
    {
        return *this;
    }

    g_clear_object(&ptr);
    if(other.ptr)
    {
        ptr = g_object_ref(other.ptr);
    }
    return *this;
}

StrongRefWrapper&
StrongRefWrapper::operator=(StrongRefWrapper&& other)
{
    orion::log_verbose("StrongRefWrapper::operator=(StrongRefWrapper&&): other.ptr=%p", other.ptr);
    g_clear_object(&ptr);
    ptr = other.ptr;
    other.ptr = nullptr;
    return *this;
}

StrongRefWrapper::~StrongRefWrapper()
{
    orion::log_verbose("StrongRefWrapper::~StrongRefWrapper(): ptr=%p", ptr);
    if(ptr)
    {
        g_object_unref(ptr);
    }
}

gpointer
StrongRefWrapper::get() const
{
    return ptr;
}

void
StrongRefWrapper::reset(gpointer object)
{
    g_clear_object(&ptr);
    if(object)
    {
        ptr = g_object_ref(object);
    }
}

StrongRefWrapper
StrongRefWrapper::create_raw(gpointer object)
{
    StrongRefWrapper wrapper(nullptr);
    wrapper.ptr = object;
    return wrapper;
}

} // namespace orion::gtk