#pragma once

#include <string>
#include <core/db/db_migration.h>
#include <vector>

namespace orion {

std::vector<DbMigration> profiles_db_migrations();

} // namespace orion