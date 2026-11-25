#include <core/profiles_db/profiles_db.h>
#include <format>
#include <chrono>

namespace orion {

//-------------------------------------------------------------------------
// Snapshot tab back-forward list entries
//-------------------------------------------------------------------------

std::vector<std::string>
ProfilesDb::get_snapshot_back_forward_entry_columns() const
{
    return {
        "id",
        "snapshot_tab_id",
        "webkit_index",
        "uri",
    };
}

void
ProfilesDb::load_snapshot_back_forward_entry_from_stmt(const DbStmt& stmt, SnapshotBackForwardEntry* entry) const
{
    entry->id = SnapshotBackForwardEntryId(stmt.integer("id"));
    entry->snapshot_tab_id = SnapshotTabId(stmt.integer("snapshot_tab_id"));
    entry->webkit_index = stmt.integer("webkit_index");
    entry->uri = stmt.text("uri");
}

Error
ProfilesDb::get_snapshot_tab_back_forward_list_entries(
    SnapshotTabId snapshot_tab_id, std::vector<SnapshotBackForwardEntry>* entries)
{
    if (snapshot_tab_id == SnapshotTabId(0)) {
        RAISE(ErrorCode::argument, "snapshot_tab_id is not set");
    }
    std::string sql = std::format(R"(
        SELECT {}
        FROM snapshot_back_forward_list_entries
        WHERE snapshot_tab_id = ?1
        ORDER BY webkit_index;
    )", Db::join_strings(get_snapshot_back_forward_entry_columns(), ", "));

    DbStmt stmt;
    VERIFY(m_db->query(sql, {snapshot_tab_id.value}, &stmt));
    while (stmt.next()) {
        SnapshotBackForwardEntry entry{};
        load_snapshot_back_forward_entry_from_stmt(stmt, &entry);
        entries->push_back(entry);
    }
    VERIFY(stmt.error());
    SUCCEED();
}

Error
ProfilesDb::create_snapshot_tab_back_forward_list_entry(SnapshotBackForwardEntry* entry)
{
    RAISE_CHECK(entry);

    std::vector<DbVariant> args{
        nullptr, // id - autoincrement
        entry->snapshot_tab_id.value,
        entry->webkit_index,
        entry->uri,
    };

    std::string sql = std::format(R"(
        INSERT INTO snapshot_back_forward_list_entries ({})
        VALUES ({});
    )", Db::join_strings(get_snapshot_back_forward_entry_columns(), ", "),
        Db::make_placeholders(args.size())
    );

    VERIFY(m_db->exec(sql, args));
    SUCCEED();
}

} // namespace orion