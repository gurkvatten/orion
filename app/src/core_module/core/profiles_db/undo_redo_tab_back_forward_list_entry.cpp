#include <core/profiles_db/profiles_db.h>
#include <format>
#include <chrono>

namespace orion {

std::vector<std::string>
ProfilesDb::get_undo_redo_tab_back_forward_list_entry_columns() const
{
    return {
        "id",
        "undo_redo_tab_id",
        "webkit_index",
        "uri",
    };
}

void
ProfilesDb::load_undo_redo_tab_back_forward_list_entry_from_stmt(
    const DbStmt& stmt, UndoRedoTabBackForwardListEntryDto* entry
) const {
    entry->id = UndoRedoTabBackForwardListEntryId(stmt.integer("id"));
    entry->undo_redo_tab_id = UndoRedoTabId(stmt.integer("undo_redo_tab_id"));
    entry->webkit_index = stmt.integer("webkit_index");
    entry->uri = stmt.text("uri");
}

Error
ProfilesDb::create_undo_redo_tab_back_forward_list_entry(
    CreateUndoRedoTabBackForwardListEntryQuery query, UndoRedoTabBackForwardListEntryId* id)
{
    if (query.undo_redo_tab_id == UndoRedoTabId(0)) {
        RAISE(ErrorCode::argument, "undo_redo_tab_id is not set");
    }
    
    std::string sql = std::format(R"(
        INSERT INTO undo_redo_tab_back_forward_list_entries ({})
        VALUES ({})
        RETURNING {};
    )", Db::join_strings(get_undo_redo_tab_back_forward_list_entry_columns(), ", "),
        Db::make_placeholders(get_undo_redo_tab_back_forward_list_entry_columns().size()),
        Db::join_strings(get_undo_redo_tab_back_forward_list_entry_columns(), ", ")
    );

    std::vector<DbVariant> args = {
        nullptr, // id will be auto-generated
        query.undo_redo_tab_id.value,
        query.webkit_index,
        query.uri,
    };

    DbStmt stmt;
    VERIFY(m_db->query(sql, args, &stmt));
    bool has_row = stmt.next();
    VERIFY(stmt.error());
    RAISE_CHECK(has_row);

    UndoRedoTabBackForwardListEntryDto new_entry{};
    load_undo_redo_tab_back_forward_list_entry_from_stmt(stmt, &new_entry);
    if (id) {
        *id = new_entry.id;
    }
    SUCCEED();
}

Error
ProfilesDb::get_undo_redo_tab_back_forward_list_entries(
    UndoRedoTabId undo_redo_tab_id, std::vector<UndoRedoTabBackForwardListEntryDto>* entries)
{
    if (undo_redo_tab_id == UndoRedoTabId(0)) {
        RAISE(ErrorCode::argument, "undo_redo_tab_id is not set");
    }

    // check undo/redo tab existence
    {
        std::vector<UndoRedoTabDto> tabs;
        VERIFY(get_undo_redo_tabs({undo_redo_tab_id}, &tabs));
        if (tabs.empty()) {
            RAISE(ErrorCode::not_found, "undo_redo_tab_id not found");
        }
    }

    std::string sql = std::format(R"(
        SELECT {}
        FROM undo_redo_tab_back_forward_list_entries
        WHERE undo_redo_tab_id = ?1
        ORDER BY webkit_index ASC;
    )", Db::join_strings(get_undo_redo_tab_back_forward_list_entry_columns(), ", ")
    );

    DbStmt stmt;
    VERIFY(m_db->query(sql, {undo_redo_tab_id.value}, &stmt));

    while (stmt.next()) {
        UndoRedoTabBackForwardListEntryDto entry{};
        load_undo_redo_tab_back_forward_list_entry_from_stmt(stmt, &entry);
        entries->push_back(entry);
    }
    VERIFY(stmt.error());
    SUCCEED();
}

} // namespace orion