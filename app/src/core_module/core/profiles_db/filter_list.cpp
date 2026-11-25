#include <core/profiles_db/profiles_db.h>
#include <format>
#include <chrono>

namespace orion {

//-------------------------------------------------------------------------
// Filter Lists
//-------------------------------------------------------------------------

std::vector<std::string>
ProfilesDb::get_filter_list_columns() const
{
    return {
        "id",
        "profile_id",
        "name",
        "version",
        "position",
        "url",
        "description",
        "display_name",
        "is_enabled",
        "updated_at",
        "active_rules_count",
        "filter_type",
        "custom_filter_data",
        "content_hash",
    };
}

void
ProfilesDb::load_filter_list_from_stmt(const DbStmt& stmt, FilterListDto* filter_list) const
{
    filter_list->id = FilterListId(stmt.integer("id"));
    filter_list->profile_id = ProfileId(stmt.integer("profile_id"));
    filter_list->name = stmt.text("name");
    filter_list->version = stmt.text("version");
    filter_list->position = stmt.integer("position");
    filter_list->url = stmt.text("url");
    filter_list->description = stmt.text("description");
    filter_list->display_name = stmt.text("display_name");
    filter_list->is_enabled = stmt.integer("is_enabled") != 0;
    filter_list->updated_at = static_cast<UnixTimeUtc64>(stmt.integer("updated_at"));
    filter_list->active_rules_count = stmt.integer("active_rules_count");
    filter_list->filter_type = static_cast<FilterListType>(stmt.integer("filter_type"));
    filter_list->custom_filter_data = stmt.text("custom_filter_data");
    filter_list->content_hash = stmt.text("content_hash");
}

Error
ProfilesDb::create_or_update_filter_list(
    CreateFilterListQuery query, FilterListId* out_id, FilterListsChange* change)
{
    if (query.profile_id == ProfileId(0)) {
        RAISE(ErrorCode::argument, "profile_id is not set");
    }
    if (query.name.empty()) {
        RAISE(ErrorCode::argument, "name is empty");
    }

    {
        // Check if a filter list with the same name already exists for the profile
        std::string sql = std::format(R"(
            SELECT id FROM filter_lists
            WHERE profile_id = ? AND name = ?;
        )");

        std::vector<DbVariant> args = {
            query.profile_id.value,
            query.name,
        };

        DbStmt stmt;
        VERIFY(m_db->query(sql, args, &stmt));
        bool has_row = stmt.next();
        VERIFY(stmt.error());
        if (has_row) {
            // If it exists, update its version and other fields
            FilterListId existing_id(stmt.integer("id"));
            std::vector<std::string> set_clauses{
                "position",
                "url",
                "display_name",
                "description",
                "filter_type",
                "custom_filter_data",
            };
            std::vector<DbVariant> update_args{
                query.position,
                query.url,
                query.display_name,
                query.description,
                static_cast<int64_t>(query.filter_type),
                query.custom_filter_data,
            };

            if (query.update_is_enabled) {
                set_clauses.push_back("is_enabled");
                update_args.push_back(query.is_enabled);
            }
            if (query.update_updated_at) {
                set_clauses.push_back("updated_at");
                update_args.push_back(static_cast<int64_t>(query.updated_at));
            }
            if (query.update_active_rules_count) {
                set_clauses.push_back("active_rules_count");
                update_args.push_back(query.active_rules_count);
            }
            if (query.update_version) {
                set_clauses.push_back("version");
                update_args.push_back(query.version);
            }
            if (query.update_content_hash) {
                set_clauses.push_back("content_hash");
                update_args.push_back(query.content_hash);
            }

            std::string update_sql = std::format(R"(
                UPDATE filter_lists
                SET {}
                WHERE id = ?
                RETURNING {};
            )", Db::join_into_set_clause(set_clauses),
                Db::join_strings(get_filter_list_columns(), ", "));
            update_args.push_back(existing_id.value);

            DbStmt update_stmt;
            VERIFY(m_db->query(update_sql, update_args, &update_stmt));
            bool updated = update_stmt.next();
            VERIFY(update_stmt.error());
            RAISE_CHECK(updated);

            FilterListDto filter_list{};
            load_filter_list_from_stmt(update_stmt, &filter_list);
            if (change) {
                change->updated.push_back(filter_list);
            }
            if (out_id) {
                *out_id = existing_id;
            }
            SUCCEED();
        }
    }

    std::string sql = std::format(R"(
        INSERT INTO filter_lists ({})
        VALUES ({})
        RETURNING {};
    )", Db::join_strings(get_filter_list_columns(), ", "),
        Db::make_placeholders(get_filter_list_columns().size()),
        Db::join_strings(get_filter_list_columns(), ", ")
    );

    std::vector<DbVariant> args{
        nullptr, // id will be auto-generated
        query.profile_id.value,
        query.name,
        query.version,
        query.position,
        query.url,
        query.description,
        query.display_name,
        query.is_enabled,
        static_cast<int64_t>(query.updated_at),
        query.active_rules_count,
        static_cast<int64_t>(query.filter_type),
        query.custom_filter_data,
        query.content_hash,
    };

    DbStmt stmt;
    VERIFY(m_db->query(sql, args, &stmt));

    bool has_row = stmt.next();
    VERIFY(stmt.error());
    RAISE_CHECK(has_row);

    FilterListDto filter_list{};
    load_filter_list_from_stmt(stmt, &filter_list);

    if (change) {
        change->created.push_back(filter_list);
    }
    if (out_id) {
        *out_id = filter_list.id;
    }
    SUCCEED();
}

Error
ProfilesDb::get_filter_lists(GetFilterListsQuery query, std::vector<FilterListDto>* out_filter_lists)
{
    RAISE_CHECK(out_filter_lists);
    RAISE_CHECK(query.profile_id != ProfileId(0));

    std::vector<DbVariant> args = { query.profile_id.value };

    std::string where_clause = "WHERE profile_id = ?";

    if (query.ignore_block_web_fonts) {
        where_clause += " AND filter_type != ?";
        args.push_back(static_cast<int64_t>(FilterListType::block_web_fonts));
    }

    std::string sql = std::format(R"(
        SELECT {} FROM filter_lists
        {}
        ORDER BY filter_type, position, name, version;
    )", Db::join_strings(get_filter_list_columns(), ", "), where_clause);

    DbStmt stmt;
    VERIFY(m_db->query(sql, args, &stmt));

    while (stmt.next()) {
        FilterListDto filter_list{};
        load_filter_list_from_stmt(stmt, &filter_list);
        out_filter_lists->push_back(filter_list);
    }
    VERIFY(stmt.error());
    SUCCEED();
}

Error
ProfilesDb::delete_filter_lists(const std::vector<FilterListId>& ids, FilterListsChange* change)
{
    if (ids.empty()) {
        orion::log_verbose("delete_filter_lists called with empty ids vector, nothing to delete");
        SUCCEED(); // nothing to delete
    }

    std::string sql = std::format(R"(
        DELETE FROM filter_lists
        WHERE id IN ({})
        RETURNING id;
    )", Db::make_placeholders(ids.size()));

    std::vector<DbVariant> args;
    for (const auto& id : ids) {
        args.emplace_back(id.value);
    }

    DbStmt stmt;
    VERIFY(m_db->query(sql, args, &stmt));
    while (stmt.next()) {
        if (change) {
            change->deleted.push_back(FilterListId(stmt.integer("id")));
        }
    }
    VERIFY(stmt.error());
    SUCCEED();
}

Error
ProfilesDb::delete_unknown_builtin_filter_lists(
    const std::vector<std::string>& accounted_names,
    FilterListsChange* change)
{
    std::string sql = "DELETE FROM filter_lists WHERE filter_type = ?";
    std::vector<DbVariant> args{
        static_cast<int64_t>(FilterListType::builtin),
    };

    if (!accounted_names.empty()) {
        sql += std::format(" AND name NOT IN ({})", Db::make_placeholders(accounted_names.size()));
        for (const std::string& name : accounted_names) {
            args.emplace_back(name);
        }
    }
    sql += std::format(" RETURNING {}", Db::join_strings(get_filter_list_columns(), ", "));
    sql += ";";

    DbStmt stmt;
    VERIFY(m_db->query(sql, args, &stmt));
    while (stmt.next()) {
        FilterListDto filter_list{};
        load_filter_list_from_stmt(stmt, &filter_list);
        if (change) {
            change->deleted.push_back(filter_list.id);
        }
    }
    VERIFY(stmt.error());
    SUCCEED();
}

} // namespace orion
