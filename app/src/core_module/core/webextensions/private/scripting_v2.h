#pragma once

#include <core/core_api.h>

namespace WebExtensions {

// Native v2 registration harness. The generated ScriptingV2 defaults reject
// every operation until the real implementation is added.
class ScriptingV2Impl : public v2::ScriptingV2 {
};

} // namespace WebExtensions
