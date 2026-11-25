#include <core/profiles_db/profiles_db.h>
#include <core/profiles_db/migrations.h>
#include <common/src/strong_types.h>
#include <filesystem>
#include <common/src/file_system.h>
#include <common/src/run_loop.h>
#include <format>
#include <set>
#include <unordered_set>

namespace orion {

ProfilesDb::ProfilesDb(const char* app_id, PasswordStorage* password_storage)
    : m_db(Db::create())
    , m_extension_session_storage_db(Db::create())
    , m_password_storage(password_storage)
{
    m_app_id = app_id;
}

std::shared_ptr<ProfilesDb>
ProfilesDb::create(const char* app_id, PasswordStorage* password_storage)
{
    return std::shared_ptr<ProfilesDb>(new ProfilesDb(app_id, password_storage)); // the new operator is used to call the private constructor
}

void
ProfilesDb::set_password_storage(PasswordStorage* password_storage)
{
    m_password_storage = password_storage;
}

Error
ProfilesDb::open(const std::string& path)
{
    VERIFY(ensure_parent_directories(path));
    VERIFY(m_db->open(path));
    VERIFY(m_extension_session_storage_db->open(":memory:"));
    VERIFY(m_extension_session_storage_db->exec(R"(
        CREATE TABLE extension_storage_areas (
            extension_id TEXT NOT NULL,
            storage_id TEXT NOT NULL,
            key TEXT NOT NULL,
            value TEXT NOT NULL DEFAULT ''
        );
        CREATE UNIQUE INDEX idx_extension_storage_areas_extension_id_storage_id_key
            ON extension_storage_areas(extension_id, storage_id, key);
    )"));
    SUCCEED();
}

Error
ProfilesDb::migrate()
{
    DbMigrator migrator(m_db, profiles_db_migrations(), [this]{
        return fix_old_migrations(m_db);
    });
    VERIFY(fix_old_migrations(m_db));
    VERIFY(migrator.migrate_to_latest());

    int64_t max_window_id = 0;
    int64_t max_tab_id = 0;
    VERIFY(m_db->query_integer(
        "SELECT COALESCE(MAX(id), 0) FROM windows;", {}, &max_window_id));
    VERIFY(m_db->query_integer(
        "SELECT COALESCE(MAX(id), 0) FROM tabs;", {}, &max_tab_id));
    m_max_window_id.store(max_window_id, std::memory_order_relaxed);
    m_max_tab_id.store(max_tab_id, std::memory_order_relaxed);

    SUCCEED();
}

WindowId
ProfilesDb::allocate_window_id()
{
    return WindowId(m_max_window_id.fetch_add(1, std::memory_order_relaxed) + 1);
}

TabId
ProfilesDb::allocate_tab_id()
{
    return TabId(m_max_tab_id.fetch_add(1, std::memory_order_relaxed) + 1);
}

void
ProfilesDb::track_window_id(WindowId window_id)
{
    int64_t current = m_max_window_id.load(std::memory_order_relaxed);
    while (current < window_id.value
        && !m_max_window_id.compare_exchange_weak(
            current, window_id.value, std::memory_order_relaxed)) {}
}

void
ProfilesDb::track_tab_id(TabId tab_id)
{
    int64_t current = m_max_tab_id.load(std::memory_order_relaxed);
    while (current < tab_id.value
        && !m_max_tab_id.compare_exchange_weak(
            current, tab_id.value, std::memory_order_relaxed)) {}
}

Error
ProfilesDb::fix_old_migrations(std::shared_ptr<Db> db)
{
    bool exists;
    VERIFY(db->table_exists("migrations", &exists));
    
    if (!exists) {
        SUCCEED();
    }
    
    // fix duplicate migration UUID
    VERIFY(db->exec(R"(
        UPDATE migrations
            SET id = '0005.6af1a126-a3b2-44bb-8f2c-e2122efabb54'
            WHERE id = '0005.cc844aa7-3e18-4447-be31-791587873bc6';
    )"));

    // Favicon migration was renumbered from 0066 to 0070 when develop was
    // merged (migrations 0066-0069 were inserted before it).
    VERIFY(db->exec(R"(
        UPDATE migrations
            SET id = '0070.d7f8a9b0-1c2d-3e4f-5a6b-7c8d9e0f1a2b'
            WHERE id = '0066.d7f8a9b0-1c2d-3e4f-5a6b-7c8d9e0f1a2b';
    )"));

    SUCCEED();
}

//-------------------------------------------------------------------------
// Database
//-------------------------------------------------------------------------

Error
ProfilesDb::begin(DbTx* tx, int* result_code)
{
    return m_db->begin(tx, result_code);
}

bool
ProfilesDb::in_transaction() const
{
    return m_db->in_transaction();
}

} // namespace orion
