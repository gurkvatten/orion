#pragma once

#include <string>
#include <common/src/error.h>
#include <core/db/db.h>
#include <common/src/strong_typedef.h>
#include <functional>

namespace orion {

DECLARE_STRONG_TYPEDEF(DbUp, std::string);
DECLARE_STRONG_TYPEDEF(DbDown, std::string);

struct DbMigration {
    std::string id;
    DbUp up;
    DbDown down;
};

class DbMigrator {
public:
    DbMigrator(std::shared_ptr<Db> db, const std::vector<DbMigration>& migrations, std::function<Error()> fix_migrations = nullptr)
        : m_db(db), m_migrations(migrations), m_fix_migrations(fix_migrations)
    {
    }

    Error migrate_to_latest();

private:
    Error try_create_migration_table();
    Error check_duplicates();
    Error next_pending_migration(std::unique_ptr<DbMigration>* next_migration);
    Error migrate_to(const DbMigration& migration);
    Error migrate_down(const DbMigration& migration);
    void trim_whitespace();

    std::shared_ptr<Db> m_db;
    std::vector<DbMigration> m_migrations;
    std::function<Error()> m_fix_migrations; // optional function to fix migrations
};

} // namespace orion