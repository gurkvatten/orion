#pragma once

#include <cstdint>

namespace orion {

enum class ThreadType : int64_t {
    main    = 0, // UI/main thread
    disk    = 1, // disk I/O thread
    network = 2, // network I/O thread
    cpu     = 3, // CPU thread (formerly "render" thread)
};

} // namespace orion