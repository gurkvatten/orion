#pragma once

#include <optional>
#include <vector>

template <typename T>
std::optional<std::vector<T>>
vectorify(const std::optional<T>& opt)
{
    if(!opt) {
        return std::nullopt;
    }
    return std::vector<T>{opt.value()};
}

template <typename T>
T
value_or_default(const std::optional<T>& opt)
{
    return opt.value_or(T{});
}

template <typename T>
constexpr T
coalesce(const T& value)
{
    return value;
}

template <typename T, typename... Rest>
constexpr T
coalesce(const T& first, Rest... rest)
{
    if (first) {
        return first;
    }
    return coalesce<T>(rest...);
}
