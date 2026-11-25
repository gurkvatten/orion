#pragma once

#include <string>
#include <assert.h>
#include <common/src/strong_types.h>

namespace orion::settings {

//-------------------------------------------------------------------------
// Type-to-string trait
//-------------------------------------------------------------------------

template<typename T, typename... Types>
struct is_any_of : std::disjunction<std::is_same<T, Types>...> {};

template<typename T, typename... Types>
inline constexpr bool is_any_of_v = is_any_of<T, Types...>::value;

template <typename... Types>
struct type_list {};

// New helper that takes type_list<Types...>
template<typename T, typename TypeList>
struct is_any_of_type_list;

template<typename T, typename... Types>
struct is_any_of_type_list<T, type_list<Types...>> : is_any_of<T, Types...> {};

template<typename T, typename TypeList>
inline constexpr bool is_any_of_type_list_v = is_any_of_type_list<T, TypeList>::value;

using IntegerStrongTypes = type_list<UnixTimeUtc64, FontSize, BookmarkFolderId>;

template <typename T>
constexpr const char*
internal_setting_type_name()
{
    if constexpr (std::is_same_v<T, int64_t>) {
        return "integer";
    } else if constexpr (std::is_same_v<T, std::string>) {
        return "text";
    } else if constexpr (std::is_same_v<T, bool>) {
        return "boolean";
    } else if constexpr (std::is_same_v<T, double>) {
        return "real";
    } else if constexpr (std::is_enum_v<T>) {
        return "integer"; // Assuming enum can be represented as int64_t
    } else if constexpr (is_any_of_type_list_v<T, IntegerStrongTypes>) {
        return "integer";
    } else {
        static_assert(false, "Unsupported type");
        return ""; // This will never be reached, but avoids compiler warning
    }
}

//-------------------------------------------------------------------------
// Marshal and unmarshal functions
//-------------------------------------------------------------------------

Error parse_int64(const std::string& str, int64_t* value);
Error parse_double(const std::string& str, double* value);

template <typename T>
Error
unmarshal_value(const std::string& str, T* value)
{
    assert(value != nullptr);
    if constexpr (std::is_same_v<T, std::string>) {
        *value = str;
    } else if constexpr (std::is_same_v<T, int64_t>) {
        VERIFY(parse_int64(str, value));
    } else if constexpr (std::is_same_v<T, bool>) {
        if (str == "true" || str == "1") {
            *value = true;
        } else if (str == "false" || str == "0") {
            *value = false;
        } else {
            RAISE(ErrorCode::argument, "failed to parse bool from string: %s", str.c_str());
        }
    } else if constexpr (std::is_same_v<T, double>) {
        VERIFY(parse_double(str, value));
    } else if constexpr (std::is_enum_v<T>) {
        int64_t int_value = 0;
        VERIFY(parse_int64(str, &int_value));
        *value = static_cast<T>(int_value); // Assuming enum can be represented as int64_t
    } else if constexpr (is_any_of_type_list_v<T, IntegerStrongTypes>) {
        int64_t int_value = 0;
        VERIFY(parse_int64(str, &int_value));
        *value = static_cast<T>(int_value);
    } else {
        // For other types, we assume they can be converted from string
        *value = str;
    }
    SUCCEED();
}

template <typename T>
std::string
marshal_value(const T& value)
{
    if constexpr (std::is_same_v<T, std::string>) {
        return value;
    } else if constexpr (std::is_same_v<T, int64_t>) {
        return std::to_string(value);
    } else if constexpr (std::is_same_v<T, bool>) {
        return value ? "1" : "0";
    } else if constexpr (std::is_same_v<T, double>) {
        return std::to_string(value);
    } else if constexpr (std::is_enum_v<T>) {
        return std::to_string(static_cast<int64_t>(value)); // Assuming enum can be represented as int64_t
    } else if constexpr (is_any_of_type_list_v<T, IntegerStrongTypes>) {
        return std::to_string(value.value);
    } else {
        static_assert(false, "Unsupported type");
        return ""; // This will never be reached, but avoids compiler warning
    }
}

//-------------------------------------------------------------------------
// Abstract base for settings
//-------------------------------------------------------------------------

struct ISetting {
    virtual std::string key() const = 0;
    virtual std::string key_name() const = 0;
    virtual std::string type() const = 0;
    virtual std::string realm() const = 0;
    virtual std::string default_value_as_string() const = 0;
    virtual ~ISetting() = default;
};

//-------------------------------------------------------------------------
// Typed setting wrapper
//-------------------------------------------------------------------------

template <typename Tag>
class Setting : public ISetting {
public:
    using value_type = typename Tag::value_type;

    Setting(const char* realm_name)
        : m_realm_name(realm_name), m_value(Tag::default_value())
    {
    }

    Setting() : m_realm_name(), m_value()
    {
    }

    Setting(const Setting& other)
        : m_realm_name(other.m_realm_name), m_value(other.m_value)
    {
    }

    Setting&
    operator=(const Setting& other)
    {
        if (this != &other) {
            m_realm_name = other.m_realm_name;
            m_value = other.m_value;
        }
        return *this;
    }

    // Full key
    std::string key() const override { return std::string(m_realm_name) + "::" + Tag::key_name; }
    // Key name without realm
    std::string key_name() const override { return Tag::key_name; }
    std::string type() const override { return Tag::type(); }
    std::string realm() const override { return m_realm_name; }
    value_type default_value() const { return Tag::default_value(); }

    std::string default_value_as_string() const override {
        return marshal_value(Tag::default_value());
    }

    std::shared_ptr<ISetting> to_isetting() const {
        return std::make_shared<Setting<Tag>>(*this);
    }

private:
    std::string m_realm_name;
    value_type m_value;
};

// Convenience macro for website settings
#define WEBSITE_SETTING(tag, uri_authority) \
    orion::settings::Setting<orion::settings::website::specific::internal_key_##tag>( \
        std::format("settings::website::specific::{}", (uri_authority)).c_str() \
    )

} // namespace orion::settings
