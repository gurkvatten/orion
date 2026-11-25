#include <core/profiles_db/migrations.h>

namespace orion {

// Migration name format:
//     "SequenceNumber.PseudoUUID" where SequenceNumber, has fixed length of 4, is unique,
//     PseudoUUID is unique, and "SequenceNumber.PseudoUUID" is unique too across all migrations.
//     Usage of PseudoUUID (that contains non-hexadecimal characters) instead of real UUID in
//     some migrations was accidental and can be fixed in the future if needed.

std::vector<DbMigration>
profiles_db_migrations()
{
    return {
        DbMigration{
            "0001.390f3c01-d6ce-4d45-860c-6bb3442e465a",
            DbUp(R"(
                CREATE TABLE test (id INTEGER PRIMARY KEY);
            )"),
            DbDown(R"(
                DROP TABLE test;
            )"),
        },
        DbMigration{
            "0002.55ed1301-cae4-408d-b303-bba50354bf4b",
            DbUp(R"(
                DROP TABLE test;
            )"),
            DbDown(R"(
                CREATE TABLE test (id INTEGER PRIMARY KEY);
            )"),
        },
        DbMigration{
            "0003.81e5fbca-c904-4e9a-8d31-bfc5a55c5d26",
            DbUp(R"(
                CREATE TABLE bookmark_folders (
                    id INTEGER PRIMARY KEY,
                    name TEXT NOT NULL,
                    -- Bookmarks Bar and Favorites are internal (cannot be modified, deleted, and they should
                    -- have localized names in the UI)
                    is_internal INTEGER NOT NULL,
                    parent_folder_id INTEGER,
                    seq_num INTEGER NOT NULL,
                    FOREIGN KEY (parent_folder_id) REFERENCES bookmark_folders(id) ON DELETE CASCADE
                );
                CREATE TABLE bookmarks (
                    id INTEGER PRIMARY KEY,
                    name TEXT NOT NULL,
                    address TEXT NOT NULL,
                    invocation_keyword TEXT NOT NULL, -- or character
                    parent_folder_id INTEGER,
                    seq_num INTEGER NOT NULL,
                    FOREIGN KEY (parent_folder_id) REFERENCES bookmark_folders(id) ON DELETE CASCADE
                );
                INSERT INTO bookmark_folders (id, name, is_internal, parent_folder_id, seq_num) VALUES
                    (1, 'Bookmarks Bar', 1, NULL, 0),
                    (2, 'Favorites', 1, NULL, 1);
            )"),
            DbDown(R"(
                DROP TABLE bookmark_folders;
                DROP TABLE bookmarks;
            )"),
        },
        DbMigration{
            // change seq_num type from INTEGER to REAL
            "0004.cc844aa7-3e18-4447-be31-791587873bc6",
            DbUp(R"(
                -- step 1: create new table
                CREATE TABLE bookmark_folders_new (
                    id INTEGER PRIMARY KEY,
                    name TEXT NOT NULL,
                    is_internal INTEGER NOT NULL,
                    parent_folder_id INTEGER,
                    seq_num REAL NOT NULL,
                    FOREIGN KEY (parent_folder_id) REFERENCES bookmark_folders_new(id) ON DELETE CASCADE
                );

                -- step 2: copy data from old table to new table
                INSERT INTO bookmark_folders_new (
                    id, name, is_internal, parent_folder_id, seq_num
                ) SELECT
                    id, name, is_internal, parent_folder_id, seq_num
                FROM bookmark_folders;

                -- step 3: drop the old table
                DROP TABLE bookmark_folders;

                -- step 4: rename the new table to the original name
                ALTER TABLE bookmark_folders_new RENAME TO bookmark_folders;

                ------------------------------------------------------------

                -- step 1: create new table
                CREATE TABLE bookmarks_new (
                    id INTEGER PRIMARY KEY,
                    name TEXT NOT NULL,
                    address TEXT NOT NULL,
                    invocation_keyword TEXT NOT NULL,
                    parent_folder_id INTEGER,
                    seq_num REAL NOT NULL,
                    FOREIGN KEY (parent_folder_id) REFERENCES bookmark_folders(id) ON DELETE CASCADE
                );

                -- step 2: copy data from old table to new table
                INSERT INTO bookmarks_new (
                    id, name, address, invocation_keyword, parent_folder_id, seq_num
                ) SELECT
                    id, name, address, invocation_keyword, parent_folder_id, seq_num
                FROM bookmarks;

                -- step 3: drop the old table
                DROP TABLE bookmarks;

                -- step 4: rename the new table to the original name
                ALTER TABLE bookmarks_new RENAME TO bookmarks;
            )"),
            DbDown(R"(
                -- step 1: create new table
                CREATE TABLE bookmark_folders_new (
                    id INTEGER PRIMARY KEY,
                    name TEXT NOT NULL,
                    is_internal INTEGER NOT NULL,
                    parent_folder_id INTEGER,
                    seq_num INTEGER NOT NULL,
                    FOREIGN KEY (parent_folder_id) REFERENCES bookmark_folders_new(id) ON DELETE CASCADE
                );

                -- step 2: copy data from old table to new table
                INSERT INTO bookmark_folders_new (
                    id, name, is_internal, parent_folder_id, seq_num
                ) SELECT
                    id, name, is_internal, parent_folder_id, seq_num
                FROM bookmark_folders;

                -- step 3: drop the old table
                DROP TABLE bookmark_folders;

                -- step 4: rename the new table to the original name
                ALTER TABLE bookmark_folders_new RENAME TO bookmark_folders;

                ------------------------------------------------------------

                -- step 1: create new table
                CREATE TABLE bookmarks_new (
                    id INTEGER PRIMARY KEY,
                    name TEXT NOT NULL,
                    address TEXT NOT NULL,
                    invocation_keyword TEXT NOT NULL,
                    parent_folder_id INTEGER,
                    seq_num INTEGER NOT NULL,
                    FOREIGN KEY (parent_folder_id) REFERENCES bookmark_folders(id) ON DELETE CASCADE
                );

                -- step 2: copy data from old table to new table
                INSERT INTO bookmarks_new (
                    id, name, address, invocation_keyword, parent_folder_id, seq_num
                ) SELECT
                    id, name, address, invocation_keyword, parent_folder_id, seq_num
                FROM bookmarks;

                -- step 3: drop the old table
                DROP TABLE bookmarks;

                -- step 4: rename the new table to the original name
                ALTER TABLE bookmarks_new RENAME TO bookmarks;
            )"),
        },
        DbMigration{
            // note: real (double) precision numbers can lead to precission issues
            "0005.6af1a126-a3b2-44bb-8f2c-e2122efabb54",
            DbUp(R"(
                CREATE UNIQUE INDEX idx_unique_bookmark_folder_seq_num ON bookmark_folders (parent_folder_id, seq_num);
                CREATE UNIQUE INDEX idx_unique_bookmark_seq_num ON bookmarks (parent_folder_id, seq_num);
            )"),
            DbDown(R"(
                DROP INDEX idx_unique_bookmark_seq_num;
                DROP INDEX idx_unique_bookmark_folder_seq_num;
            )"),
        },
        DbMigration{
            // note: real (double) precision numbers can lead to precission issues
            "0006.918f64a6-1860-4dfc-b40e-20fb101311c2",
            DbUp(R"(
                DROP INDEX idx_unique_bookmark_seq_num;
                DROP INDEX idx_unique_bookmark_folder_seq_num;
            )"),
            DbDown(R"(
                CREATE UNIQUE INDEX idx_unique_bookmark_folder_seq_num ON bookmark_folders (parent_folder_id, seq_num);
                CREATE UNIQUE INDEX idx_unique_bookmark_seq_num ON bookmarks (parent_folder_id, seq_num);
            )"),
        },
        DbMigration{
            "0007.3d4b758d-ab05-4d5f-b4a9-fd068b51d951",
            DbUp(R"(
                CREATE UNIQUE INDEX idx_unique_bookmark_folder_seq_num
                    ON bookmark_folders(parent_folder_id, seq_num)
                    WHERE parent_folder_id IS NOT NULL;
                CREATE UNIQUE INDEX idx_unique_bookmark_folder_seq_num_root
                    ON bookmark_folders(seq_num)
                    WHERE parent_folder_id IS NULL;
            )"),
            DbDown(R"(
                DROP INDEX idx_unique_bookmark_folder_seq_num_root;
                DROP INDEX idx_unique_bookmark_folder_seq_num;
            )"),
        },
        DbMigration{
            "0008.71285e2c-1645-4cae-9e81-35a0bfb89503",
            DbUp(R"(
                CREATE TABLE history (
                    id INTEGER PRIMARY KEY,
                    visited_at INTEGER NOT NULL,
                    website TEXT NOT NULL,
                    address TEXT NOT NULL
                );
                CREATE INDEX idx_history_visited_at ON history(visited_at);
            )"),
            DbDown(R"(
                DROP INDEX idx_history_visited_at;
                DROP TABLE history;
            )"),
        },
        DbMigration{
            "0009.70a9d19f-0b3b-442e-bdac-164c65aa41e6",
            DbUp(R"(
                ALTER TABLE history RENAME TO history_entries;
            )"),
            DbDown(R"(
                ALTER TABLE history_entries RENAME TO history;
            )"),
        },
        DbMigration{
            "0010.b6fc369b-927d-4bf9-8f99-e7354ac13b47",
            DbUp(R"(
                CREATE UNIQUE INDEX idx_unique_bookmark_seq_num
                    ON bookmarks(parent_folder_id, seq_num)
                    WHERE parent_folder_id IS NOT NULL;
                CREATE UNIQUE INDEX idx_unique_bookmark_seq_num_root
                    ON bookmarks(seq_num)
                    WHERE parent_folder_id IS NULL;
            )"),
            DbDown(R"(
                DROP INDEX idx_unique_bookmark_seq_num_root;
                DROP INDEX idx_unique_bookmark_seq_num;
            )"),
        },
        DbMigration{
            "0011.ebd7e3ba-fddb-4c10-9e81-6ac034604394",
            DbUp(R"(
                CREATE TABLE profiles (
                    id INTEGER PRIMARY KEY,
                    name TEXT NOT NULL DEFAULT '',
                    icon_name TEXT NOT NULL DEFAULT '',
                    show_in_address_bar INTEGER DEFAULT 1,
                    show_badge_in_app_icon INTEGER DEFAULT 1,
                    keyboard_shortcut TEXT NOT NULL DEFAULT '',
                    seq_num REAL NOT NULL DEFAULT 0.0
                );
                CREATE UNIQUE INDEX idx_unique_profile_seq_num
                    ON profiles(seq_num);

                CREATE TABLE reading_list_entries (
                    id INTEGER PRIMARY KEY,
                    profile_id INTEGER NOT NULL,
                    added_on INTEGER NOT NULL,
                    icon_name TEXT NOT NULL DEFAULT '',
                    title TEXT NOT NULL DEFAULT '',
                    address TEXT NOT NULL DEFAULT '',
                    referrer TEXT NOT NULL DEFAULT '',
                    is_unread INTEGER NOT NULL DEFAULT 1,
                    is_saved_offline INTEGER NOT NULL DEFAULT 0,
                    FOREIGN KEY (profile_id) REFERENCES profiles(id) ON DELETE CASCADE
                );

                CREATE TABLE windows (
                    id INTEGER PRIMARY KEY,
                    profile_id INTEGER NOT NULL,
                    name TEXT NOT NULL DEFAULT '',
                    keyboard_shortcut TEXT NOT NULL DEFAULT '',
                    is_suspended INTEGER NOT NULL DEFAULT 0,
                    seq_num REAL NOT NULL DEFAULT 0.0,
                    is_tab_group INTEGER NOT NULL DEFAULT 0,
                    FOREIGN KEY (profile_id) REFERENCES profiles(id) ON DELETE CASCADE
                );
                CREATE UNIQUE INDEX idx_unique_window_seq_num
                    ON windows(seq_num);
                
                CREATE TABLE tabs (
                    id INTEGER PRIMARY KEY,
                    window_id INTEGER NOT NULL,
                    name TEXT NOT NULL DEFAULT '',
                    icon_name TEXT NOT NULL DEFAULT '',
                    is_pinned INTEGER NOT NULL DEFAULT 0,
                    is_suspended INTEGER NOT NULL DEFAULT 0,
                    seq_num REAL NOT NULL DEFAULT 0.0,
                    FOREIGN KEY (window_id) REFERENCES windows(id) ON DELETE CASCADE
                );
                CREATE UNIQUE INDEX idx_unique_tab_seq_num
                    ON tabs(seq_num);
            )"),
            DbDown(R"(
                DROP INDEX idx_unique_tab_seq_num;
                DROP TABLE tabs;

                DROP INDEX idx_unique_window_seq_num;
                DROP TABLE windows;

                DROP INDEX idx_unique_profile_seq_num;
                DROP TABLE profiles;
            )"),
        },
        DbMigration{
            "0012.0f3c1d8b-4e5a-4c6b-9f7d-8f1e2c3d4e5f",
            DbUp(R"(
                ALTER TABLE profiles ADD COLUMN is_internal INTEGER NOT NULL DEFAULT 0;
                INSERT INTO profiles (name, icon_name, show_in_address_bar, show_badge_in_app_icon, keyboard_shortcut, seq_num, is_internal)
                VALUES ('Default', '<span foreground="#2646DE">⬤</span>', 1, 1, '', 0.0, 1);
            )"),
            DbDown(R"(
                DELETE FROM profiles WHERE is_internal = 1;
                ALTER TABLE profiles DROP COLUMN is_internal;
            )"),
        },
        DbMigration{
            "0013.4d0f9ae3-3372-44a5-83a2-235cca148a42",
            DbUp(R"(
                CREATE INDEX idx_history_entries_visited_at
                    ON history_entries(visited_at);
            )"),
            DbDown(R"(
                DROP INDEX idx_history_entries_visited_at;
            )"),
        },
        DbMigration{
            "0014.5a6b7c8d-9e0f-1a2b-3c4d-5e6f7a8b9c0d",
            DbUp(R"(
                ALTER TABLE profiles ADD COLUMN is_default INTEGER NOT NULL DEFAULT 0;
                UPDATE profiles SET is_default = 1 WHERE is_internal = 1;

                -- Ensure that there is always at most one default profile
                CREATE UNIQUE INDEX idx_unique_profile_is_default
                    ON profiles(is_default)
                    WHERE is_default = 1;

                -- If the current default profile is deleted, set the original internal profile
                -- as the new default profile
                CREATE TRIGGER reassign_default_profile_on_delete
                    BEFORE DELETE ON profiles
                    WHEN OLD.is_default = 1
                    BEGIN
                        UPDATE profiles
                        SET is_default = 1
                        WHERE id = (
                            SELECT id FROM profiles
                                WHERE is_internal = 1
                                ORDER BY id
                                LIMIT 1
                        );
                    END;
            )"),
            DbDown(R"(
                DROP INDEX idx_unique_profile_is_default;
                DROP TRIGGER reassign_default_profile_on_delete;
                ALTER TABLE profiles DROP COLUMN is_default;
            )"),
        },
        DbMigration{ // adds profile_id to bookmarks table
            "0015.363a25e7-863d-4a50-b34b-50bb1f2022fd",
            DbUp(R"(
                -- create temporary table
                CREATE TABLE bookmarks_new (
                    id INTEGER PRIMARY KEY,
                    profile_id INTEGER NOT NULL,
                    name TEXT NOT NULL DEFAULT '',
                    address TEXT NOT NULL DEFAULT '',
                    invocation_keyword TEXT NOT NULL DEFAULT '',
                    parent_folder_id INTEGER, -- may be NULL for root bookmarks
                    seq_num REAL NOT NULL,
                    FOREIGN KEY (profile_id) REFERENCES profiles(id) ON DELETE CASCADE,
                    FOREIGN KEY (parent_folder_id) REFERENCES bookmark_folders(id) ON DELETE CASCADE
                );

                -- copy data from old table to new table
                INSERT INTO bookmarks_new (id, profile_id, name, address, invocation_keyword, parent_folder_id, seq_num)
                    SELECT id, (SELECT id FROM profiles WHERE is_default = 1), name, address, invocation_keyword,
                        parent_folder_id, seq_num FROM bookmarks;

                -- replace the old table
                DROP TABLE bookmarks;
                ALTER TABLE bookmarks_new RENAME TO bookmarks;

                -- create index on profile_id
                CREATE INDEX idx_bookmarks_profile_id
                    ON bookmarks(profile_id);
            )"),
            DbDown(R"(
                DROP INDEX idx_bookmarks_profile_id;
                ALTER TABLE bookmarks DROP COLUMN profile_id;
            )"),
        },
        DbMigration{ // adds profile_id to bookmark_folders table
            "0016.2e82f835-b742-42a8-a9c6-b7e264b8fd78",
            DbUp(R"(
                -- create temporary table
                CREATE TABLE bookmark_folders_new (
                    id INTEGER PRIMARY KEY,
                    profile_id INTEGER NOT NULL,
                    name TEXT NOT NULL DEFAULT '',
                    is_internal INTEGER NOT NULL DEFAULT 0,
                    parent_folder_id INTEGER, -- may be NULL for root folders
                    seq_num REAL NOT NULL,
                    FOREIGN KEY (profile_id) REFERENCES profiles(id) ON DELETE CASCADE,
                    FOREIGN KEY (parent_folder_id) REFERENCES bookmark_folders_new(id) ON DELETE CASCADE
                );

                -- copy data from old table to new table
                INSERT INTO bookmark_folders_new (id, profile_id, name, is_internal, parent_folder_id, seq_num)
                    SELECT id, (SELECT id FROM profiles WHERE is_default = 1), name, is_internal,
                        parent_folder_id, seq_num FROM bookmark_folders;

                -- replace the old table
                DROP TABLE bookmark_folders;
                ALTER TABLE bookmark_folders_new RENAME TO bookmark_folders;

                -- create index on profile_id
                CREATE INDEX idx_bookmark_folders_profile_id
                    ON bookmark_folders(profile_id);
            )"),
            DbDown(R"(
                DROP INDEX idx_bookmark_folders_profile_id;
                ALTER TABLE bookmark_folders DROP COLUMN profile_id;
            )"),
        },
        DbMigration{ // adds profile_id to history_entries table
            "0017.1f2e3d4c-5b6a-7e8f-9a0b-c1d2e3f4a5b6",
            DbUp(R"(
                -- create temporary table
                CREATE TABLE history_entries_new (
                    id INTEGER PRIMARY KEY,
                    profile_id INTEGER NOT NULL,
                    visited_at INTEGER NOT NULL,
                    website TEXT NOT NULL DEFAULT '',
                    address TEXT NOT NULL DEFAULT '',
                    FOREIGN KEY (profile_id) REFERENCES profiles(id) ON DELETE CASCADE
                );

                -- copy data from old table to new table
                INSERT INTO history_entries_new (id, profile_id, visited_at, website, address)
                    SELECT id, (SELECT id FROM profiles WHERE is_default = 1), visited_at,
                        website, address FROM history_entries;
                
                -- replace the old table
                DROP TABLE history_entries;
                ALTER TABLE history_entries_new RENAME TO history_entries;

                -- create index on profile_id
                CREATE INDEX idx_history_entries_profile_id
                    ON history_entries(profile_id);
            )"),
            DbDown(R"(
                DROP INDEX idx_history_entries_profile_id;
                ALTER TABLE history_entries DROP COLUMN profile_id;
            )"),
        },
        DbMigration{ // fix existing trigger
            "0018.2a3b4c5d-6e7f-8a9b-0c1d-e2f3a4b5c6d7",
            DbUp(R"(
                DROP TRIGGER reassign_default_profile_on_delete;
                CREATE TRIGGER reassign_default_profile_on_delete
                    AFTER DELETE ON profiles
                    WHEN OLD.is_default = 1
                    BEGIN
                        UPDATE profiles
                        SET is_default = 1
                        WHERE id = (
                            SELECT id FROM profiles
                                WHERE is_internal = 1
                                ORDER BY id
                                LIMIT 1
                        );
                    END;
            )"),
            DbDown(R"(
                DROP TRIGGER reassign_default_profile_on_delete;
                CREATE TRIGGER reassign_default_profile_on_delete
                    BEFORE DELETE ON profiles
                    WHEN OLD.is_default = 1
                    BEGIN
                        UPDATE profiles
                        SET is_default = 1
                        WHERE id = (
                            SELECT id FROM profiles
                                WHERE is_internal = 1
                                ORDER BY id
                                LIMIT 1
                        );
                    END;
            )"),
        },
        DbMigration{ // fix seq_num uniqueness of windows and tabs
            "0019.3b4c5d6e-7f8a-9b0c-1d2e-3f4a5b6c7d8e",
            DbUp(R"(
                DROP INDEX idx_unique_window_seq_num;
                CREATE UNIQUE INDEX idx_unique_window_seq_num
                    ON windows(profile_id, seq_num);

                DROP INDEX idx_unique_tab_seq_num;
                CREATE UNIQUE INDEX idx_unique_tab_seq_num
                    ON tabs(window_id, seq_num);
            )"),
            DbDown(R"(
                CREATE UNIQUE INDEX idx_unique_tab_seq_num
                    ON tabs(seq_num);

                CREATE UNIQUE INDEX idx_unique_window_seq_num
                    ON windows(seq_num);
            )"),
        },
        DbMigration{ // fix seq_num uniqueness of bookmark_folders and bookmarks
            "0020.4c5d6e7f-8a9b-0c1d-2e3f-4a5b6c7d8e9f",
            DbUp(R"(
                CREATE UNIQUE INDEX idx_unique_bookmark_folder_seq_num
                    ON bookmark_folders(profile_id, seq_num);

                CREATE UNIQUE INDEX idx_unique_bookmark_seq_num
                    ON bookmarks(profile_id, seq_num);
            )"),
            DbDown(R"(
                DROP INDEX idx_unique_bookmark_seq_num;

                DROP INDEX idx_unique_bookmark_folder_seq_num;
            )"),
        },
        DbMigration{
            "0021.5d6e7f8a-9b0c-1d2e-3f4a-5b6c7d8e9f0a",
            DbUp(R"(
                ALTER TABLE tabs ADD COLUMN uri TEXT NOT NULL DEFAULT '';
                ALTER TABLE tabs ADD COLUMN session_state BLOB NOT NULL DEFAULT '';
            )"),
            DbDown(R"(
                ALTER TABLE tabs DROP COLUMN session_state;
                ALTER TABLE tabs DROP COLUMN uri;
            )"),
        },
        DbMigration{
            "0022.6e7f8a9b-0c1d-2e3f-4a5b-6c7d8e9f0a1b",
            DbUp(R"(
                DROP INDEX IF EXISTS idx_unique_bookmark_folder_seq_num;
                CREATE UNIQUE INDEX idx_unique_bookmark_folder_seq_num
                    ON bookmark_folders(profile_id, parent_folder_id, seq_num)
                    WHERE parent_folder_id IS NOT NULL;
                CREATE UNIQUE INDEX idx_unique_bookmark_folder_seq_num_root
                    ON bookmark_folders(profile_id, seq_num)
                    WHERE parent_folder_id IS NULL;

                DROP INDEX IF EXISTS idx_unique_bookmark_seq_num;
                CREATE UNIQUE INDEX idx_unique_bookmark_seq_num
                    ON bookmarks(profile_id, parent_folder_id, seq_num)
                    WHERE parent_folder_id IS NOT NULL;
                CREATE UNIQUE INDEX idx_unique_bookmark_seq_num_root
                    ON bookmarks(profile_id, seq_num)
                    WHERE parent_folder_id IS NULL;
            )"),
            DbDown(R"(
                DROP INDEX idx_unique_bookmark_folder_seq_num_root;
                DROP INDEX idx_unique_bookmark_folder_seq_num;
                CREATE UNIQUE INDEX idx_unique_bookmark_folder_seq_num
                    ON bookmark_folders(profile_id, seq_num);

                DROP INDEX idx_unique_bookmark_seq_num_root;
                DROP INDEX idx_unique_bookmark_seq_num;
                CREATE UNIQUE INDEX idx_unique_bookmark_seq_num
                    ON bookmarks(profile_id, seq_num);
            )"),
        },
        DbMigration{
            "0023.7f8a9b0c-1d2e-3f4a-5b6c-7d8e9f0a1b2c",
            DbUp(R"(
                ALTER TABLE bookmark_folders ADD COLUMN type INTEGER NOT NULL DEFAULT 0;
                UPDATE bookmark_folders SET type = 1 WHERE is_internal = 1 AND name = 'Bookmarks Bar';
                UPDATE bookmark_folders SET type = 2 WHERE is_internal = 1 AND name = 'Favorites';
            )"),
            DbDown(R"(
                ALTER TABLE bookmark_folders DROP COLUMN type;
            )"),
        },
        DbMigration{
            "0024.8a9b0c1d-2e3f-4a5b-6c7d-8e9f0a1b2c3d",
            DbUp(R"(
                -- Insert trigger
                CREATE TRIGGER validate_bookmark_folders_after_insert
                AFTER INSERT ON bookmark_folders
                FOR EACH ROW
                WHEN NEW.parent_folder_id IS NOT NULL
                BEGIN
                    -- 1. Prevent self-reference
                    SELECT RAISE(ABORT, 'cannot set parent_folder_id to self')
                    WHERE NEW.id = NEW.parent_folder_id;

                    -- 2. Prevent circular reference
                    WITH RECURSIVE ancestors(id) AS (
                        SELECT parent_folder_id FROM bookmark_folders WHERE id = NEW.parent_folder_id
                        UNION ALL
                        SELECT f.parent_folder_id FROM bookmark_folders f
                        JOIN ancestors a ON f.id = a.id
                        WHERE f.parent_folder_id IS NOT NULL
                    )
                    SELECT RAISE(ABORT, 'circular reference with this parent_folder_id detected')
                    FROM ancestors
                    WHERE id = NEW.id;
                END;

                -- Update trigger
                CREATE TRIGGER validate_bookmark_folders_before_update
                BEFORE UPDATE ON bookmark_folders
                FOR EACH ROW
                WHEN NEW.parent_folder_id IS NOT NULL
                BEGIN
                    -- Prevent self-reference
                    SELECT RAISE(ABORT, 'cannot set parent_folder_id to self')
                    WHERE NEW.id = NEW.parent_folder_id;

                    -- Prevent circular reference
                    WITH RECURSIVE ancestors(id, path) AS (
                        SELECT parent_folder_id, printf('%d', parent_folder_id)
                        FROM bookmark_folders WHERE id = NEW.parent_folder_id
                        UNION ALL
                        SELECT f.parent_folder_id, path || ',' || f.parent_folder_id
                        FROM bookmark_folders f
                        JOIN ancestors a ON f.id = a.id
                        WHERE f.parent_folder_id IS NOT NULL
                        AND instr(path, printf('%d', f.parent_folder_id)) = 0
                    )
                    SELECT RAISE(ABORT, 'circular reference with this parent_folder_id detected')
                    FROM ancestors
                    WHERE id = NEW.id;
                END;
            )"),
            DbDown(R"(
                DROP TRIGGER prevent_circular_reference_on_bookmark_folders_update;
                DROP TRIGGER prevent_circular_reference_on_bookmark_folders_insert;
            )"),
        },
        DbMigration{
            "0025.9b0c1d2e-3f4a-5b6c-7d8e-9f0a1b2c3d4e",
            DbUp(R"(
                CREATE TABLE settings (
                    profile_id INTEGER NOT NULL,
                    key TEXT NOT NULL,
                    value TEXT NOT NULL,
                    type TEXT NOT NULL CHECK (type IN ('text', 'integer', 'boolean', 'real')),
                    PRIMARY KEY (profile_id, key),
                    FOREIGN KEY (profile_id) REFERENCES profiles(id) ON DELETE CASCADE
                );
            )"),
            DbDown(R"(
                DROP TABLE settings;
            )"),
        },
        DbMigration{
            "0026.0c1d2e3f-4a5b-6c7d-8e9f-0a1b2c3d4e5f",
            DbUp(R"(
                ALTER TABLE reading_list_entries ADD COLUMN description TEXT NOT NULL DEFAULT '';
            )"),
            DbDown(R"(
                ALTER TABLE reading_list_entries DROP COLUMN description;
            )"),
        },
        DbMigration{
            "0027.1d2e3f4a-5b6c-7d8e-9f0a-1b2c3d4e5f6a",
            DbUp(R"(
                ALTER TABLE windows ADD COLUMN icon_name TEXT NOT NULL DEFAULT '';
            )"),
            DbDown(R"(
                ALTER TABLE windows DROP COLUMN icon_name;
            )"),
        },
        DbMigration{
            "0028.2e3f4a5b-6c7d-8e9f-0a1b-2c3d4e5f6a7b",
            DbUp(R"(
                CREATE TABLE extensions (
                    profile_id INTEGER NOT NULL,
                    sku TEXT NOT NULL,
                    file_path TEXT NOT NULL DEFAULT '',
                    name TEXT NOT NULL DEFAULT '',
                    description TEXT NOT NULL DEFAULT '',
                    icon_ref TEXT NOT NULL DEFAULT '',
                    is_enabled INTEGER NOT NULL DEFAULT 1,
                    last_updated_at INTEGER NOT NULL DEFAULT 0,
                    activate_shortcut TEXT NOT NULL DEFAULT '',
                    lock_shortcut TEXT NOT NULL DEFAULT '',
                    type INTEGER NOT NULL DEFAULT 0,
                    version TEXT NOT NULL DEFAULT '',
                    do_not_update INTEGER NOT NULL DEFAULT 0,

                    PRIMARY KEY (profile_id, sku),
                    FOREIGN KEY (profile_id) REFERENCES profiles(id) ON DELETE CASCADE
                );
                CREATE INDEX idx_extensions_profile_id ON extensions(profile_id);
            )"),
            DbDown(R"(
                DROP INDEX idx_extensions_profile_id;
                DROP TABLE extensions;
            )"),
        },
        DbMigration{
            "0029.3f4a5b6c-7d8e-9f0a-1b2c-3d4e5f6a7b8c",
            DbUp(R"(
                ALTER TABLE extensions ADD COLUMN has_options INTEGER NOT NULL DEFAULT 0;
            )"),
            DbDown(R"(
                ALTER TABLE extensions DROP COLUMN has_options;
            )"),
        },
        DbMigration{
            "0030.4a5b6c7d-8e9f-0a1b-2c3d-4e5f6a7b8c9d",
            DbUp(R"(
                -- We cannot add a proper foreign key constraint easily here
                -- because we would need to recreate the whole table.
                ALTER TABLE windows ADD COLUMN selected_tab_id INTEGER; -- nullable
            )"),
            DbDown(R"(
                ALTER TABLE windows DROP COLUMN selected_tab_id;
            )"),
        },
        DbMigration{
            "0031.5b6c7d8e-9f0a-1b2c-3d4e-4f5a6b7c8d9e",
            DbUp(R"(
                ALTER TABLE bookmarks ADD COLUMN created_at INTEGER NOT NULL DEFAULT 0;
            )"),
            DbDown(R"(
                ALTER TABLE bookmarks DROP COLUMN created_at;
            )"),
        },
        DbMigration{
            "0032.6c7d8e9f-0a1b-2c3d-4e5f-6a7b8c9d0e1f",
            DbUp(R"(
                ALTER TABLE extensions RENAME TO extensions_old;
                DROP INDEX idx_extensions_profile_id;
                CREATE TABLE extensions (
                    id TEXT PRIMARY KEY NOT NULL DEFAULT (
                        lower(
                            hex(randomblob(4)) || '-' ||
                            hex(randomblob(2)) || '-' ||
                            '4' || substr(hex(randomblob(2)), 2) || '-' ||
                            substr('89ab', abs(random()) % 4 + 1, 1) || substr(hex(randomblob(2)), 2) || '-' ||
                            hex(randomblob(6))
                        )
                    ),
                    profile_id INTEGER NOT NULL,
                    sku TEXT NOT NULL DEFAULT '',
                    type INTEGER NOT NULL DEFAULT 0,
                    file_path TEXT NOT NULL DEFAULT '',
                    name TEXT NOT NULL DEFAULT '',
                    description TEXT NOT NULL DEFAULT '',
                    icon_ref TEXT NOT NULL DEFAULT '',
                    is_enabled INTEGER NOT NULL DEFAULT 1,
                    last_updated_at INTEGER NOT NULL DEFAULT 0,
                    activate_shortcut TEXT NOT NULL DEFAULT '',
                    lock_shortcut TEXT NOT NULL DEFAULT '',
                    version TEXT NOT NULL DEFAULT '',
                    do_not_update INTEGER NOT NULL DEFAULT 0,
                    has_options INTEGER NOT NULL DEFAULT 0,

                    FOREIGN KEY (profile_id) REFERENCES profiles(id) ON DELETE CASCADE
                );
                INSERT INTO extensions (
                    profile_id, sku, type, file_path, name, description, icon_ref, is_enabled,
                    last_updated_at, activate_shortcut, lock_shortcut, version, do_not_update, has_options
                ) SELECT
                    profile_id, sku, type, file_path, name, description, icon_ref, is_enabled,
                    last_updated_at, activate_shortcut, lock_shortcut, version, do_not_update, has_options
                FROM extensions_old;
                DROP TABLE extensions_old;
                CREATE UNIQUE INDEX idx_extensions_profile_id_sku_type ON extensions(profile_id, sku, type);
            )"),
            DbDown(R"(
                DROP INDEX idx_extensions_profile_id_sku_type;
                ALTER TABLE extensions RENAME TO extensions_old;
                CREATE TABLE extensions (
                    profile_id INTEGER NOT NULL,
                    sku TEXT NOT NULL,
                    file_path TEXT NOT NULL DEFAULT '',
                    name TEXT NOT NULL DEFAULT '',
                    description TEXT NOT NULL DEFAULT '',
                    icon_ref TEXT NOT NULL DEFAULT '',
                    is_enabled INTEGER NOT NULL DEFAULT 1,
                    last_updated_at INTEGER NOT NULL DEFAULT 0,
                    activate_shortcut TEXT NOT NULL DEFAULT '',
                    lock_shortcut TEXT NOT NULL DEFAULT '',
                    type INTEGER NOT NULL DEFAULT 0,
                    version TEXT NOT NULL DEFAULT '',
                    do_not_update INTEGER NOT NULL DEFAULT 0,
                    has_options INTEGER NOT NULL DEFAULT 0,

                    PRIMARY KEY (profile_id, sku),
                    FOREIGN KEY (profile_id) REFERENCES profiles(id) ON DELETE CASCADE
                );
                INSERT INTO extensions (
                    profile_id, sku, type, file_path, name, description, icon_ref, is_enabled,
                    last_updated_at, activate_shortcut, lock_shortcut, version, do_not_update, has_options
                ) SELECT
                    profile_id, sku, type, file_path, name, description, icon_ref, is_enabled,
                    last_updated_at, activate_shortcut, lock_shortcut, version, do_not_update, has_options
                FROM extensions_old;
                DROP TABLE extensions_old;
                CREATE INDEX idx_extensions_profile_id ON extensions(profile_id);
            )"),
        },
        DbMigration{
            "0033.7d8e9f0a-1b2c-3d4e-5f6a-7b8c9d0e1f2g",
            DbUp(R"(
                ALTER TABLE extensions ADD COLUMN seq_num REAL NOT NULL DEFAULT 0.0;
                WITH numbered AS (
                    SELECT id, ROW_NUMBER() OVER (PARTITION BY profile_id ORDER BY is_enabled DESC, name) AS rn
                    FROM extensions
                )
                UPDATE extensions SET seq_num = (
                    SELECT rn FROM numbered WHERE numbered.id = extensions.id
                );
                CREATE INDEX idx_extensions_profile_id_seq_num ON extensions(profile_id, seq_num);
            )"),
            DbDown(R"(
                DROP INDEX idx_extensions_profile_id_seq_num;
                ALTER TABLE extensions DROP COLUMN seq_num;
            )"),
        },
        DbMigration{
            "0034.8e9f0a1b-2c3d-4e5f-6a7b-8c9d0e1f2g3h",
            DbUp(R"(
                ALTER TABLE bookmark_folders ADD COLUMN is_expanded INTEGER NOT NULL DEFAULT 0;
            )"),
            DbDown(R"(
                ALTER TABLE bookmark_folders DROP COLUMN is_expanded;
            )"),
        },
        DbMigration{
            "0035.9f0a1b2c-3d4e-5f6a-7b8c-9d0e1f2g3h4i",
            DbUp(R"(
                CREATE TABLE tab_back_forward_list_entries (
                    id INTEGER PRIMARY KEY,
                    tab_id INTEGER NOT NULL,
                    webkit_index INTEGER NOT NULL DEFAULT 0,
                    uri TEXT NOT NULL DEFAULT '',
                    FOREIGN KEY (tab_id) REFERENCES tabs(id) ON DELETE CASCADE
                );
                CREATE UNIQUE INDEX idx_tab_back_forward_list_entries_tab_id_webkit_index ON tab_back_forward_list_entries(tab_id, webkit_index);
            )"),
            DbDown(R"(
                DROP TABLE tab_back_forward_list_entries;
            )"),
        },
        DbMigration{
            "0036.0a1b2c3d-4e5f-6a7b-8c9d-0e1f2g3h4i5j",
            DbUp(R"(
                ALTER TABLE profiles ADD COLUMN selected_window_id INTEGER NOT NULL DEFAULT 0;
            )"),
            DbDown(R"(
                ALTER TABLE profiles DROP COLUMN selected_window_id;
            )"),
        },
        DbMigration{
            "0037.1b2c3d4e-5f6a-7b8c-9d0e-1f2g3h4i5j6k",
            DbUp(R"(
                ALTER TABLE windows ADD COLUMN num_tabs INTEGER NOT NULL DEFAULT 0;
            )"),
            DbDown(R"(
                ALTER TABLE windows DROP COLUMN num_tabs;
            )"),
        },
        DbMigration{ // update num_tabs for existing windows
            "0038.2c3d4e5f-6a7b-8c9d-0e1f-2g3h4i5j6k7l",
            DbUp(R"(
                UPDATE windows SET num_tabs = (
                    SELECT COUNT(*) FROM tabs WHERE tabs.window_id = windows.id
                );
            )"),
            DbDown(R"()"),
        },
        DbMigration{
            "0039.3d4e5f6a-7b8c-9d0e-1f2g-3h4i5j6k7l8m",
            DbUp(R"(
                CREATE TABLE snapshot_tabs (
                    id INTEGER PRIMARY KEY,
                    profile_id INTEGER NOT NULL,
                    created_at INTEGER NOT NULL DEFAULT 0,
                    window_seq_num REAL NOT NULL DEFAULT 0.0,
                    name TEXT NOT NULL DEFAULT '',
                    icon_name TEXT NOT NULL DEFAULT '',
                    is_pinned INTEGER NOT NULL DEFAULT 0,
                    is_suspended INTEGER NOT NULL DEFAULT 0,
                    seq_num REAL NOT NULL DEFAULT 0.0,
                    uri TEXT NOT NULL DEFAULT '',
                    session_state BLOB NOT NULL DEFAULT '',
                    FOREIGN KEY (profile_id) REFERENCES profiles(id) ON DELETE CASCADE
                );
                CREATE UNIQUE INDEX idx_snapshot_tabs_profile_id_created_at_window_seq_num_seq_num ON snapshot_tabs(profile_id, created_at, window_seq_num, seq_num);
            )"),
            DbDown(R"(
                DROP INDEX idx_snapshot_tabs_profile_id_created_at_window_seq_num_seq_num;
                DROP TABLE snapshot_tabs;
            )"),
        },
        DbMigration{
            "0040.4e5f6a7b-8c9d-0e1f-2g3h-4i5j6k7l8m9n",
            DbUp(R"(
                CREATE TABLE recently_closed_tabs (
                    id INTEGER PRIMARY KEY,
                    profile_id INTEGER NOT NULL,
                    name TEXT NOT NULL DEFAULT '',
                    uri TEXT NOT NULL DEFAULT '',
                    closed_at INTEGER NOT NULL DEFAULT 0,
                    icon_name TEXT NOT NULL DEFAULT '',
                    FOREIGN KEY (profile_id) REFERENCES profiles(id) ON DELETE CASCADE
                );
            )"),
            DbDown(R"(
                DROP TABLE recently_closed_tabs;
            )"),
        },
        DbMigration{
            "0041.84feb1a0-d6a0-499d-b906-64e2773c7a43",
            DbUp(R"(
                -- zero (0) for root tabs
                ALTER TABLE tabs ADD COLUMN parent_tab_id INTEGER NOT NULL DEFAULT 0;
                ALTER TABLE tabs ADD COLUMN is_collapsed INTEGER NOT NULL DEFAULT 0;
            )"),
            DbDown(R"(
                ALTER TABLE tabs DROP COLUMN parent_tab_id;
                ALTER TABLE tabs DROP COLUMN is_collapsed;
            )"),
        },
        DbMigration{
            "0042.79296470-39db-45b4-94a3-e531e9af7146",
            DbUp(R"(
                ALTER TABLE snapshot_tabs ADD COLUMN is_tab_group INTEGER NOT NULL DEFAULT 0;
            )"),
            DbDown(R"(
                ALTER TABLE snapshot_tabs DROP COLUMN is_tab_group;
            )"),
        },
        DbMigration{
            "0043.a1b2c3d4-e5f6-7a8b-9c0d-e1f2a3b4c5d6",
            DbUp(R"(
                ALTER TABLE windows ADD COLUMN has_ui_window INTEGER NOT NULL DEFAULT 0;
            )"),
            DbDown(R"(
                ALTER TABLE windows DROP COLUMN has_ui_window;
            )"),
        },
        DbMigration{
            "0044.b2c3d4e5-f6a7-8b9c-0de1-f2a3b4c5d6e7",
            DbUp(R"(
                ALTER TABLE snapshot_tabs ADD COLUMN has_ui_window INTEGER NOT NULL DEFAULT 0;
                ALTER TABLE snapshot_tabs ADD COLUMN is_window_selected INTEGER NOT NULL DEFAULT 0;
            )"),
            DbDown(R"(
                ALTER TABLE snapshot_tabs DROP COLUMN is_window_selected;
                ALTER TABLE snapshot_tabs DROP COLUMN has_ui_window;
            )"),
        },
        DbMigration{
            "0045.c3d4e5f6-a7b8-9c0d-e1f2-a3b4c5d6e7f8",
            DbUp(R"(
                ALTER TABLE snapshot_tabs ADD COLUMN window_name TEXT NOT NULL DEFAULT '';
                ALTER TABLE snapshot_tabs ADD COLUMN window_icon_name TEXT NOT NULL DEFAULT '';
                ALTER TABLE snapshot_tabs ADD COLUMN window_keyboard_shortcut TEXT NOT NULL DEFAULT '';
                ALTER TABLE snapshot_tabs ADD COLUMN window_is_suspended INTEGER NOT NULL DEFAULT 0;
                ALTER TABLE snapshot_tabs ADD COLUMN is_tab_selected INTEGER NOT NULL DEFAULT 0;
            )"),
            DbDown(R"(
                ALTER TABLE snapshot_tabs DROP COLUMN is_tab_selected;
                ALTER TABLE snapshot_tabs DROP COLUMN window_is_suspended;
                ALTER TABLE snapshot_tabs DROP COLUMN window_keyboard_shortcut;
                ALTER TABLE snapshot_tabs DROP COLUMN window_icon_name;
                ALTER TABLE snapshot_tabs DROP COLUMN window_name;
            )"),
        },
        DbMigration{
            "0046.d4e5f6a7-b8c9-0de1-f2a3-b4c5d6e7f809",
            DbUp(R"(
                ALTER TABLE tabs ADD COLUMN back_forward_list_index INTEGER NOT NULL DEFAULT 0;
                ALTER TABLE tabs ADD COLUMN webkit_back_forward_list_index INTEGER NOT NULL DEFAULT 0;
                ALTER TABLE snapshot_tabs ADD COLUMN back_forward_list_index INTEGER NOT NULL DEFAULT 0;
                ALTER TABLE snapshot_tabs ADD COLUMN webkit_back_forward_list_index INTEGER NOT NULL DEFAULT 0
            )"),
            DbDown(R"(
                ALTER TABLE snapshot_tabs DROP COLUMN webkit_back_forward_list_index;
                ALTER TABLE snapshot_tabs DROP COLUMN back_forward_list_index;
                ALTER TABLE tabs DROP COLUMN webkit_back_forward_list_index;
                ALTER TABLE tabs DROP COLUMN back_forward_list_index;
            )"),
        },
        DbMigration{
            "0047.e5f6a7b8-c9d0-de1f-2a3b-4c5d6e7f8091",
            DbUp(R"(
                CREATE TABLE snapshot_back_forward_list_entries (
                    id INTEGER PRIMARY KEY,
                    snapshot_tab_id INTEGER NOT NULL,
                    webkit_index INTEGER NOT NULL DEFAULT 0,
                    uri TEXT NOT NULL DEFAULT '',
                    FOREIGN KEY (snapshot_tab_id) REFERENCES snapshot_tabs(id) ON DELETE CASCADE
                );
                CREATE UNIQUE INDEX idx_snapshot_back_forward_list_entries_snapshot_tab_id_webkit_index
                    ON snapshot_back_forward_list_entries(snapshot_tab_id, webkit_index);
            )"),
            DbDown(R"(
                DROP INDEX idx_snapshot_back_forward_list_entries_snapshot_tab_id_webkit_index;
                DROP TABLE snapshot_back_forward_list_entries;
            )"),
        },
        DbMigration{
            "0048.f6a7b8c9-0de1-f2a3-b4c5-d6e7f8091a2b",
            DbUp(R"(
                ALTER TABLE tabs ADD COLUMN merged_back_forward_list_length INTEGER NOT NULL DEFAULT 0;
                ALTER TABLE snapshot_tabs ADD COLUMN merged_back_forward_list_length INTEGER NOT NULL DEFAULT 0;
            )"),
            DbDown(R"(
                ALTER TABLE snapshot_tabs DROP COLUMN merged_back_forward_list_length;
                ALTER TABLE tabs DROP COLUMN merged_back_forward_list_length;
            )"),
        },
        DbMigration{
            "0049.a7b8c9d0-de1f-2a3b-4c5d-6e7f8091a2b3",
            DbUp(R"(
                UPDATE tabs SET uri = 'orion://newtab/' WHERE uri = '' OR uri IS NULL;
                UPDATE snapshot_tabs SET uri = 'orion://newtab/' WHERE uri = '' OR uri IS NULL;
            )"),
            DbDown(R"(
                UPDATE snapshot_tabs SET uri = '' WHERE uri = 'orion://newtab/';
                UPDATE tabs SET uri = '' WHERE uri = 'orion://newtab/';
            )"),
        },
        DbMigration{
            "0050.b8c9d0de-1f2a-3b4c-5d6e-7f8091a2b3c4",
            DbUp(R"(
                CREATE TABLE recently_closed_windows (
                    id INTEGER PRIMARY KEY,
                    profile_id INTEGER NOT NULL,
                    name TEXT NOT NULL DEFAULT '',
                    closed_at INTEGER NOT NULL DEFAULT 0,
                    icon_name TEXT NOT NULL DEFAULT '',
                    FOREIGN KEY (profile_id) REFERENCES profiles(id) ON DELETE CASCADE
                );

                -- 1. Create new table with window_id column
                CREATE TABLE new_recently_closed_tabs (
                    id INTEGER PRIMARY KEY,
                    profile_id INTEGER NOT NULL,
                    name TEXT NOT NULL DEFAULT '',
                    uri TEXT NOT NULL DEFAULT '',
                    closed_at INTEGER NOT NULL DEFAULT 0,
                    icon_name TEXT NOT NULL DEFAULT '',
                    window_id INTEGER, -- nullable
                    FOREIGN KEY (profile_id) REFERENCES profiles(id) ON DELETE CASCADE,
                    FOREIGN KEY (window_id) REFERENCES recently_closed_windows(id) ON DELETE CASCADE
                );

                -- 2. Copy old data (with NULL window_id)
                INSERT INTO new_recently_closed_tabs (id, profile_id, name, uri, closed_at, icon_name)
                    SELECT id, profile_id, name, uri, closed_at, icon_name FROM recently_closed_tabs;

                -- 3. Drop the old table
                DROP TABLE recently_closed_tabs;

                -- 4. Rename the new table to the old table's name
                ALTER TABLE new_recently_closed_tabs RENAME TO recently_closed_tabs;

                -- 5. Create index on window_id
                CREATE INDEX idx_recently_closed_tabs_window_id
                    ON recently_closed_tabs(window_id);
            )"),
            DbDown(R"(
                DROP INDEX idx_recently_closed_tabs_window_id;
                ALTER TABLE recently_closed_tabs DROP COLUMN window_id;
            )"),
        },
        DbMigration{
            "0051.c9d0de1f-2a3b-4c5d-6e7f-8091a2b3c4d5",
            DbUp(R"(
                CREATE TABLE undo_redo_tab_steps (
                    id INTEGER PRIMARY KEY,
                    window_id INTEGER NOT NULL,
                    seq_num INTEGER NOT NULL DEFAULT 0,
                    FOREIGN KEY (window_id) REFERENCES windows(id) ON DELETE CASCADE
                );
                CREATE TABLE undo_redo_tabs (
                    id INTEGER PRIMARY KEY,
                    undo_redo_step_id INTEGER NOT NULL,
                    original_tab_id INTEGER NOT NULL DEFAULT 0,
                    original_parent_tab_id INTEGER NOT NULL DEFAULT 0,
                    original_seq_num REAL NOT NULL DEFAULT 0.0,
                    name TEXT NOT NULL DEFAULT '',
                    uri TEXT NOT NULL DEFAULT '',
                    is_pinned INTEGER NOT NULL DEFAULT 0,
                    icon_name TEXT NOT NULL DEFAULT '',
                    session_state BLOB NOT NULL DEFAULT '',
                    FOREIGN KEY (undo_redo_step_id) REFERENCES undo_redo_tab_steps(id) ON DELETE CASCADE
                );
                CREATE TABLE undo_redo_tab_back_forward_list_entries (
                    id INTEGER PRIMARY KEY,
                    undo_redo_tab_id INTEGER NOT NULL,
                    webkit_index INTEGER NOT NULL DEFAULT 0,
                    uri TEXT NOT NULL DEFAULT '',
                    FOREIGN KEY (undo_redo_tab_id) REFERENCES undo_redo_tabs(id) ON DELETE CASCADE
                );
                ALTER TABLE windows ADD COLUMN undo_redo_tab_step_seq_num INTEGER NOT NULL DEFAULT 0;
            )"),
            DbDown(R"(
                ALTER TABLE windows DROP COLUMN undo_redo_tab_step_id;
                DROP TABLE undo_redo_tab_back_forward_list_entries;
                DROP TABLE undo_redo_tabs;
                DROP TABLE undo_redo_tab_steps;
            )"),
        },
        DbMigration{
            "0052.d0de1f2a-3b4c-5d6e-7f80-91a2b3c4d5e6",
            DbUp(R"(
                ALTER TABLE undo_redo_tabs ADD COLUMN back_forward_list_index INTEGER NOT NULL DEFAULT 0;
                ALTER TABLE undo_redo_tabs ADD COLUMN webkit_back_forward_list_index INTEGER NOT NULL DEFAULT 0;
                ALTER TABLE undo_redo_tabs ADD COLUMN merged_back_forward_list_length INTEGER NOT NULL DEFAULT 0;
            )"),
            DbDown(R"(
                ALTER TABLE undo_redo_tabs DROP COLUMN merged_back_forward_list_length;
                ALTER TABLE undo_redo_tabs DROP COLUMN webkit_back_forward_list_index;
                ALTER TABLE undo_redo_tabs DROP COLUMN back_forward_list_index;
            )"),
        },
        DbMigration{
            "0053.e1f2a3b4-c5d6-e7f8-09a1-2b3c4d5e6f70",
            DbUp(R"(
                CREATE UNIQUE INDEX idx_undo_redo_tab_steps_window_id_seq_num
                    ON undo_redo_tab_steps(window_id, seq_num);
            )"),
            DbDown(R"(
                DROP INDEX idx_undo_redo_tab_steps_window_id_seq_num;
            )"),
        },
        DbMigration{
            "0054.f2a3b4c5-d6e7-f809-1a2b-3c4d5e6f7081",
            DbUp(R"(
                ALTER TABLE windows ADD COLUMN prev_selected_tab_id INTEGER NOT NULL DEFAULT 0;
            )"),
            DbDown(R"(
                ALTER TABLE windows DROP COLUMN prev_selected_tab_id;
            )"),
        },
        DbMigration{
            "0055.a3b4c5d6-e7f8-09a1-2b3c-4d5e6f708192",
            DbUp(R"(
                ALTER TABLE tabs ADD COLUMN snapshot_image BLOB NOT NULL DEFAULT '';
            )"),
            DbDown(R"(
                ALTER TABLE tabs DROP COLUMN snapshot_image;
            )"),
        },
        DbMigration{
            "0056.b4c5d6e7-f809-1a2b-3c4d-5e6f708192a3",
            DbUp(R"(
                CREATE TABLE password_items (
                    id INTEGER PRIMARY KEY,
                    profile_id INTEGER NOT NULL,
                    seq_num REAL NOT NULL DEFAULT 0.0,
                    FOREIGN KEY (profile_id) REFERENCES profiles(id) ON DELETE CASCADE
                );
            )"),
            DbDown(R"(
                DROP TABLE password_items;
            )"),
        },
        DbMigration{
            "0057.c5d6e7f8-091a-2b3c-4d5e-6f708192a3b4",
            DbUp(R"(
                DROP TABLE password_items;
            )"),
            DbDown(R"(
                CREATE TABLE password_items (
                    id INTEGER PRIMARY KEY,
                    profile_id INTEGER NOT NULL,
                    seq_num REAL NOT NULL DEFAULT 0.0,
                    FOREIGN KEY (profile_id) REFERENCES profiles(id) ON DELETE CASCADE
                );
            )"),
        },
        DbMigration{
            "0058.d6e7f809-1a2b-3c4d-5e6f-708192a3b4c5",
            DbUp(R"(
                CREATE TABLE extension_storage_areas (
                    extension_id TEXT NOT NULL,
                    storage_id TEXT NOT NULL,
                    key TEXT NOT NULL,
                    value TEXT NOT NULL DEFAULT '',
                    FOREIGN KEY (extension_id) REFERENCES extensions(id) ON DELETE CASCADE
                );
                CREATE UNIQUE INDEX idx_extension_storage_areas_extension_id_storage_id_key
                    ON extension_storage_areas(extension_id, storage_id, key);
            )"),
            DbDown(R"(
                DROP TABLE extension_storage_areas;
            )"),
        },
        DbMigration{
            "0059.a47dd256-026a-4af6-be1e-3493e5a2c26a",
            DbUp(R"(
                ALTER TABLE profiles ADD shared_pinned_tabs_window_id INTEGER;
            )"),
            DbDown(R"(
                ALTER TABLE profiles DROP COLUMN shared_pinned_tabs_window_id;
            )"),
        },
        DbMigration{
            "0060.6fb63e7f-954b-45e0-903f-a63e3c146f40",
            DbUp(R"(
                CREATE TABLE downloads (
                    id INTEGER PRIMARY KEY,
                    profile_id INTEGER NOT NULL,
                    name TEXT NOT NULL,
                    icon_string TEXT NOT NULL,
                    path TEXT NOT NULL,
                    operation_state INTEGER NOT NULL,
                    timestamp INTEGER NOT NULL,
                    total_size INTEGER NOT NULL,
                    current_size INTEGER NOT NULL,
                    FOREIGN KEY (profile_id) REFERENCES profiles(id) ON DELETE CASCADE
                );
            )"),
            DbDown(R"(
                DROP TABLE downloads;
            )"),
        },
        DbMigration{
            "0061.591795a8-1a53-4dae-bc2f-452781560e6e",
            DbUp(R"(
                ALTER TABLE downloads 
                ADD origin_uri TEXT NOT NULL;
            )"),
            DbDown(R"(
                ALTER TABLE downloads
                DROP origin_uri;
            )"),
        },
        DbMigration{
            "0062.7c9e8f1a-2b3c-4d5e-6f70-8192a3b4c5d6",
            DbUp(R"(
                CREATE TABLE search_engines (
                    id INTEGER PRIMARY KEY,
                    profile_id INTEGER NOT NULL,
                    name TEXT NOT NULL,
                    search_url_template TEXT NOT NULL,
                    suggestions_url_template TEXT NOT NULL,
                    keyword TEXT NOT NULL,
                    FOREIGN KEY (profile_id) REFERENCES profiles(id) ON DELETE CASCADE
                );
            )"),
            DbDown(R"(
                DROP TABLE search_engines;
            )"),
        },
        DbMigration{
            "0063.8f1a2b3c-4d5e-6f70-8192-a3b4c5d6e7f8",
            DbUp(R"(
                CREATE UNIQUE INDEX idx_search_engines_profile_id_name ON search_engines(profile_id, name);
            )"),
            DbDown(R"(
                DROP INDEX idx_search_engines_profile_id_name;
            )"),
        },
        DbMigration{
            "0064.9e8f1a2b-3c4d-5e6f-7081-92a3b4c5d6e7",
            DbUp(R"(
                ALTER TABLE profiles ADD COLUMN is_test INTEGER NOT NULL DEFAULT 0;
            )"),
            DbDown(R"(
                ALTER TABLE profiles DROP COLUMN is_test;
            )"),
        },
        DbMigration{
            "0065.a1b2c3d4-5e6f-7081-92a3-b4c5d6e7f8g9",
            DbUp(R"(
                ALTER TABLE windows ADD COLUMN is_focus_mode INTEGER NOT NULL DEFAULT 0;
            )"),
            DbDown(R"(
                ALTER TABLE windows DROP COLUMN is_focus_mode;
            )"),
        },
        DbMigration{
            "0066.b2c3d4e5-6f70-8192-a3b4-c5d6e7f8g9h0",
            DbUp(R"(
                CREATE TABLE filter_lists (
                    id INTEGER PRIMARY KEY,
                    profile_id INTEGER NOT NULL,
                    name TEXT NOT NULL,
                    version TEXT NOT NULL,
                    FOREIGN KEY (profile_id) REFERENCES profiles(id) ON DELETE CASCADE
                );
                CREATE UNIQUE INDEX idx_filter_lists_profile_id_name ON filter_lists(profile_id, name);
            )"),
            DbDown(R"(
                DROP INDEX idx_filter_lists_profile_id_name;
                DROP TABLE filter_lists;
            )"),
        },
        DbMigration{
            "0067.58effccf-efc0-471e-afaf-93953ee13d9e",
            DbUp(R"(
                CREATE TABLE common_settings (
                    key TEXT PRIMARY KEY,
                    value TEXT NOT NULL,
                    type TEXT NOT NULL CHECK (type IN ('text', 'integer', 'boolean', 'real'))
                );
            )"),
            DbDown(R"(
                DROP TABLE common_settings;
            )"),
        },
        // Delete all filter lists that should not be embedded from all profiles.
        // This migration was needed when upgrading to 0.3.0 beta version from
        // older Orion versions.
        DbMigration{
            "0068.c59fa3d9-e528-4588-a93b-2f122caff70f",
            DbUp(R"(
                DELETE FROM filter_lists
                WHERE name NOT IN ('easylist', 'easyprivacy');
            )"),
            DbDown(R"(
            )"),
        },
        DbMigration{
            "0069.aad76da9-2b03-4431-8ff8-2b14133bdd3a",
            DbUp(R"(
                DELETE FROM filter_lists
                WHERE name NOT IN ('easylist', 'easyprivacy');
            )"),
            DbDown(R"(
            )"),
        },
        DbMigration{
            "0070.d5ac633c-ca8b-400d-9e9b-77d5806f77bf",
            DbUp(R"(
                ALTER TABLE windows ADD COLUMN is_private INTEGER NOT NULL DEFAULT 0;
            )"),
            DbDown(R"(
                ALTER TABLE windows DROP COLUMN is_private;
            )"),
        },
        DbMigration{
            "0071.2616eb49-ea3c-4519-86e5-e9532c0b396a",
            DbUp(R"(
                ALTER TABLE filter_lists ADD COLUMN is_private INTEGER NOT NULL DEFAULT 0;
                DROP INDEX idx_filter_lists_profile_id_name;
                CREATE UNIQUE INDEX idx_filter_lists_profile_id_name_is_private ON filter_lists(profile_id, name, is_private);
            )"),
            DbDown(R"(
                ALTER TABLE filter_lists DROP COLUMN is_private;
                DROP INDEX idx_filter_lists_profile_id_name_is_private;
                CREATE UNIQUE INDEX idx_filter_lists_profile_id_name ON filter_lists(profile_id, name);
            )"),
        },
        DbMigration{
            "0072.58ab98b2-9ac9-41b5-bb33-3f446965233c",
            DbUp(R"(
                CREATE TABLE overflow_menu_items (
                    id INTEGER PRIMARY KEY,
                    profile_id INTEGER NOT NULL,
                    action TEXT NOT NULL,
                    command_id INTEGER NOT NULL,
                    idx INTEGER NOT NULL,
                    FOREIGN KEY (profile_id) REFERENCES profiles(id) ON DELETE CASCADE
                );
            )"),
            DbDown(R"(
                DROP TABLE overflow_menu_items;
            )"),
        },
        DbMigration{
            "0070.d7f8a9b0-1c2d-3e4f-5a6b-7c8d9e0f1a2b",
            DbUp(R"(
                CREATE TABLE favicon_icons (
                    id INTEGER PRIMARY KEY,
                    icon_url TEXT NOT NULL UNIQUE,
                    data BLOB,
                    stamp INTEGER NOT NULL DEFAULT 0
                );
                CREATE INDEX idx_favicon_icons_stamp ON favicon_icons(stamp);

                CREATE TABLE favicon_page_urls (
                    page_url TEXT NOT NULL PRIMARY KEY,
                    icon_id INTEGER NOT NULL,
                    FOREIGN KEY (icon_id) REFERENCES favicon_icons(id) ON DELETE CASCADE
                );
            )"),
            DbDown(R"(
                DROP TABLE favicon_page_urls;
                DROP INDEX idx_favicon_icons_stamp;
                DROP TABLE favicon_icons;
            )"),
        },
        DbMigration{
            "0073.237b1b57-f63e-4fee-8618-0ef54778d98e",
            DbUp(R"(
                DROP TABLE overflow_menu_items;
                CREATE TABLE overflow_menu_items (
                    id INTEGER PRIMARY KEY,
                    profile_id INTEGER NOT NULL,
                    action TEXT NOT NULL,
                    command_id INTEGER NOT NULL DEFAULT 0,
                    idx INTEGER NOT NULL,
                    FOREIGN KEY (profile_id) REFERENCES profiles(id) ON DELETE CASCADE
                );
            )"),
            DbDown(R"(
                DROP TABLE overflow_menu_items;
                CREATE TABLE overflow_menu_items (
                    id INTEGER PRIMARY KEY,
                    profile_id INTEGER NOT NULL,
                    action TEXT NOT NULL,
                    command_id INTEGER NOT NULL,
                    idx INTEGER NOT NULL,
                    FOREIGN KEY (profile_id) REFERENCES profiles(id) ON DELETE CASCADE
                );
            )"),
        },
        DbMigration{
            "0074.0138d3e6-460a-4070-8ad3-c7cabc2caf05",
            DbUp(R"(
                INSERT INTO overflow_menu_items (profile_id, action, idx)
                SELECT p.id, i.action, i.idx
                FROM profiles p
                CROSS JOIN (
                    SELECT 'summarize_page' AS action, 1 AS idx
                    UNION ALL
                    SELECT 'programmable_button' AS action, 2 AS idx
                ) i;
            )"),
            DbDown(R"(
                DELETE FROM overflow_menu_items;
            )"),
        },
        DbMigration{
            "0075.1d61d720-9b38-4628-b238-697490e16758",
            // Do not create unique index on (profile_id, position), it would make it more difficult
            // to normalize positions after create, move, and delete operations on toolbar items.
            DbUp(R"(
                CREATE TABLE toolbar_items (
                    id INTEGER PRIMARY KEY,
                    profile_id INTEGER NOT NULL,
                    item_type INTEGER NOT NULL,
                    position INTEGER NOT NULL,
                    in_overflow_menu INTEGER DEFAULT 0,
                    FOREIGN KEY (profile_id) REFERENCES profiles(id) ON DELETE CASCADE
                );
            )"),
            DbDown(R"(
                DROP TABLE toolbar_items;
            )"),
        },
        DbMigration{
            "0076.1c8f5db4-2e31-4c6c-addc-4e7213c2b524",
            //a programmable button instance can only exist for a corresponding toolbar item, thus we use the same id
            DbUp(R"(
                CREATE TABLE programmable_buttons (
                    id INTEGER PRIMARY KEY,
                    profile_id INTEGER NOT NULL,
                    icon_string TEXT NOT NULL DEFAULT '',
                    action_type INTEGER NOT NULL,
                    action_string TEXT NOT NULL DEFAULT '',
                    keyboard_shortcut TEXT NOT NULL DEFAULT '',
                    FOREIGN KEY (id) REFERENCES toolbar_items(id) ON DELETE CASCADE,
                    FOREIGN KEY (profile_id) REFERENCES profiles(id)
                );
            )"),
            DbDown(R"(
                DROP TABLE programmable_buttons; 
            )"),
        },
        DbMigration{
            "0077.303bc51a-2e38-4b63-aed7-7bdebb0bec11",
            DbUp(R"(
                ALTER TABLE programmable_buttons
                    ADD title TEXT NOT NULL DEFAULT '';
            )"),
            DbDown(R"(
                ALTER TABLE programmable_buttons
                    DROP COLUMN title;
            )"),
        },
        DbMigration{
            "0078.96312b42-256b-4583-ac19-7dec5cec0fed",
            DbUp(R"(
                DELETE FROM toolbar_items;
                WITH items(item_type, position) AS (
                    VALUES
                        (9, 0), -- back_forward
                        (6, 1), -- flexible_space
                        (3, 2), -- privacy
                        (4, 3), -- page_tweaks
                        (5, 4), -- website_settings
                        (8, 5), -- address_and_search
                        (26, 6), -- bookmark
                        (15, 7), -- new_tab
                        (16, 8), -- history
                        (25, 9), -- focus_mode
                        (14, 10), -- share
                        (24, 11), -- tab_overview
                        (13, 12), -- downloads
                        (21, 13) -- overflow_menu
                )
                INSERT INTO toolbar_items (id, profile_id, item_type, position, in_overflow_menu)
                SELECT NULL, p.id, i.item_type, i.position, 0
                FROM profiles AS p
                CROSS JOIN items AS i;
            )"),
            DbDown(R"(
            )")
        },
        DbMigration{
            "0079.ca9b2468-d873-40a6-93a5-5aacda67e240",
            DbUp(R"(
                DELETE FROM toolbar_items;
                WITH items(item_type, position) AS (
                    VALUES
                        (17, 0), -- sidebar
                        (9, 1), -- back_forward
                        (6, 2), -- flexible_space
                        (3, 3), -- privacy
                        (4, 4), -- page_tweaks
                        (5, 5), -- website_settings
                        (8, 6), -- address_and_search
                        (26, 7), -- bookmark
                        (15, 8), -- new_tab
                        (6, 9), -- flexible_space
                        (13, 10), -- downloads
                        (14, 11), -- share
                        (24, 12), -- tab_overview
                        (21, 13) -- overflow_menu
                )
                INSERT INTO toolbar_items (id, profile_id, item_type, position, in_overflow_menu)
                SELECT NULL, p.id, i.item_type, i.position, 0
                FROM profiles AS p
                CROSS JOIN items AS i;
            )"),
            DbDown(R"(
            )")
        },
        DbMigration{
            "0080.5942ab16-70cb-4f43-8cb3-0f50e63b398d",
            DbUp(R"(
                ALTER TABLE toolbar_items ADD COLUMN is_compact_tab_layout INTEGER NOT NULL DEFAULT 0;
            )"),
            DbDown(R"(
                ALTER TABLE toolbar_items DROP COLUMN is_compact_tab_layout;
            )")
        },
        DbMigration{
            "0081.85a9dbcb-c212-4c77-94ed-92e71dae4d65",
            DbUp(R"(
                DELETE FROM toolbar_items;
                WITH items(item_type, position, is_compact_tab_layout, in_overflow_menu) AS (
                    VALUES
                        (17, 0, 0, 0), -- sidebar
                        (9, 1, 0, 0), -- back_forward
                        (6, 2, 0, 0), -- flexible_space
                        (3, 3, 0, 0), -- privacy
                        (4, 4, 0, 0), -- page_tweaks
                        (5, 5, 0, 0), -- website_settings
                        (8, 6, 0, 0), -- address_and_search
                        (26, 7, 0, 0), -- bookmark
                        (15, 8, 0, 0), -- new_tab
                        (6, 9, 0, 0), -- flexible_space
                        (13, 10, 0, 0), -- downloads
                        (14, 11, 0, 0), -- share
                        (24, 12, 0, 0), -- tab_overview
                        (21, 13, 0, 0), -- overflow_menu
                        (1, 14, 0, 1), -- summarize_page
                        (2, 15, 0, 1), -- programmable_button
                        (17, 0, 1, 0), -- sidebar
                        (9, 1, 1, 0), -- back_forward
                        (6, 2, 1, 0), -- flexible_space
                        (8, 3, 1, 0), -- address_and_search
                        (6, 4, 1, 0), -- flexible_space
                        (21, 5, 1, 0), -- overflow_menu
                        (5, 6, 1, 1), -- website_settings
                        (26, 7, 1, 1), -- bookmark
                        (14, 8, 1, 1), -- share
                        (24, 9, 1, 1), -- tab_overview
                        (1, 10, 1, 1), -- summarize_page
                        (2, 11, 1, 1) -- programmable_button
                )
                INSERT INTO toolbar_items (id, profile_id, item_type, position, in_overflow_menu, is_compact_tab_layout)
                SELECT NULL, p.id, i.item_type, i.position, i.in_overflow_menu, i.is_compact_tab_layout
                FROM profiles AS p
                CROSS JOIN items AS i;
            )"),
            DbDown(R"(
            )")
        },
        DbMigration{
            "0082.a2d51acb-9a29-4960-935d-4c3344a14081",
            DbUp(R"(
                DELETE FROM toolbar_items;
                WITH items(item_type, position, is_compact_tab_layout, in_overflow_menu) AS (
                    VALUES
                        (17, 0, 0, 0), -- sidebar
                        (30, 1, 0, 0), -- tab_groups
                        (9, 2, 0, 0), -- back_forward
                        (6, 3, 0, 0), -- flexible_space
                        (3, 4, 0, 0), -- privacy
                        (4, 5, 0, 0), -- page_tweaks
                        (5, 6, 0, 0), -- website_settings
                        (8, 7, 0, 0), -- address_and_search
                        (26, 8, 0, 0), -- bookmark
                        (15, 9, 0, 0), -- new_tab
                        (6, 10, 0, 0), -- flexible_space
                        (13, 11, 0, 0), -- downloads
                        (14, 12, 0, 0), -- share
                        (24, 13, 0, 0), -- tab_overview
                        (21, 14, 0, 0), -- overflow_menu
                        (1, 15, 0, 1), -- summarize_page
                        (2, 16, 0, 1), -- programmable_button
                        (17, 0, 1, 0), -- sidebar
                        (30, 1, 1, 0), -- tab_groups
                        (9, 2, 1, 0), -- back_forward
                        (6, 3, 1, 0), -- flexible_space
                        (8, 4, 1, 0), -- address_and_search
                        (6, 5, 1, 0), -- flexible_space
                        (21, 6, 1, 0), -- overflow_menu
                        (5, 7, 1, 1), -- website_settings
                        (26, 8, 1, 1), -- bookmark
                        (14, 9, 1, 1), -- share
                        (24, 10, 1, 1), -- tab_overview
                        (1, 11, 1, 1), -- summarize_page
                        (2, 12, 1, 1) -- programmable_button
                )
                INSERT INTO toolbar_items (id, profile_id, item_type, position, in_overflow_menu, is_compact_tab_layout)
                SELECT NULL, p.id, i.item_type, i.position, i.in_overflow_menu, i.is_compact_tab_layout
                FROM profiles AS p
                CROSS JOIN items AS i;
            )"),
            DbDown(R"(
            )")
        },
        DbMigration{
            "0083.c5a4f468-4a1d-4ec6-b4b5-f5af84c333ad",
            DbUp(R"(
                CREATE TABLE extension_storage_access_levels (
                    extension_id TEXT NOT NULL,
                    storage_id TEXT NOT NULL,
                    allow_untrusted_contexts INTEGER NOT NULL CHECK (allow_untrusted_contexts IN (0, 1)),
                    PRIMARY KEY (extension_id, storage_id),
                    FOREIGN KEY (extension_id) REFERENCES extensions(id) ON DELETE CASCADE
                );
            )"),
            DbDown(R"(
                DROP TABLE extension_storage_access_levels;
            )")
        },
        DbMigration{
            "0084.6f8f09fd-a9a7-4759-97f2-c280b875d8a6",
            DbUp(R"(
                ALTER TABLE filter_lists ADD COLUMN position INTEGER NOT NULL DEFAULT 0;
                ALTER TABLE filter_lists ADD COLUMN url TEXT NOT NULL DEFAULT '';
                ALTER TABLE filter_lists ADD COLUMN description TEXT NOT NULL DEFAULT '';
                ALTER TABLE filter_lists ADD COLUMN display_name TEXT NOT NULL DEFAULT '';
                ALTER TABLE filter_lists ADD COLUMN is_enabled INTEGER NOT NULL DEFAULT 0;
                ALTER TABLE filter_lists ADD COLUMN updated_at INTEGER NOT NULL DEFAULT 0;
                ALTER TABLE filter_lists ADD COLUMN active_rules_count INTEGER NOT NULL DEFAULT 0;
                ALTER TABLE filter_lists ADD COLUMN filter_type INTEGER NOT NULL DEFAULT 0;
                ALTER TABLE filter_lists ADD COLUMN custom_filter_data TEXT NOT NULL DEFAULT '';

                UPDATE filter_lists SET
                    is_enabled = 1,
                    filter_type = 1;
                
                -- Insert custom filter (disabled by default) into each profile:
                WITH items(name, filter_type, is_enabled, position, custom_filter_data) AS (
                    VALUES
                        ('custom-filter', 3, 0, 0, 'reddit.com##.promotedlink')
                )
                INSERT INTO filter_lists (id, profile_id, name, version, filter_type, is_enabled, position, custom_filter_data)
                SELECT NULL, p.id, i.name, '0', i.filter_type, i.is_enabled, i.position, i.custom_filter_data
                FROM profiles AS p
                CROSS JOIN items AS i;
            )"),
            DbDown(R"(
                ALTER TABLE filter_lists DROP COLUMN position;
                ALTER TABLE filter_lists DROP COLUMN url;
                ALTER TABLE filter_lists DROP COLUMN description;
                ALTER TABLE filter_lists DROP COLUMN display_name;
                ALTER TABLE filter_lists DROP COLUMN is_enabled;
                ALTER TABLE filter_lists DROP COLUMN updated_at;
                ALTER TABLE filter_lists DROP COLUMN active_rules_count;
                ALTER TABLE filter_lists DROP COLUMN filter_type;
                ALTER TABLE filter_lists DROP COLUMN custom_filter_data;

                DELETE FROM filter_lists WHERE filter_type = 3;
            )")
        },
        DbMigration{
            "0085.81a278ac-b0da-4bbd-858d-a5f6c94df973",
            DbUp(R"(
                ALTER TABLE filter_lists ADD COLUMN content_hash TEXT NOT NULL DEFAULT '';
            )"),
            DbDown(R"(
                ALTER TABLE filter_lists DROP COLUMN content_hash;
            )")
        },
        DbMigration{
            "0086.cc868ce1-5706-49c0-bcfb-8770b0542464",
            DbUp(R"(
                DELETE FROM filter_lists WHERE is_private=1;
                DROP INDEX IF EXISTS idx_filter_lists_profile_id_name_is_private;
                ALTER TABLE filter_lists DROP COLUMN is_private;
            )"),
            DbDown(R"(
                ALTER TABLE filter_lists ADD COLUMN is_private INTEGER NOT NULL DEFAULT 0;
                CREATE UNIQUE INDEX idx_filter_lists_profile_id_name_is_private ON filter_lists(profile_id, name, is_private);
                -- No down migration for deleting private filter lists
            )")
        },
        DbMigration{
            "0087.9a0866d1-e3d1-4aae-af45-fb4bc4c7c11f",
            DbUp(R"(
                DROP INDEX idx_unique_window_seq_num;
                CREATE UNIQUE INDEX idx_unique_window_seq_num
                    ON windows(profile_id, is_private, seq_num);
            )"),
            DbDown(R"(
                DROP INDEX idx_unique_window_seq_num;
                -- Worlds may now reuse keys. Restore profile-wide uniqueness
                -- without changing the relative order within either world.
                WITH ordered AS MATERIALIZED (
                    SELECT id, ROW_NUMBER() OVER (
                        PARTITION BY profile_id ORDER BY seq_num, is_private, id
                    ) - 1 AS new_seq_num
                    FROM windows
                )
                UPDATE windows SET seq_num = (
                    SELECT new_seq_num FROM ordered WHERE ordered.id = windows.id
                );
                CREATE UNIQUE INDEX idx_unique_window_seq_num
                    ON windows(profile_id, seq_num);
            )")
        },
    };
}

// Migration name format: see the top of this file

} // namespace orion
