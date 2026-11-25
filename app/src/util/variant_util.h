#pragma once

#include <glib.h>

#include <variant>
#include <string>

#include "variant_traits.h"


namespace orion::gtk
{

class VariantWrapper
{
public:
    VariantWrapper() = default;
    ~VariantWrapper();
    
    template<typename T> VariantWrapper(T val)
    {
        m_variant = g_variant_ref_sink(VariantTraits<T>::create(val));
    }

    template<typename... Args> static VariantWrapper Tuple(Args&&... args)
    {
        const size_t num_args = sizeof...(Args);

        std::array<VariantWrapper, num_args> wrappers = { VariantWrapper(std::forward<Args>(args))... };

        std::array<GVariant*, num_args> variants;
        for(size_t i = 0; i<num_args; i++)
        {
            variants[i] = wrappers[i].get();
        }

        return VariantWrapper(g_variant_ref_sink(g_variant_new_tuple(num_args?variants.data():nullptr, num_args)));
    }

    VariantWrapper(GVariant*);
    VariantWrapper(const VariantWrapper&);
    VariantWrapper(VariantWrapper&&);
    
    VariantWrapper&operator= (const VariantWrapper&);
    VariantWrapper&operator= (VariantWrapper&&);


    GVariant* get() const;
private:
    GVariant* m_variant = nullptr;
};

template<typename T>
T get_from_variant(GVariant* var)
{
    return VariantTraits<T>::get(var);
}

}
