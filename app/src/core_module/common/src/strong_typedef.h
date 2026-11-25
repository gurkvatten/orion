#pragma once

#include <utility>

// alternatively, this could be declared as:
//   using name = struct Opaque##name{}*;
#define DECLARE_STRONG_TYPEDEF(name, type) using name = StrongTypedef<type, struct OpaqueSimple##name>

namespace orion {

// Note: constexpr specifiers on some ctors and methods are needed to allow initialization
// of default values in settings.h file.
template<typename T, typename Tag>
class StrongTypedef {
public:
    constexpr StrongTypedef() = default;
    constexpr StrongTypedef(const StrongTypedef&) = default;
    constexpr StrongTypedef(StrongTypedef&&) = default;
    StrongTypedef& operator=(const StrongTypedef&) = default;
    StrongTypedef& operator=(StrongTypedef&&) = default;
    ~StrongTypedef() = default;

    explicit constexpr StrongTypedef(const T& v) : value(v) {}
    explicit constexpr StrongTypedef(T&& v) : value(std::move(v)) {}

    // Explicit conversion
    explicit operator const T&() const { return value; }
    explicit operator T&() { return value; }

    // Assignment from T
    StrongTypedef& operator=(const T& v)
    {
        value = v;
        return *this;
    }

    StrongTypedef& operator=(T&& v)
    {
        value = std::move(v);
        return *this;
    }

    // Comparisons (optional)
    bool operator==(const StrongTypedef& other) const { return value == other.value; }
    bool operator!=(const StrongTypedef& other) const { return value != other.value; }
    bool operator<(const StrongTypedef& other) const { return value < other.value; }
    bool operator<=(const StrongTypedef& other) const { return value <= other.value; }

public:
    T value;
};

} // namespace orion