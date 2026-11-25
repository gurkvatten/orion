#include "variant_util.h"

#include <core/settings.h>

#include "global_core_api.h"


namespace orion::gtk
{

VariantWrapper::~VariantWrapper()
{
    g_clear_pointer(&m_variant, g_variant_unref);
}

VariantWrapper::VariantWrapper(GVariant* variant)
{
    m_variant = variant;
}



VariantWrapper::VariantWrapper(const VariantWrapper& other)
{
    m_variant = g_variant_ref(other.m_variant);
}

VariantWrapper::VariantWrapper(VariantWrapper&& other)
{
    m_variant = other.m_variant;
    other.m_variant = nullptr;
}

VariantWrapper& VariantWrapper::operator=(const VariantWrapper& other)
{
    m_variant = g_variant_ref(other.m_variant);
    return *this;
}

VariantWrapper& VariantWrapper::operator= (VariantWrapper&& other)
{
    m_variant = other.m_variant;
    other.m_variant = nullptr;
    return *this;
}


    
GVariant* VariantWrapper::get() const
{
    return this->m_variant;
}

}
