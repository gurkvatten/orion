#include <core/profiles_db/profiles_db.h>
#include <format>
#include <chrono>

namespace orion {

std::vector<std::string>
ProfilesDb::get_undo_redo_tab_columns() const
{
    return {
        "id",
        "undo_redo_step_id",
        "original_tab_id",
        "original_parent_tab_id",
        "original_seq_num",
        "name",
        "uri",
        "is_pinned",
        "icon_name",
        "session_state",
        "back_forward_list_index",
        "webkit_back_forward_list_index",
        "merged_back_forward_list_length",
    };
}

void
ProfilesDb::load_undo_redo_tab_from_stmt(const DbStmt& stmt, UndoRedoTabDto* tab) const
{
    tab->id = UndoRedoTabId(stmt.integer("id"));
    tab->undo_redo_step_id = UndoRedoTabStepId(stmt.integer("undo_redo_step_id"));
    tab->original_tab_id = TabId(stmt.integer("original_tab_id"));
    tab->original_parent_tab_id = TabId(stmt.integer("original_parent_tab_id"));
    tab->original_seq_num = stmt.real("original_seq_num");
    tab->name = stmt.text("name");
    tab->uri = stmt.text("uri");
    tab->is_pinned = stmt.integer("is_pinned");
    tab->icon_name = stmt.text("icon_name");
    tab->session_state = stmt.blob("session_state");
    tab->back_forward_list_index = stmt.integer("back_forward_list_index");
    tab->webkit_back_forward_list_index = stmt.integer("webkit_back_forward_list_index");
    tab->merged_back_forward_list_length = stmt.integer("merged_back_forward_list_length");
}

Error
ProfilesDb::create_undo_redo_tab(CreateUndoRedoTabQuery query, UndoRedoTabId* id)
{
    if (query.undo_redo_step_id == UndoRedoTabStepId(0)) {
        RAISE(ErrorCode::argument, "undo_redo_step_id is not set");
    }
    
    std::string sql = std::format(R"(
        INSERT INTO undo_redo_tabs ({})
        VALUES ({})
        RETURNING {};
    )", Db::join_strings(get_undo_redo_tab_columns(), ", "),
        Db::make_placeholders(get_undo_redo_tab_columns().size()),
        Db::join_strings(get_undo_redo_tab_columns(), ", ")
    );

    std::vector<DbVariant> args = {
        nullptr, // id will be auto-generated
        query.undo_redo_step_id.value,
        query.original_tab_id.value,
        query.original_parent_tab_id.value,
        query.original_seq_num,
        query.name,
        query.uri,
        query.is_pinned,
        query.icon_name,
        query.session_state,
        query.back_forward_list_index,
        query.webkit_back_forward_list_index,
        query.merged_back_forward_list_length,
    };

    DbStmt stmt;
    VERIFY(m_db->query(sql, args, &stmt));
    bool has_row = stmt.next();
    VERIFY(stmt.error());
    RAISE_CHECK(has_row);
    
    UndoRedoTabDto new_tab{};
    load_undo_redo_tab_from_stmt(stmt, &new_tab);
    if (id) {
        *id = new_tab.id;
    }
    SUCCEED();
}

Error
ProfilesDb::get_undo_redo_tab_ids(
    UndoRedoTabStepId step_id, std::vector<UndoRedoTabId>* ids)
{
    if (step_id == UndoRedoTabStepId(0)) {
        RAISE(ErrorCode::argument, "step_id is not set");
    }
    RAISE_CHECK(ids != nullptr);

    // check step_id existence
    {
        std::vector<UndoRedoTabStepDto> steps;
        VERIFY(get_undo_redo_tab_steps({step_id}, &steps));
        if (steps.empty()) {
            RAISE(ErrorCode::not_found, "undo/redo step_id not found");
        }
    }

    std::vector<DbVariant> args = {
        step_id.value,
    };

    std::string sql = std::format(R"(
        SELECT id
        FROM undo_redo_tabs
        WHERE undo_redo_step_id = ?1
        ORDER BY original_seq_num ASC;
    )", Db::join_strings(get_undo_redo_tab_columns(), ", "));

    DbStmt stmt;
    VERIFY(m_db->query(sql, args, &stmt));

    while (stmt.next()) {
        UndoRedoTabId id = UndoRedoTabId(stmt.integer("id"));
        ids->push_back(id);
    }
    VERIFY(stmt.error());
    SUCCEED();
}

Error
ProfilesDb::get_undo_redo_tabs(
    const std::vector<UndoRedoTabId>& ids, std::vector<UndoRedoTabDto>* tabs)
{
    std::vector<DbVariant> args = to_db_variant_vector(ids);

    std::string sql = std::format(R"(
        SELECT {}
        FROM undo_redo_tabs
        WHERE id IN ({})
        ORDER BY undo_redo_step_id, original_seq_num;
    )", Db::join_strings(get_undo_redo_tab_columns(), ", "),
        Db::make_placeholders(ids.size())
    );

    DbStmt stmt;
    VERIFY(m_db->query(sql, args, &stmt));

    while (stmt.next()) {
        UndoRedoTabDto tab{};
        load_undo_redo_tab_from_stmt(stmt, &tab);
        tabs->push_back(tab);
    }
    VERIFY(stmt.error());
    SUCCEED();
}

Error
ProfilesDb::update_undo_redo_tab(UpdateUndoRedoTabQuery query)
{
    if (query.id == UndoRedoTabId(0)) {
        RAISE(ErrorCode::argument, "id is not set");
    }

    // check undo/redo tab existence
    {
        std::vector<UndoRedoTabDto> tabs;
        VERIFY(get_undo_redo_tabs({query.id}, &tabs));
        if (tabs.empty()) {
            RAISE(ErrorCode::not_found, "undo_redo_tab id not found");
        }
    }

    if (!query.update_original_tab_id) {
        // nothing to update
        SUCCEED();
    }

    std::string sql = R"(
        UPDATE undo_redo_tabs
        SET original_tab_id = ?1
        WHERE id = ?2;
    )";

    std::vector<DbVariant> args = {
        query.original_tab_id.value,
        query.id.value,
    };

    VERIFY(m_db->exec(sql, args));
    SUCCEED();
}

} // namespace orion