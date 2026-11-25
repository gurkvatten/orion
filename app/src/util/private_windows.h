#pragma once

#include <core/dto/profile_dto.h>
#include <common/src/error.h>
#include <core/core_api.h>

namespace orion::profile {

void cleanup_private_data(orion::ProfileId profile_id, orion::Cb<Error> cb);

} // namespace orion::profile