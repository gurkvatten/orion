#include <core/db/db_migration.h>
#include <unordered_set>
#include <common/src/string_util.h>

namespace orion {

Error
DbMigrator::try_create_migration_table()
{
    bool exists;
    VERIFY(m_db->table_exists("migrations", &exists));
    if (exists) {
        SUCCEED();
    }
    
    log_debug("%s: creating migrations table", m_db->path().c_str());

    VERIFY(m_db->exec(R"(
        CREATE TABLE migrations (
            id TEXT PRIMARY KEY,
            up TEXT,
            down TEXT
        ) WITHOUT ROWID;
    )"));
    SUCCEED();
}

Error
DbMigrator::check_duplicates()
{
    std::unordered_set<std::string> id_set;
    std::unordered_set<std::string> seq_num_set;
    std::unordered_set<std::string> guid_set;

    for (const auto& m : m_migrations) {
        // Check migration ID uniqueness
        if (!id_set.insert(m.id).second) {
            RAISE(ErrorCode::db_duplicate_migration, "%s: duplicate migration id '%s'", m_db->path().c_str(), m.id.c_str());
        }

        // Do not check sequence number uniqueness and UUID uniqueness anymore.
        // This is basically redundant and causes issues with multiple developers
        // adding migrations in parallel.
    }

    SUCCEED();
}

// Sets next_migration to nullptr if all migrations have already been applied
Error
DbMigrator::next_pending_migration(std::unique_ptr<DbMigration>* next_migration)
{
    std::unordered_set<std::string> done_set;

    DbStmt stmt;
    VERIFY(m_db->query("SELECT id FROM migrations;", &stmt));
    while (stmt.next()) {
        done_set.insert(stmt.text("id"));
    }
    VERIFY(stmt.error());

    for (const auto& m : m_migrations) {
        if (done_set.find(m.id) == done_set.end()) {
            // This is the first migration that has not been applied yet
            *next_migration = std::make_unique<DbMigration>(m);
            SUCCEED();
        }
    }

    // No need to perform any migration
    *next_migration = nullptr;

    SUCCEED();
}

Error
DbMigrator::migrate_to(const DbMigration& migration)
{
    VERIFY(m_db->exec(migration.up.value));
    VERIFY(m_db->exec("INSERT INTO migrations (id, up, down) VALUES (?, ?, ?);", {
        migration.id,
        migration.up.value,
        migration.down.value,
    }));
    SUCCEED();
}

Error
DbMigrator::migrate_down(const DbMigration& migration)
{
    VERIFY(m_db->exec(migration.down.value));
    VERIFY(m_db->exec("DELETE FROM migrations WHERE id = ?1;", {
        migration.id,
    }));
    SUCCEED();
}

void
DbMigrator::trim_whitespace()
{
    for (auto& m : m_migrations) {
        str::trim(&m.up.value);
        str::trim(&m.down.value);
    }
}

Error
DbMigrator::migrate_to_latest()
{
    VERIFY(try_create_migration_table());
    if (m_fix_migrations) {
        VERIFY(m_fix_migrations());
    }
    VERIFY(check_duplicates());
    trim_whitespace();
    
    // Apply each unapplied migration one by one, an error can occur otherwise
	// when for example a single table is changed in multiple migrations.

    while (true) {
        DbTx tx;
        VERIFY(m_db->begin(&tx));

        std::unique_ptr<DbMigration> next_migration;

        // Returns all done migrations followed by the first pending migration (if any)
        VERIFY(next_pending_migration(&next_migration));

        if (!next_migration) {
            break;
        }

        log_info("%s: migrating to: '%s'", m_db->path().c_str(), next_migration->id.c_str());

        VERIFY(migrate_to(*next_migration));
        VERIFY(tx.commit());
    }
    SUCCEED();
}

} // namespace orion