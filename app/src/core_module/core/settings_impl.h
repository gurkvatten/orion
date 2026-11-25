#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <type_traits>
#include <common/src/error.h>
#include <assert.h>
#include <common/src/log.h>
#include <charconv>
#include <cstdint>
#include <core/settings_types.h>

namespace orion::settings {

//-------------------------------------------------------------------------
// Dynamic registry per realm
//-------------------------------------------------------------------------

struct InternalSettingList {
    InternalSettingList()
    {
        //orion::log_debug("InternalSettingList created");
    }

    void
    register_setting(const ISetting* s)
    {
        // orion::log_debug("InternalSettingList register_setting: %s::%s (%s)",
        //     s->realm().c_str(), s->key_name().c_str(), s->type().c_str()
        // );
        items.push_back(s);
    }

    const std::vector<const ISetting*>&
    all() const
    {
        return items;
    }

private:
    std::vector<const ISetting*> items;
};

} // namespace orion::settings

//-------------------------------------------------------------------------
// Macros for defining realms and settings
//-------------------------------------------------------------------------

#define SETTING_REALM(name)                                             \
    namespace name {                                                    \
    constexpr const char* internal_realm_name = #name;                  \
    inline std::string realm_name() { return internal_realm_name; }     \
    inline orion::settings::InternalSettingList internal_registry;      \
    inline void internal_register_to_realm(const ISetting* s) {         \
         internal_registry.register_setting(s);                         \
    }                                                                   \
    inline const std::vector<const orion::settings::ISetting*>& all() { \
        return internal_registry.all();                                 \
    }                                                                   \
    } // namespace name

#ifdef ORION_SETTINGS_CPP
#define ORION_INTERNAL_REGISTRAR_SPECIFIER static
#else
#define ORION_INTERNAL_REGISTRAR_SPECIFIER extern
#endif // ORION_SETTINGS_CPP

#define SETTING_VALUE(realm, propname, valuetype, ...)                       \
    namespace realm {                                                        \
    struct internal_key_##propname {                                         \
        using value_type = valuetype;                                        \
        static constexpr const char* key_name =                              \
            #propname;                                                       \
        static constexpr const char* type() {                                \
            return orion::settings::internal_setting_type_name<valuetype>(); \
        }                                                                    \
        static constexpr value_type default_value() {                        \
            return default_impl();                                           \
        }                                                                    \
    private:                                                                 \
        static constexpr value_type default_impl() {                         \
            if constexpr (sizeof(#__VA_ARGS__) > 1) {                        \
                return value_type(__VA_ARGS__);                              \
            } else {                                                         \
                return value_type{};                                         \
            }                                                                \
        }                                                                    \
    };                                                                       \
    static orion::settings::Setting<internal_key_##propname>                 \
        propname{internal_realm_name};                                       \
    struct InternalRegistrar_##propname {                                    \
        InternalRegistrar_##propname() {                                     \
            orion::realm::internal_register_to_realm(&propname);             \
        }                                                                    \
    };                                                                       \
    ORION_INTERNAL_REGISTRAR_SPECIFIER                                       \
        InternalRegistrar_##propname                                         \
            internal_registrar_##propname;                                   \
    } // namespace realm
