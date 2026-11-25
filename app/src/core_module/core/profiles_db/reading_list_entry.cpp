#include <core/profiles_db/profiles_db.h>
#include <format>

namespace orion {

//-------------------------------------------------------------------------
// Reading list entries
//-------------------------------------------------------------------------

Error
ProfilesDb::get_reading_list_entry_ids(GetReadingListEntryIdsQuery query, std::vector<ReadingListEntryId>* ids)
{
    if (query.profile_id == ProfileId(0)) {
        RAISE(ErrorCode::argument, "profile_id is not set");
    }

    std::vector<DbVariant> args = {
        query.profile_id.value,
        query.added_after.value,
        query.added_before.value,
    };
    std::vector<std::string> like_clauses;

    if (!query.search_filter.empty()) {
        like_clauses.push_back("title LIKE '%' || ? || '%' ESCAPE '\\'");
        args.push_back(Db::escape_like(query.search_filter, '\\'));

        like_clauses.push_back("address LIKE '%' || ? || '%' ESCAPE '\\'");
        args.push_back(Db::escape_like(query.search_filter, '\\'));

        like_clauses.push_back("description LIKE '%' || ? || '%' ESCAPE '\\'");
        args.push_back(Db::escape_like(query.search_filter, '\\'));
    }

    std::string where_clause = "WHERE profile_id = ?1 AND added_on >= ?2 AND added_on < ?3";

    if (query.address.has_value()) {
        where_clause += " AND address = ?4";
        args.push_back(query.address.value());
    }

    if (!like_clauses.empty()) {
        where_clause += std::format(" AND ({})", Db::join_strings(like_clauses, " OR "));
    }

    std::string sql = std::format(R"(
        SELECT id FROM reading_list_entries
            {}
            ORDER BY added_on DESC;
    )", where_clause);

    DbStmt stmt;
    VERIFY(m_db->query(sql, args, &stmt));

    while (stmt.next()) {
        ids->push_back(ReadingListEntryId(stmt.integer("id")));
    }
    VERIFY(stmt.error());
    SUCCEED();
}

Error
ProfilesDb::get_reading_list_entries(
    const std::vector<ReadingListEntryId>& ids, std::vector<ReadingListEntryDto>* entries)
{
    auto args = to_db_variant_vector(ids);
    std::string sql = std::format(R"(
        SELECT id, profile_id, added_on, icon_name, title, address, referrer, is_unread, is_saved_offline, description
        FROM reading_list_entries
        WHERE id IN ({})
        ORDER BY added_on DESC;
    )", Db::make_placeholders(ids.size()));

    DbStmt stmt;
    VERIFY(m_db->query(sql, args, &stmt));

    while (stmt.next()) {
        auto entry = ReadingListEntryDto{};
        entry.id = ReadingListEntryId(stmt.integer("id"));
        entry.profile_id = ProfileId(stmt.integer("profile_id"));
        entry.added_on = UnixTimeUtc64(stmt.integer("added_on"));
        entry.icon_name = stmt.text("icon_name");
        entry.title = stmt.text("title");
        entry.address = stmt.text("address");
        entry.referrer = stmt.text("referrer");
        entry.is_unread = stmt.integer("is_unread");
        entry.is_saved_offline = stmt.integer("is_saved_offline");
        entry.description = stmt.text("description");
        entries->emplace_back(entry);
    }
    VERIFY(stmt.error());
    SUCCEED();
}

Error
ProfilesDb::create_reading_list_entry(
    CreateReadingListEntryQuery query, ReadingListEntryId* entry_id, ReadingListEntriesChange* change)
{
    if (query.profile_id == ProfileId(0)) {
        RAISE(ErrorCode::argument, "profile_id is not set");
    }

    {
        // Try to find an existing entry with the same address
        std::vector<ReadingListEntryId> existing_ids;
        GetReadingListEntryIdsQuery get_query{
            .profile_id = query.profile_id,
            .address = query.address,
        };
        VERIFY(get_reading_list_entry_ids(get_query, &existing_ids));
        if (!existing_ids.empty()) {
            auto existing_id = existing_ids.front();
            std::vector<ReadingListEntryDto> existing_entries;
            VERIFY(get_reading_list_entries({existing_id}, &existing_entries));

            if (!existing_entries.empty()) {
                auto& existing_entry = existing_entries.front();
                if (existing_entry.is_unread) {
                    log_debug("%s: nothing to do, entry already exists in unread state", __func__);
                    SUCCEED(); // nothing to do, entry already exists in unread state
                } else {
                    // Update the existing entry instead of creating a new one
                    UpdateReadingListEntryQuery update_query{
                        .id = existing_id,
                        .added_on = query.added_on,
                        .icon_name = query.icon_name,
                        .title = query.title,
                        .referrer = query.referrer,
                        .is_unread = true,
                        .is_saved_offline = false,
                        .description = query.description,
                        .update_added_on = true,
                        .update_icon_name = true,
                        .update_title = true,
                        .update_referrer = true,
                        .update_is_unread = true,
                        .update_is_saved_offline = true,
                        .update_description = true,
                    };
                    VERIFY(update_reading_list_entry(update_query, change));
                    if (entry_id) {
                        *entry_id = update_query.id;
                    }
                    log_debug("%s: updated existing entry to unread state", __func__);
                    SUCCEED();
                }
            }
        }
    }

    DbStmt stmt;
    VERIFY(m_db->query(R"(
        INSERT INTO reading_list_entries (
            profile_id, added_on, icon_name, title, address, referrer, description
        ) VALUES (?, ?, ?, ?, ?, ?, ?)
        RETURNING id, is_unread, is_saved_offline;
    )", {
        query.profile_id.value,
        query.added_on.value,
        query.icon_name,
        query.title,
        query.address,
        query.referrer,
        query.description,
    }, &stmt));

    bool has_row = stmt.next();
    VERIFY(stmt.error());
    RAISE_CHECK(has_row);

    ReadingListEntryDto entry{};
    entry.id = ReadingListEntryId(stmt.integer("id"));
    entry.profile_id = query.profile_id;
    entry.added_on = query.added_on;
    entry.icon_name = query.icon_name;
    entry.title = query.title;
    entry.address = query.address;
    entry.referrer = query.referrer;
    entry.description = query.description;
    entry.is_unread = stmt.integer("is_unread");
    entry.is_saved_offline = stmt.integer("is_saved_offline");
    if (change) {
        change->created.emplace_back(entry);
    }

    if (entry_id) {
        *entry_id = entry.id;
    }
    SUCCEED();
}

Error
ProfilesDb::update_reading_list_entry(UpdateReadingListEntryQuery query, ReadingListEntriesChange* change)
{
    if (query.id == ReadingListEntryId(0)) {
        RAISE(ErrorCode::argument, "reading list entry id is not set");
    }

    std::vector<std::string> set_columns;
    std::vector<DbVariant> args;
    
    if (query.update_profile_id) {
        if (query.profile_id == ProfileId(0)) {
            RAISE(ErrorCode::argument, "profile_id is not set");
        }
        args.push_back(query.profile_id.value);
        set_columns.push_back("profile_id");
    }

    if (query.update_added_on) {
        args.push_back(query.added_on.value);
        set_columns.push_back("added_on");
    }

    if (query.update_icon_name) {
        args.push_back(query.icon_name);
        set_columns.push_back("icon_name");
    }

    if (query.update_title) {
        args.push_back(query.title);
        set_columns.push_back("title");
    }

    // Address field cannot be updated to reduce complexity

    if (query.update_referrer) {
        args.push_back(query.referrer);
        set_columns.push_back("referrer");
    }

    if (query.update_is_unread) {
        args.push_back(query.is_unread);
        set_columns.push_back("is_unread");
    }

    if (query.update_is_saved_offline) {
        args.push_back(query.is_saved_offline);
        set_columns.push_back("is_saved_offline");
    }

    if (query.update_description) {
        args.push_back(query.description);
        set_columns.push_back("description");
    }

    if (set_columns.empty()) {
        SUCCEED(); // nothing to update
    }

    args.push_back(query.id.value);

    std::string sql = std::format(R"(
        UPDATE reading_list_entries SET {}
        WHERE id = ?
        RETURNING id, profile_id, added_on, icon_name, title, address, referrer, is_unread, is_saved_offline, description;
    )", Db::join_into_set_clause(set_columns));

    DbStmt stmt;
    VERIFY(m_db->query(sql, args, &stmt));

    while (stmt.next()) {
        ReadingListEntryDto entry{};
        entry.id = ReadingListEntryId(stmt.integer("id"));
        entry.profile_id = ProfileId(stmt.integer("profile_id"));
        entry.added_on = UnixTimeUtc64(stmt.integer("added_on"));
        entry.icon_name = stmt.text("icon_name");
        entry.title = stmt.text("title");
        entry.address = stmt.text("address");
        entry.referrer = stmt.text("referrer");
        entry.is_unread = stmt.integer("is_unread");
        entry.is_saved_offline = stmt.integer("is_saved_offline");
        entry.description = stmt.text("description");

        if (change) {
            change->updated.emplace_back(entry);
        }
    }
    VERIFY(stmt.error());
    SUCCEED();
}

Error
ProfilesDb::delete_reading_list_entries(const std::vector<ReadingListEntryId>& ids, ReadingListEntriesChange* change)
{
    auto args = to_db_variant_vector(ids);
    std::string sql = std::format(R"(
        DELETE FROM reading_list_entries WHERE id IN ({})
        RETURNING id;
    )", Db::make_placeholders(ids.size()));

    DbStmt stmt;
    VERIFY(m_db->query(sql, args, &stmt));

    while (stmt.next()) {
        auto id = ReadingListEntryId(stmt.integer("id"));
        if (change) {
            change->deleted.push_back(id);
        }
    }
    VERIFY(stmt.error());
    SUCCEED();
}

} // namespace orion