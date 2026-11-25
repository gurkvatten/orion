#pragma once

//-----------------------------------------------------------------------
// Simple DTO members
//-----------------------------------------------------------------------

// ---- basic helpers ----
#define DTO_PP_EXPAND(x) x
#define DTO_PP_CAT(a,b) DTO_PP_CAT_I(a,b)
#define DTO_PP_CAT_I(a,b) a##b

// ---- count args: supports 1..3 (we use 2 or 3) ----
// Note: This assumes you always call DTO_MEMBER with 2 or 3 args (as in your examples).
#define DTO_PP_NARG(...) DTO_PP_EXPAND(DTO_PP_NARG_I(__VA_ARGS__,3,2,1,0))
#define DTO_PP_NARG_I(_1,_2,_3,N,...) N

// ---- implementation ----
#define DTO_MEMBER(...) \
    DTO_PP_EXPAND(DTO_PP_CAT(INTERNAL_DTO_MEMBER_, DTO_PP_NARG(__VA_ARGS__))(__VA_ARGS__))

#define INTERNAL_DTO_MEMBER_2(name, type) \
    type name{}

#define INTERNAL_DTO_MEMBER_3(name, type, default_value) \
    type name{default_value}

//-----------------------------------------------------------------------
// More complex DTO properties with metadata and possible instrumentation
//-----------------------------------------------------------------------

// useful when initializing using designated initializers: T t{.property = value} - usage:
//   StructDto object{
//     DTO_VALUE_INIT(name, std::string, "value"),
//   };

namespace orion {

#define ENABLE_DTO_METADATA 0

template<typename T>
class DtoValue {
public:
    DtoValue(const std::string& name, const std::string& type_name)
        : m_value()
#if ENABLE_DTO_METADATA
        , m_name(name), m_type_name(type_name)
#endif // ENABLE_DTO_METADATA
        {}

    DtoValue(const std::string& name, const std::string& type_name, T value)
        : m_value(value)
#if ENABLE_DTO_METADATA
        , m_name(name), m_type_name(type_name)
#endif // ENABLE_DTO_METADATA
        {}

    // copy constructor and copy assignment operator
    DtoValue(const DtoValue& other) = default;
    DtoValue& operator=(const DtoValue& other) = default;

    // assignment from T
    DtoValue&
    operator=(const T& val)
    {
        m_value = val;
        return *this;
    }

    // conversion operator if we want to use this as T directly
    operator const T&() const { return m_value; }

    bool operator==(const T& other) const { return m_value == other; }

    const T* operator->() const { return &m_value; }
    T* operator->() { return &m_value; }

    const T& operator*() const { return m_value; }
    T& operator*() { return m_value; }

    const T& value() const { return m_value; }
    T& value() { return m_value; }

private:
    T m_value;

#if ENABLE_DTO_METADATA
    // these members can be useful for runtime introspection and debugging
    std::string m_name;
    std::string m_type_name;
#endif // ENABLE_DTO_METADATA
};

} // namespace orion