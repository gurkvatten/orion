#pragma once

#include <functional>

namespace orion {
    using Fn = std::function<void()>;
    using ErrorFn = std::function<void(orion::Error)>;
}
