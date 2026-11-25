#pragma once

#include <core/core_api.h>

namespace orion {

// At the moment, done_cb callback is called only when the whole test succeeds,
// or when the test timeout is reached. The timeout is typically reached when some
// part of the test fails and in that case orion::ErrorCode::timeout is returned.
// Also, done_cb callback can be called multiple times if the test timeout is reached
// and then all internal parts of the test complete without errors.
// Timeout duration should be set to a reasonable value (for example 10 seconds) to avoid
// both false positives and waiting too long for a failed test.
void test_core_api(CoreApi* core_api, std::chrono::steady_clock::duration timeout, Cb<Error> done_cb);

} // namespace orion