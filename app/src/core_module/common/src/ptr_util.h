#pragma once

#include <stdint.h>
#include <stddef.h>

namespace orion {

// compile-time check
static_assert(
    sizeof(int64_t) <= sizeof(void*),
    "int64_t cannot safely fit into void* on this platform"
);

// safely converts int64_t to void*
static inline void*
int64_to_void_ptr(int64_t i)
{
    return reinterpret_cast<void*>(static_cast<intptr_t>(i));
}

// safely converts int64_t to void*
static inline void*
gulong_to_void_ptr(gulong gu)
{
    return int64_to_void_ptr(static_cast<int64_t>(gu));
}

// safely converts int64_t to const void*
static inline const void*
int64_to_const_void_ptr(int64_t i)
{
    return reinterpret_cast<const void*>(static_cast<intptr_t>(i));
}

// safely converts void* to int64_t
static inline int64_t
void_ptr_to_int64(void* p)
{
    return static_cast<int64_t>(reinterpret_cast<intptr_t>(p));
}

// safely converts void* to gulong (unsigned long)
static inline int64_t
void_ptr_to_gulong(void* p)
{
    return static_cast<gulong>(void_ptr_to_int64(p));
}

// safely converts const void* to int64_t
static inline int64_t
const_void_ptr_to_int64(const void* p)
{
    return static_cast<int64_t>(reinterpret_cast<intptr_t>(p));
}

// safely converts bool to void*
static inline void*
bool_to_void_ptr(bool b)
{
    return reinterpret_cast<void*>(static_cast<intptr_t>(b));
}

// safely converts void* to bool
static inline bool
void_ptr_to_bool(void* p)
{
    return static_cast<bool>(reinterpret_cast<intptr_t>(p));
}

// safely converts bool to const void*
static inline const void*
bool_to_const_void_ptr(bool b)
{
    return reinterpret_cast<const void*>(static_cast<intptr_t>(b));
}

// safely converts const void* to bool
static inline bool
const_void_ptr_to_bool(const void* p)
{
    return static_cast<bool>(reinterpret_cast<intptr_t>(p));
}

} // namespace orion