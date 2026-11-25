#pragma once

#include <chrono>
#include "src/strong_types.h"

namespace orion::tm {

UnixTimeUtc64 unix_time_utc64_now();
int64_t today_local();
bool is_same_day(UnixTimeUtc64 t1, UnixTimeUtc64 t2);
bool is_today_utc(UnixTimeUtc64 t);
int64_t to_day(UnixTimeUtc64 t);
// Subtracts t1 - t2
int64_t duration_seconds(UnixTimeUtc64 t1, UnixTimeUtc64 t2);

} // namespace orion::tm
