#include <format>

#include "profiles_db.h"

namespace orion {

//-------------------------------------------------------------------------
// Programmable Buttons
//-------------------------------------------------------------------------

std::vector<std::string>
ProfilesDb::get_programmable_button_columns() const
{
    return {
        "id",
        "profile_id",
        "title",
        "icon_string",
        "action_type",
        "action_string",
        "keyboard_shortcut"
    };
}

void
ProfilesDb::load_programmable_button_from_stmt(const DbStmt& stmt, ProgrammableButtonDto* item) const
{
    item->id = ProgrammableButtonId(stmt.integer("id"));
    item->profile_id = ProfileId(stmt.integer("profile_id"));
    item->title = stmt.text("title");
    item->icon_string = stmt.text("icon_string");
    item->action_type = ProgrammableButtonActionType(stmt.integer("action_type"));
    item->action_string = stmt.text("action_string");
    item->keyboard_shortcut = stmt.text("keyboard_shortcut");
}

Error
ProfilesDb::get_programmable_buttons(GetProgrammableButtonsQuery query, std::vector<ProgrammableButtonDto>* out_items)
{
    RAISE_CHECK(out_items);

    std::vector<std::string> and_where_clauses{};
    std::vector<DbVariant> args{};

    if(query.profile_id.value != 0)
    {
        and_where_clauses.push_back("profile_id = ?");
        args.push_back(query.profile_id.value);
    }

    if(query.ids.has_value())
    {
        auto& ids = query.ids.value();
        std::ostringstream json_array_builder;
        json_array_builder << "[";
        for(size_t i = 0; i<ids.size(); ++i)
        {
            if (i>0) json_array_builder << ",";
            json_array_builder << ids.at(i).value;
        }
        json_array_builder << "]";
        std::string json_array = json_array_builder.str();

        args.push_back(json_array);
        and_where_clauses.push_back("id IN (SELECT value FROM json_each(?))");
    }

    std::string where_clause = "";
    if (!and_where_clauses.empty()) {
        where_clause = "WHERE " + Db::join_strings(and_where_clauses, " AND ");
    }
    std::string sql = std::format(R"(
            SELECT {} FROM programmable_buttons
            {};
        )", Db::join_strings(get_programmable_button_columns(), ", "),
            where_clause);
    
    DbStmt stmt;
    VERIFY(m_db->query(sql, args, &stmt));

    while (stmt.next()) {
        ProgrammableButtonDto item{};
        load_programmable_button_from_stmt(stmt, &item);
        out_items->push_back(item);
    }
    VERIFY(stmt.error());

    SUCCEED();
}

Error
ProfilesDb::create_programmable_button(
    CreateProgrammableButtonQuery query, ProgrammableButtonId* out_id, ProgrammableButtonsChange* change)
{
    if(!query.id.value)
    {
        RAISE(ErrorCode::argument, "id is not set");
    }
    if(!query.profile_id.value)
    {
        RAISE(ErrorCode::argument, "profile_id is not set");
    }
    std::string sql = std::format(R"(
        INSERT INTO programmable_buttons ({})
        VALUES ({})
        RETURNING {};
    )", Db::join_strings(get_programmable_button_columns(), ", "),
        Db::make_placeholders(get_programmable_button_columns().size()),
        Db::join_strings(get_programmable_button_columns(), ", ")
    );

    std::vector<DbVariant> args = {
        query.id.value,
        query.profile_id.value,
        query.title,
        query.icon_string,
        int64_t(query.action_type),
        query.action_string,
        query.keyboard_shortcut,
    };

    DbStmt stmt;
    VERIFY(m_db->query(sql, args, &stmt));

    bool has_row = stmt.next();
    VERIFY(stmt.error());
    RAISE_CHECK(has_row);

    ProgrammableButtonDto dto{};
    load_programmable_button_from_stmt(stmt, &dto);

    if (out_id) {
        *out_id = dto.id.value();
    }

    if (change) {
        change->created.push_back(dto);
    }
    SUCCEED();
}

Error
ProfilesDb::delete_programmable_buttons(
    const std::vector<ProgrammableButtonId>& ids, ProgrammableButtonsChange* change)
{
    if (ids.empty()) {
        SUCCEED(); // nothing to delete
    }

    std::ostringstream json_array_builder;
    json_array_builder << "[";
    for(size_t i = 0; i<ids.size(); ++i)
    {
        if (i>0) json_array_builder << ",";
        json_array_builder << ids.at(i).value;
    }
    json_array_builder << "]";
    std::string json_array = json_array_builder.str();

    std::string sql = R"(
        DELETE FROM programmable_buttons
        WHERE id IN (SELECT value FROM json_each(?1))
        RETURNING id;
    )";

    std::vector<DbVariant> args = {json_array};

    DbStmt stmt{};
    VERIFY(m_db->query(sql, args, &stmt));

    while(stmt.next())
    {
        if(change) {
            change->deleted.push_back(ProgrammableButtonId(stmt.integer("id")));
        }
    }
    VERIFY(stmt.error());
    SUCCEED();
}

Error
ProfilesDb::update_programmable_button(
    UpdateProgrammableButtonQuery query, TxMode tx_mode, ProgrammableButtonsChange* change)
{
    if (query.id.value == 0) {
        RAISE(ErrorCode::argument, "id is not set");
    }

    DbTx tx;
    if (tx_mode == TxMode::use_tx) {
        VERIFY(tx.begin(m_db));
    }

    std::vector<std::string> set_clauses;
    std::vector<DbVariant> args;

    if (query.update_title) {
        set_clauses.push_back("title = ?");
        args.push_back(query.title);
    }

    if (query.update_icon_string) {
        set_clauses.push_back("icon_string = ?");
        args.push_back(query.icon_string);
    }

    if (query.update_action_type) {
        set_clauses.push_back("action_type = ?");
        args.push_back(static_cast<int64_t>(query.action_type));
    }

    if (query.update_action_string) {
        set_clauses.push_back("action_string = ?");
        args.push_back(query.action_string);
    }

    if (query.update_keyboard_shortcut) {
        set_clauses.push_back("keyboard_shortcut = ?");
        args.push_back(query.keyboard_shortcut);
    }

    if (set_clauses.empty()) {
        SUCCEED(); // nothing to update
    }

    {
        std::string sql = std::format(R"(
            UPDATE programmable_buttons
            SET {}
            WHERE id = ?
            RETURNING {};
        )", Db::join_strings(set_clauses, ", "),
            Db::join_strings(get_programmable_button_columns(), ", ")
        );

        args.push_back(query.id.value);

        DbStmt stmt;
        VERIFY(m_db->query(sql, args, &stmt));

        bool has_row = stmt.next();
        VERIFY(stmt.error());
        RAISE_CHECK(has_row);

        if(change)
        {
            load_programmable_button_from_stmt(stmt, &change->updated.emplace_back());
        }
    }

    if (tx_mode == TxMode::use_tx) {
        VERIFY(tx.commit());
    }
    
    SUCCEED();
}

} // namespace orion