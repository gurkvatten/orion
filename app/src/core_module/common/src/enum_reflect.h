#pragma once
#include <array>
#include <optional>
#include <string_view>
#include <string>
#include <utility>
//#include <iostream>
//#include <typeinfo>
//#include <cstddef>
//#include <functional>
//#include <limits>
//#include <type_traits>

/*
this is a primitive implementation to turn enum to string and vice versa. The more elaborate stuff is here:
https://blog.rink.nu/2023/02/12/behind-the-magic-of-magic_enum/ and hence we can use that in case we have more use of this feature in the code
so far this is enough. The approach behind the link doesn't require the enum table declaration as it can be descerned from the enum itself.
*/
namespace orion::enum_reflect {

// opur enums should only ever contain ^[a-zA-Z]+$
constexpr auto
is_pretty(char ch) noexcept
{
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
}
 
constexpr auto
pretty_name(std::string_view sv) noexcept
{
    for(std::size_t n = sv.size() - 1; n > 0; --n) {
        if (!is_pretty(sv[n])) {
            sv.remove_prefix(n + 1);
            break;
        }
    }
    return sv;
}
 
template<typename E, E V>
constexpr auto _name() noexcept {
#if defined(__GNUC__) || defined(__clang__)
    return pretty_name({ __PRETTY_FUNCTION__, sizeof(__PRETTY_FUNCTION__) - 2 });
#elif defined(_MSC_VER)
    return pretty_name({ __FUNCSIG__, sizeof(__FUNCSIG__) - 17 });
#endif
}

template <typename E>
struct enum_entry {
    const std::string_view name;
    const E value;
};

#define ENUM_ENTRY(Val) Val 
#define REGISTER_ENUM(EnumType, ...) enum_map<EnumType, __VA_ARGS__>().handler();

// build a table from a pack of enum values (non-type template parameter pack)
template <typename E, E... V>
struct enum_map {
    using EnumEntry = enum_entry<E>;
    constexpr static std::array<EnumEntry, sizeof...(V)> table = {{ EnumEntry{_name<E, V>(), V}... }};

    constexpr const auto&
    handler() const noexcept
    {
        return *this;
    }

    // we expect the enum to provide an array of all names/values
    constexpr const std::optional<std::string_view>
    to_string(E e) const noexcept
    {
        for (enum_entry<E> const& e2n : table) {
            if (e2n.value == e)
                return e2n.name;
        }
        return std::nullopt;
    }

    constexpr std::optional<E>
    from_string(std::string s) const noexcept
    {
        for (enum_entry<E> const& e2n : table) {
            if (e2n.name == s)
                return e2n.value;
        }
        return std::nullopt;
    }

    constexpr const auto&
    values() const noexcept
    {
        return table;
    }

};

} // namespace orion::enum_reflect