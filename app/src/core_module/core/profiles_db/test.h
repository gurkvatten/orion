#pragma once

#include <common/src/error.h>
#include <memory>
#include <core/db/db.h>
#include <core/profiles_db/profiles_db.h>

namespace orion {

Error test_db(std::shared_ptr<Db> db);
void test_profiles_db(std::shared_ptr<ProfilesDb> profiles_db);

} // namespace orion