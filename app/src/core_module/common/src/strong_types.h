#pragma once

#include <src/strong_typedef.h>
#include <limits>

namespace orion {

// Unix timestamp (seconds) in utc timezone, 64-bit, it is signed because that maps
// to INTEGER type in SQLite (which is also signed and has the same range).
DECLARE_STRONG_TYPEDEF(UnixTimeUtc64, int64_t);
const UnixTimeUtc64 UnixTimeUtc64Max = UnixTimeUtc64(std::numeric_limits<int64_t>::max());

DECLARE_STRONG_TYPEDEF(FontSize, int64_t);
DECLARE_STRONG_TYPEDEF(BookmarkFolderId, int64_t);

} // namespace orion