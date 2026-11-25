#pragma once

#include <common/src/func_types.h>
#include <core/profiles_db/profiles_db.h>

namespace orion {

enum class CleanupDownloadsTrigger {
    not_set,
    manual,
    on_startup,
    on_quit,
    on_periodic_timer,
    on_successful_download,
};

void cleanup_downloads(orion::ProfileId profile_id, CleanupDownloadsTrigger trigger, orion::ErrorFn done_cb);

} // namespace orion