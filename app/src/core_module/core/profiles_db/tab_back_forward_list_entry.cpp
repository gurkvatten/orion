#include <core/profiles_db/profiles_db.h>
#include <format>

namespace orion {
    
//-------------------------------------------------------------------------
// Tab back-forward list entries
//-------------------------------------------------------------------------

Error
ProfilesDb::get_tab_back_forward_list_entries(TabId tab_id, std::vector<TabBackForwardListEntryDto>* entries)
{
    if (tab_id == TabId(0)) {
        RAISE(ErrorCode::argument, "tab_id is not set");
    }

    std::string sql = R"(
        SELECT id, tab_id, webkit_index, uri
        FROM tab_back_forward_list_entries
        WHERE tab_id = ?1
        ORDER BY webkit_index;
    )";

    DbStmt stmt;
    VERIFY(m_db->query(sql, {tab_id.value}, &stmt));

    while (stmt.next()) {
        TabBackForwardListEntryDto entry{};
        entry.id = TabBackForwardListEntryId(stmt.integer("id"));
        entry.tab_id = TabId(stmt.integer("tab_id"));
        entry.webkit_index = stmt.integer("webkit_index");
        entry.uri = stmt.text("uri");
        entries->emplace_back(entry);
    }
    VERIFY(stmt.error());

    SUCCEED();
}

Error
ProfilesDb::create_tab_back_forward_list_entry(
    CreateTabBackForwardListEntryQuery query, TabBackForwardListEntryDto* entry,
    TabBackForwardListEntriesChange* change)
{
    if (query.tab_id == TabId(0)) {
        RAISE(ErrorCode::argument, "tab_id is not set");
    }

    {
        // Check if an entry with the same tab_id and webkit_index already exists
        std::string sql = R"(
            SELECT COUNT(*) as count
            FROM tab_back_forward_list_entries
            WHERE tab_id = ?1 AND webkit_index = ?2;
        )";

        int64_t count = 0;
        VERIFY(m_db->query_integer(sql, {
            query.tab_id.value,
            query.webkit_index,
        }, &count));

        if (count > 0) {
            log_verbose("create_tab_back_forward_list_entry: entry already exists for tab_id=%ld webkit_index=%d"
                ", nothing to do", query.tab_id.value, query.webkit_index);
            SUCCEED();    
        }
    }

    std::string sql = std::format(R"(
        INSERT INTO tab_back_forward_list_entries (
            tab_id, webkit_index, uri
        ) VALUES ({})
        RETURNING id;
    )", Db::make_placeholders(3));

    DbStmt stmt;
    VERIFY(m_db->query(sql, {
        query.tab_id.value,
        query.webkit_index,
        query.uri,
    }, &stmt));

    bool has_row = stmt.next();
    VERIFY(stmt.error());
    RAISE_CHECK(has_row);

    TabBackForwardListEntryDto new_entry{};
    new_entry.id = TabBackForwardListEntryId(stmt.integer("id"));
    new_entry.tab_id = query.tab_id;
    new_entry.webkit_index = query.webkit_index;
    new_entry.uri = query.uri;
    if (change) {
        change->created.push_back(new_entry);
    }

    if (entry) {
        *entry = new_entry;
    }
    SUCCEED();
}

Error
ProfilesDb::delete_tab_back_forward_list_entries(
    DeleteTabBackForwardListEntriesQuery query, TabBackForwardListEntriesChange* change)
{
    if (query.tab_id == TabId(0)) {
        RAISE(ErrorCode::argument, "tab_id is not set");
    }

    std::string sql = R"(
        DELETE FROM tab_back_forward_list_entries
        WHERE tab_id = ?1 AND webkit_index >= ?2
        RETURNING id, tab_id, webkit_index, uri;
    )";

    std::vector<DbVariant> args = {
        query.tab_id.value,
        query.start_webkit_index,
    };

    DbStmt stmt;
    VERIFY(m_db->query(sql, args, &stmt));

    while (stmt.next()) {
        TabBackForwardListEntryDto entry{};
        entry.id = TabBackForwardListEntryId(stmt.integer("id"));
        entry.tab_id = TabId(stmt.integer("tab_id"));
        entry.webkit_index = stmt.integer("webkit_index");
        entry.uri = stmt.text("uri");

        if (change) {
            change->deleted.push_back(entry);
        }
    }
    VERIFY(stmt.error());
    SUCCEED();
}

} // namespace orion