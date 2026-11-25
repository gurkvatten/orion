#include <core/profiles_db/profiles_db.h>
#include <format>

namespace orion {

//-------------------------------------------------------------------------
// Bookmark folders
//-------------------------------------------------------------------------

Error
ProfilesDb::get_bookmark_folder_ids(ProfileId profile_id, std::vector<BookmarkFolderId>* ids)
{
    if (profile_id == ProfileId(0)) {
        RAISE(ErrorCode::argument, "profile_id is not set");
    }

    DbStmt stmt;
    VERIFY(m_db->query(R"(
        SELECT id FROM bookmark_folders
        WHERE profile_id = ?1
        ORDER BY parent_folder_id, seq_num;
    )", {profile_id.value}, &stmt));

    while (stmt.next()) {
        ids->push_back(BookmarkFolderId(stmt.integer("id")));
    }
    VERIFY(stmt.error());
    SUCCEED();
}

Error
ProfilesDb::get_bookmark_folders(const std::vector<BookmarkFolderId>& ids, std::vector<BookmarkFolderDto>* folders)
{
    auto args = to_db_variant_vector(ids);
    std::string sql = std::format(R"(
        SELECT id, profile_id, name, is_internal, parent_folder_id, seq_num, type, is_expanded
        FROM bookmark_folders
        WHERE id IN ({})
        ORDER BY parent_folder_id, seq_num;
    )", Db::make_placeholders(ids.size()));

    DbStmt stmt;
    VERIFY(m_db->query(sql, args, &stmt));

    while (stmt.next()) {
        auto folder = BookmarkFolderDto{};
        folder.id = BookmarkFolderId(stmt.integer("id"));
        folder.profile_id = ProfileId(stmt.integer("profile_id"));
        folder.name = stmt.text("name");
        folder.is_internal = stmt.integer("is_internal");
        if (!stmt.is_null("parent_folder_id")) {
            folder.parent_folder_id = BookmarkFolderId(stmt.integer("parent_folder_id"));
        }
        folder.seq_num = stmt.real("seq_num");
        folder.type = static_cast<BookmarkFolderType>(stmt.integer("type"));
        folder.is_expanded = stmt.integer("is_expanded");

        folders->emplace_back(folder);
    }
    VERIFY(stmt.error());
    SUCCEED();
}

Error
ProfilesDb::get_bookmark_folder_id_by_type(ProfileId profile_id, BookmarkFolderType type, BookmarkFolderId* folder_id)
{
    if (profile_id == ProfileId(0)) {
        RAISE(ErrorCode::argument, "profile_id is not set");
    }

    std::vector<DbVariant> args = {
        profile_id.value,
        static_cast<int64_t>(type),
    };

    DbStmt stmt;
    VERIFY(m_db->query(R"(
        SELECT id FROM bookmark_folders
        WHERE profile_id = ?1 AND type = ?2;
    )", args, &stmt));

    bool has_row = stmt.next();
    VERIFY(stmt.error());

    if (has_row) {
        *folder_id = BookmarkFolderId(stmt.integer("id"));
    } else {
        RAISE(ErrorCode::not_found, "folder of type %lld not found in profile %lld",
            type,
            profile_id.value
        );
    }
    SUCCEED();
}

Error
ProfilesDb::get_bookmark_folder_children(
    ProfileId profile_id, const BookmarkFolderId* folder_id, std::vector<BookmarkItem>* children)
{
    RAISE_CHECK(children);

    std::vector<DbVariant> args{};
    std::string condition{};

    if (folder_id) {
        args.push_back(profile_id.value);
        args.push_back(folder_id->value);
        condition = "profile_id = ?1 AND parent_folder_id = ?2";
    } else {
        args.push_back(profile_id.value);
        // root level folders and bookmarks have NULL parent_folder_id in the database
        condition = "profile_id = ?1 AND parent_folder_id IS NULL";
    }

    std::string sql = std::format(R"(
        SELECT id, 'folder' AS type, parent_folder_id, seq_num FROM bookmark_folders
        WHERE {}
        UNION ALL
        SELECT id, 'bookmark' AS type, parent_folder_id, seq_num FROM bookmarks
        WHERE {}
        ORDER BY seq_num;
    )", condition, condition);

    DbStmt stmt{};
    VERIFY(m_db->query(sql, args, &stmt));

    while (stmt.next()) {
        BookmarkItem item{};
        item.id = stmt.integer("id");
        std::string type_str = stmt.text("type");
        if (type_str == "folder") {
            item.type = BookmarkItemType::folder;
        } else if (type_str == "bookmark") {
            item.type = BookmarkItemType::bookmark;
        } else {
            RAISE(ErrorCode::invalid_state, "unexpected bookmark item type: %s", type_str.c_str());
        }
        if (!stmt.is_null("parent_folder_id")) {
            item.parent_folder_id = BookmarkFolderId(stmt.integer("parent_folder_id"));
        }
        item.seq_num = stmt.real("seq_num");

        children->emplace_back(item);
    }
    VERIFY(stmt.error());
    SUCCEED();
}

Error
ProfilesDb::refine_bookmark_seq_num(RefineBookmarkSeqNumQuery query, double* seq_num)
{
    RAISE_CHECK(seq_num);

    BookmarkSeqNumExistsReply exists_reply{};
    {
        BookmarkSeqNumExistsQuery exists_query{
            .id = query.existing_id,
            .profile_id = query.profile_id,
            .parent_folder_id = query.parent_folder_id,
            .seq_num = query.seq_num,
        };
        VERIFY(bookmark_seq_num_exists(exists_query, &exists_reply));
        if (!exists_reply.exists) {
            // nothing to refine, we can use the seq_num as is
            *seq_num = exists_reply.seq_num;
            SUCCEED();
        }
    }

    // selects 3 columns:
    // 1. id_exists - whether the seq_num exists in bookmark folders or bookmarks
    // 2. lower_seq_num - sequence number of existing item that directly precedes seq_num (NULL if no such item exists)
    // 3. higher_seq_num - sequence number of existing item that directly follows seq_num (NULL if no such item exists)
    std::string sql = R"(
        WITH combined AS (
            SELECT seq_num FROM bookmark_folders
            WHERE profile_id = ?1 AND parent_folder_id IS ?2
            UNION ALL
            SELECT seq_num FROM bookmarks
            WHERE profile_id = ?1 AND parent_folder_id IS ?2
        )
        SELECT
            EXISTS(
                SELECT 1 FROM combined WHERE seq_num = ?3
            ) AS id_exists,
            (
                SELECT MAX(seq_num) FROM combined WHERE seq_num < ?3
            ) AS lower_seq_num,
            (
                SELECT MIN(seq_num) FROM combined WHERE seq_num > ?3
            ) AS higher_seq_num;
    )";

    DbVariant parent_folder_id_variant;
    if (exists_reply.parent_folder_id.value) {
        parent_folder_id_variant = exists_reply.parent_folder_id.value;
    }

    std::vector<DbVariant> args = {
        exists_reply.profile_id.value,
        parent_folder_id_variant,
        exists_reply.seq_num,
    };

    DbStmt stmt;
    VERIFY(m_db->query(sql, args, &stmt));
    bool has_row = stmt.next();
    VERIFY(stmt.error());
    if (!has_row) {
        RAISE(ErrorCode::invalid_state, "unexpected empty result set when refining seq_num");
    }
    bool id_exists = stmt.integer("id_exists") != 0;
    std::optional<double> lower_seq_num = stmt.real_optional("lower_seq_num");
    std::optional<double> higher_seq_num = stmt.real_optional("higher_seq_num");

    if (!id_exists) {
        // seq_num is not used by any bookmark or folder, so it can be used as is
        *seq_num = exists_reply.seq_num;
        SUCCEED();
    }

    if (query.insert_mode == BookmarkInsertMode::not_set) {
        query.insert_mode = BookmarkInsertMode::after; // default insert mode
    }

    if (query.insert_mode == BookmarkInsertMode::before) {
        if (lower_seq_num.has_value()) {
            // set seq_num to midpoint between the existing seq_num and the one before it
            *seq_num = average(exists_reply.seq_num, lower_seq_num.value());
        } else {
            // no items before the existing one
            *seq_num = exists_reply.seq_num - 1.0;
        }
    } else if (query.insert_mode == BookmarkInsertMode::after) {
        if (higher_seq_num.has_value()) {
            // set seq_num to midpoint between the existing seq_num and the one after it
            *seq_num = average(exists_reply.seq_num, higher_seq_num.value());
        } else {
            // no items after the existing one
            *seq_num = exists_reply.seq_num + 1.0;
        }
    } else {
        RAISE(ErrorCode::argument, "invalid insert mode: %d", static_cast<int>(query.insert_mode));
    }
    SUCCEED();
}

// finds the next sequence number for the bookmark folder (takes into account both bookmark folders and bookmarks)
Error
ProfilesDb::find_next_bookmark_seq_num(ProfileId profile_id, const DbVariant& parent_folder_id, double* next_seq_num)
{
    RAISE_CHECK(next_seq_num);
    
    std::string sql = R"(
        SELECT IFNULL(MAX(seq_num) + 1, 0.0) AS next_seq_num
        FROM (
            SELECT seq_num FROM bookmark_folders
                WHERE profile_id = ?1 AND parent_folder_id IS ?2
            UNION ALL
            SELECT seq_num FROM bookmarks
                WHERE profile_id = ?1 AND parent_folder_id IS ?2
        );
    )";
    std::vector<DbVariant> args = {
        profile_id.value,
        parent_folder_id,
    };
    VERIFY(m_db->query_real(sql, args, next_seq_num));
    SUCCEED();
}

Error
ProfilesDb::bookmark_seq_num_exists(BookmarkSeqNumExistsQuery query, BookmarkSeqNumExistsReply* reply)
{
    RAISE_CHECK(reply);

    if (const BookmarkFolderId* folder_id = std::get_if<BookmarkFolderId>(&query.id)) {
        // find the bookmark folder by its id
        std::string sql = R"(
            SELECT seq_num, parent_folder_id, profile_id FROM bookmark_folders WHERE id = ?1;
        )";
        DbStmt stmt;
        VERIFY(m_db->query(sql, {folder_id->value}, &stmt));
        bool has_row = stmt.next();
        VERIFY(stmt.error());
        if (!has_row) {
            RAISE(ErrorCode::not_found, "bookmark folder with id %lld not found", folder_id->value);
        }
        if (!query.seq_num.has_value()) {
            query.seq_num = stmt.real("seq_num");
        }
        if (!query.parent_folder_id.has_value()) {
            query.parent_folder_id = stmt.variant("parent_folder_id");
        }
        if (query.profile_id == ProfileId(0)) {
            query.profile_id = ProfileId(stmt.integer("profile_id"));
        }
    } else if (const BookmarkId* bookmark_id = std::get_if<BookmarkId>(&query.id)) {
        // find the bookmark by its id
        std::string sql = R"(
            SELECT seq_num, parent_folder_id, profile_id FROM bookmarks WHERE id = ?1;
        )";
        DbStmt stmt;
        VERIFY(m_db->query(sql, {bookmark_id->value}, &stmt));
        bool has_row = stmt.next();
        VERIFY(stmt.error());
        if (!has_row) {
            RAISE(ErrorCode::not_found, "bookmark with id %lld not found", bookmark_id->value);
        }
        if (!query.seq_num.has_value()) {
            query.seq_num = stmt.real("seq_num");
        }
        if (!query.parent_folder_id.has_value()) {
            query.parent_folder_id = stmt.variant("parent_folder_id");
        }
        if (query.profile_id == ProfileId(0)) {
            query.profile_id = ProfileId(stmt.integer("profile_id"));
        }
    } else if (std::get_if<std::nullptr_t>(&query.id)) {
        RAISE_CHECK(query.seq_num.has_value());
        RAISE_CHECK(query.parent_folder_id.has_value());
    } else {
        RAISE(ErrorCode::argument, "invalid id type");
    }

    RAISE_CHECK(query.profile_id != ProfileId(0));
    RAISE_CHECK(query.seq_num.has_value());
    RAISE_CHECK(query.parent_folder_id.has_value());

    reply->profile_id = query.profile_id;
    reply->seq_num = query.seq_num.value();
    if (query.parent_folder_id->is_null()) {
        reply->parent_folder_id = BookmarkFolderId(0); // root level
    } else {
        reply->parent_folder_id = query.parent_folder_id->integer();
    }

    // now check if the seq_num exists in the bookmark folders or bookmarks
    std::string sql = R"(
        SELECT EXISTS(
            SELECT 1 FROM (
                SELECT seq_num FROM bookmark_folders
                    WHERE profile_id = ?1 AND parent_folder_id IS ?2 AND seq_num = ?3
                UNION ALL
                SELECT seq_num FROM bookmarks
                    WHERE profile_id = ?1 AND parent_folder_id IS ?2 AND seq_num = ?3
            )
        ) AS id_exists;
    )";
    std::vector<DbVariant> args = {
        query.profile_id.value,
        query.parent_folder_id.value(),
        query.seq_num.value(),
    };

    VERIFY(m_db->query_bool(sql, args, &reply->exists));
    SUCCEED();
}

Error
ProfilesDb::check_circular_reference_error(int result_code, const std::string& error_message) const
{
    if (result_code != SQLITE_CONSTRAINT) {
        orion::log_debug(
            "check_circular_reference_error: result_code=%d", result_code);
        SUCCEED();
    }

    if (error_message != "cannot set parent_folder_id to self" &&
        error_message != "circular reference with this parent_folder_id detected")
    {
        orion::log_debug(
            "check_circular_reference_error: error_message=%s", error_message.c_str());
        SUCCEED();
    }
    RAISE(ErrorCode::circular_reference, "cannot set parent_folder_id to self or circular reference detected");
}

Error
ProfilesDb::create_bookmark_folder_internal(
    CreateBookmarkFolderQuery query, bool is_internal, BookmarkFolderType type, BookmarkFolderId* folder_id,
    BookmarkFoldersChange* change)
{
    if (query.profile_id == ProfileId(0)) {
        RAISE(ErrorCode::argument, "profile_id is not set");
    }

    DbVariant parent_folder_id;
    if (query.parent_folder_id.value != 0) {
        parent_folder_id = query.parent_folder_id.value;
    }

    double seq_num = 0.0;
    if (query.seq_num.has_value()) {
        RefineBookmarkSeqNumQuery refine_query{
            .profile_id = query.profile_id,
            .existing_id = nullptr, // no existing id, we are creating a new folder
            .parent_folder_id = parent_folder_id,
            .insert_mode = query.insert_mode,
            .seq_num = query.seq_num.value(),
        };
        VERIFY(refine_bookmark_seq_num(refine_query, &seq_num));
        log_verbose("create_bookmark_folder_internal: refine_bookmark_folder_seq_num: old_seq_num=%f new_seq_num=%f",
            query.seq_num.value(), seq_num
        );
    } else {
        VERIFY(find_next_bookmark_seq_num(query.profile_id, parent_folder_id, &seq_num));
    }
    query.seq_num = seq_num;

    std::vector<DbVariant> args = {
        query.profile_id.value,
        query.name,
        is_internal,
        parent_folder_id,
        query.seq_num.value(),
        static_cast<int64_t>(type),
    };

    std::string sql = std::format(R"(
        INSERT INTO bookmark_folders (profile_id, name, is_internal, parent_folder_id, seq_num, type)
        VALUES ({})
        RETURNING id;
    )", Db::make_placeholders(args.size()));

    DbStmt stmt;
    VERIFY(m_db->query(sql, args, &stmt));

    int result_code = 0;
    std::string error_message;
    stmt.next(&result_code, &error_message);
    auto err = stmt.error();
    if (err) {
        VERIFY(check_circular_reference_error(result_code, error_message));
    }
    VERIFY(err);

    int64_t id = stmt.integer("id");
    if (folder_id) {
        *folder_id = BookmarkFolderId(id);
    }

    // This call can avoid "database table is locked" error in some cases
    VERIFY(stmt.finalize());

    BookmarkFolderDto folder{};
    folder.id = BookmarkFolderId(id);
    folder.profile_id = query.profile_id;
    folder.name = query.name;
    folder.is_internal = is_internal;
    folder.parent_folder_id = query.parent_folder_id;
    folder.seq_num = query.seq_num.value();
    folder.type = type;
    if (change) {
        change->created.emplace_back(folder);
    }
    SUCCEED();
}

Error
ProfilesDb::create_bookmark_folder(
    CreateBookmarkFolderQuery query, BookmarkFolderId* folder_id, BookmarkFoldersChange* change)
{
    return create_bookmark_folder_internal(query, false, BookmarkFolderType::normal, folder_id, change);
}

Error
ProfilesDb::update_bookmark_folder(UpdateBookmarkFolderQuery query, BookmarkFoldersChange* change)
{
    if (query.id == BookmarkFolderId(0)) {
        RAISE(ErrorCode::argument, "query.id is not set");
    }

    // check if the bookmark folder exists
    std::vector<BookmarkFolderDto> folders;
    VERIFY(get_bookmark_folders({query.id}, &folders));
    if (folders.empty()) {
        RAISE(ErrorCode::not_found, "bookmark folder with id %lld not found", query.id.value);
    }
    auto existing_folder = folders[0];

    std::vector<std::string> set_columns;
    std::vector<DbVariant> args;

    if (query.update_name) {
        args.push_back(query.name);
        set_columns.push_back("name");
    }

    if (query.update_parent_folder_id) {
        DbVariant parent_folder_id; // NULL by default
        if (query.parent_folder_id.value != 0) {
            parent_folder_id = query.parent_folder_id.value;
        }
        args.push_back(parent_folder_id);
        set_columns.push_back("parent_folder_id");

        if (!query.update_seq_num) {
            // we must set the seq_num anyway, because we are updating the parent folder id
            double next_seq_num = 0.0;
            VERIFY(find_next_bookmark_seq_num(existing_folder.profile_id, parent_folder_id, &next_seq_num));
            query.seq_num = next_seq_num;
            query.update_seq_num = true;
        }
    }

    if (query.update_seq_num) {
        // do not push back query.seq_num yet, we will will set it later after refining it
        set_columns.push_back("seq_num");
    }

    if (query.update_is_expanded) {
        args.push_back(query.is_expanded);
        set_columns.push_back("is_expanded");
    }

    if (set_columns.empty()) {
        SUCCEED(); // nothing to update
    }

    if (query.update_parent_folder_id || query.update_seq_num) {
        RefineBookmarkSeqNumQuery refine_query{
            .profile_id = existing_folder.profile_id,
            .existing_id = query.id,
            .parent_folder_id = query.parent_folder_id.value != 0 ? DbVariant(query.parent_folder_id.value) : DbVariant(nullptr),
            .insert_mode = query.insert_mode,
            .seq_num = query.seq_num,
        };

        if (query.update_parent_folder_id) {
            if (query.parent_folder_id.value != 0) {
                refine_query.parent_folder_id = DbVariant(query.parent_folder_id.value);
            } else {
                // We must not set the parent folder id to std::nullopt here because
                // we want to update it to null in the database.
                refine_query.parent_folder_id = DbVariant(nullptr);
            }
        }

        if (query.update_seq_num) {
            refine_query.seq_num = query.seq_num;
        } else {
            refine_query.seq_num = std::nullopt;
        }

        double seq_num = 0.0;
        VERIFY(refine_bookmark_seq_num(refine_query, &seq_num));
        log_verbose("update_bookmark_folder: refine_bookmark_folder_seq_num: old_seq_num=%f new_seq_num=%f",
            query.seq_num, seq_num
        );
        query.seq_num = seq_num;
    }

    if (query.update_seq_num) {
        // now we can push back the seq_num after refining it
        args.push_back(query.seq_num);
    }

    args.push_back(query.id.value);

    std::string sql = std::format(R"(
        UPDATE bookmark_folders SET {}
        WHERE id = ? AND is_internal = 0
        RETURNING id, profile_id, name, is_internal, parent_folder_id, seq_num, type, is_expanded;
    )", Db::join_into_set_clause(set_columns));

    DbStmt stmt;
    VERIFY(m_db->query(sql, args, &stmt));
    
    int result_code = 0;
    std::string error_message;

    while (stmt.next(&result_code, &error_message)) {
        BookmarkFolderDto folder{};
        folder.id = BookmarkFolderId(stmt.integer("id"));
        folder.profile_id = ProfileId(stmt.integer("profile_id"));
        folder.name = stmt.text("name");
        folder.is_internal = stmt.integer("is_internal");
        if (!stmt.is_null("parent_folder_id")) {
            folder.parent_folder_id = BookmarkFolderId(stmt.integer("parent_folder_id"));
        }
        folder.seq_num = stmt.real("seq_num");
        folder.type = static_cast<BookmarkFolderType>(stmt.integer("type"));
        folder.is_expanded = stmt.integer("is_expanded");

        if (change) {
            change->updated.emplace_back(folder);
        }
    }

    if (stmt.error()) {
        VERIFY(check_circular_reference_error(result_code, error_message));
        // This log is useful just for debugging core API tests
        orion::log_warning("update_bookmark_folder: fail to update bookmark folder with id %lld", query.id.value);
    }

    VERIFY(stmt.error());
    SUCCEED();
}

Error
ProfilesDb::move_bookmark_folder(MoveBookmarkFolderQuery query, BookmarkFoldersChange* change)
{
    if (query.folder_id == BookmarkFolderId(0)) {
        RAISE(ErrorCode::argument, "folder_id is not set");
    }

    std::vector<BookmarkFolderDto> folders;
    VERIFY(get_bookmark_folders({query.folder_id}, &folders));
    if (folders.empty()) {
        RAISE(ErrorCode::not_found, "bookmark folder with id %lld not found", query.folder_id.value);
    }
    ProfileId profile_id = folders[0].profile_id;

    DbVariant parent_folder_id_variant;
    if (query.target_parent_folder_id.value != 0) {
        parent_folder_id_variant = query.target_parent_folder_id.value;
    }

    double new_seq_num = 0;
    if (query.next_bookmark_id == BookmarkId(0) && query.next_folder_id == BookmarkFolderId(0)) {
        VERIFY(find_next_bookmark_seq_num(profile_id, parent_folder_id_variant, &new_seq_num));
    } else {
        std::string next_sql;
        std::vector<DbVariant> next_args;
        if (query.next_bookmark_id != BookmarkId(0)) {
            next_sql = "SELECT seq_num FROM bookmarks WHERE id = ?1";
            next_args = {query.next_bookmark_id.value};
        } else {
            next_sql = "SELECT seq_num FROM bookmark_folders WHERE id = ?1";
            next_args = {query.next_folder_id.value};
        }

        DbStmt next_stmt;
        VERIFY(m_db->query(next_sql, next_args, &next_stmt));
        if (!next_stmt.next()) {
            RAISE(ErrorCode::not_found, "next item not found");
        }
        double next_seq_num = next_stmt.real("seq_num");

        std::string prev_sql = R"(
            SELECT MAX(seq_num) as prev FROM (
                SELECT seq_num FROM bookmarks
                WHERE profile_id = ?1 AND parent_folder_id IS ?2 AND seq_num < ?3
                UNION ALL
                SELECT seq_num FROM bookmark_folders
                WHERE profile_id = ?1 AND parent_folder_id IS ?2 AND seq_num < ?3
            )
        )";
        std::vector<DbVariant> prev_args = {profile_id.value, parent_folder_id_variant, next_seq_num};
        DbStmt prev_stmt;
        VERIFY(m_db->query(prev_sql, prev_args, &prev_stmt));

        if (prev_stmt.next()) {
            auto prev_optional = prev_stmt.real_optional("prev");
            double prev_seq_num = prev_optional.value_or(next_seq_num - 8);
            new_seq_num = (prev_seq_num + next_seq_num) / 2.0;
        } else {
            new_seq_num = next_seq_num - 8;
        }
    }

    UpdateBookmarkFolderQuery update_query{
        .id = query.folder_id,
        .parent_folder_id = query.target_parent_folder_id,
        .seq_num = new_seq_num,
        .update_parent_folder_id = true,
        .update_seq_num = true,
    };
    VERIFY(update_bookmark_folder(update_query, change));
    SUCCEED();
}

Error
ProfilesDb::delete_bookmark_folders(
    const std::vector<BookmarkFolderId>& ids, BookmarkFoldersChange* change, BookmarksChange* bookmarks_change)
{
    auto args = to_db_variant_vector(ids);
    {
        std::string sql = std::format(R"(
            SELECT b.id FROM bookmarks b
            JOIN bookmark_folders f ON b.parent_folder_id = f.id 
            WHERE b.parent_folder_id IN ({}) AND f.is_internal = 0;
        )", Db::make_placeholders(ids.size()));

        DbStmt stmt;
        VERIFY(m_db->query(sql, args, &stmt));
        
        while (stmt.next()) {
            auto id = BookmarkId(stmt.integer("id"));
            if (bookmarks_change) {
                bookmarks_change->deleted.push_back(id);
            }
        }
        VERIFY(stmt.error());
    }

    std::string sql = std::format(R"(
        DELETE FROM bookmark_folders
        WHERE id IN ({}) AND is_internal = 0
        RETURNING id;
    )", Db::make_placeholders(ids.size()));

    DbStmt stmt;
    VERIFY(m_db->query(sql, args, &stmt));

    // For detection of invalid (non-existent or internal) input ids
    std::set<BookmarkFolderId> id_set(ids.begin(), ids.end());

    while (stmt.next()) {
        auto id = BookmarkFolderId(stmt.integer("id"));
        id_set.erase(id);

        if (change) {
            change->deleted.push_back(id);
        }
    }
    VERIFY(stmt.error());

    if (!id_set.empty()) {
        // Some ids were not found or were internal, so we raise an error
        log_warning("delete_bookmark_folders: fail to delete non-existent or internal bookmark folders with ids: %s", to_string(id_set).c_str());
    }
    SUCCEED();
}

// Note that it is needed to call build() on the hierarchy after all items are added
struct BookmarkHierarchy {
    BookmarkHierarchy() = default;

    Error
    add(const BookmarkItem& item)
    {
        auto key = BookmarkItemKey{item.parent_folder_id, item.seq_num};
        RAISE_IF(m_items.find(key) != m_items.end(), "duplicate bookmark item found");
        m_items[key] = item;

        if (item.type == BookmarkItemType::folder) {
            RAISE_IF(m_parent_map.find(BookmarkFolderId(item.id)) != m_parent_map.end(), "duplicate parent folder item found");
            // If the item is a folder, we need to track it in the parent map
            m_parent_map[static_cast<BookmarkFolderId>(item.id)] = key;
        }
        SUCCEED();
    }

    Error
    find(BookmarkItemKey key, BookmarkItem* item) const
    {
        auto it = m_items.find(key);
        RAISE_IF(it == m_items.end(), "bookmark item not found");
        *item = it->second;
        SUCCEED();
    }

    Error
    build()
    {
        VERIFY(fill_children());
        VERIFY(fill_roots());
        SUCCEED();
    }

    // Returns empty or stale vector if build() was not called before
    const std::vector<BookmarkItemKey>& roots() const {
        return m_roots;
    }

private:
    Error
    fill_children()
    {
        for (const auto& [key, item] : m_items) {
            if (key.parent_folder_id != BookmarkFolderId(0)) {
                // Find the parent item and add this item to its children
                if (m_parent_map.find(key.parent_folder_id) == m_parent_map.end()) {
                    // The parent folder is not contained in the hierarchy
                    continue;
                }
                auto parent = m_parent_map[key.parent_folder_id];
                m_items[parent].children.push_back(key);
            }
        }
        SUCCEED();
    }

    Error
    fill_roots()
    {
        for (const auto& [key, item] : m_items) {
            if (m_parent_map.find(key.parent_folder_id) == m_parent_map.end()) {
                m_roots.push_back(key);
            }
        }
        SUCCEED();
    }

private:
    std::map<BookmarkItemKey, BookmarkItem> m_items;
    std::map<BookmarkFolderId, BookmarkItemKey> m_parent_map;
    std::vector<BookmarkItemKey> m_roots;
};

Error
ProfilesDb::copy_bookmarks(
    CopyBookmarksQuery query, CopyBookmarksReply* reply, BookmarkFoldersChange* folder_change,
    BookmarksChange* bookmark_change)
{
    if (query.target_folder_id.value && query.target_bookmark_id.value) {
        RAISE(ErrorCode::argument, "either target_folder_id or target_bookmark_id must be set, not both");
    }

    BookmarkFolderDto target_folder{};

    // If the target is a bookmark, we find its parent folder and use it as the target folder.
    if (query.target_bookmark_id.value) {
        // check if the target bookmark exists
        std::vector<BookmarkDto> bookmarks;
        VERIFY(get_bookmarks(GetBookmarksQuery{.ids={query.target_bookmark_id}}, &bookmarks));
        if (bookmarks.empty()) {
            RAISE(ErrorCode::not_found, "target bookmark with id %lld not found", query.target_bookmark_id.value);
        }
        BookmarkDto target_bookmark = bookmarks[0];
        query.target_folder_id = target_bookmark.parent_folder_id;
    }

    if (query.target_folder_id.value) {
        // check if the target folder exists
        std::vector<BookmarkFolderDto> folders;
        VERIFY(get_bookmark_folders({query.target_folder_id}, &folders));
        if (folders.empty()) {
            RAISE(ErrorCode::not_found, "target folder with id %lld not found", query.target_folder_id.value);
        }
        target_folder = folders[0];
    } else {
        target_folder.id = BookmarkFolderId(0); // root level
    }

    std::set<BookmarkFolderId> folders_to_skip;

    // check folder existence and skip internal source folders
    {
        std::vector<BookmarkFolderDto> folders;
        std::set<BookmarkFolderId> unknown_folders(query.source_folder_ids.begin(), query.source_folder_ids.end());
        VERIFY(get_bookmark_folders({query.source_folder_ids}, &folders));
        for (const auto& folder : folders) {
            unknown_folders.erase(folder.id);
            if (folder.is_internal) {
                log_verbose("copy_bookmarks: skipping internal source folder %lld", folder.id.value);
                folders_to_skip.insert(folder.id);
            }
        }
        if (!unknown_folders.empty()) {
            RAISE(ErrorCode::not_found, "source folders not found: %s", to_string(unknown_folders).c_str());
        }
    }

    log_verbose("copy_bookmarks: source_folder_ids=%s source_bookmark_ids=%s target_folder_id=%s target_bookmark_id=%s",
        to_string(query.source_folder_ids).c_str(),
        to_string(query.source_bookmark_ids).c_str(),
        std::to_string(query.target_folder_id.value).c_str(),
        std::to_string(query.target_bookmark_id.value).c_str()
    );

    // 1a. prepare a map of source folder ids to their ascendant folders:
    //    - std::map<BookmarkFolderId, std::set<BookmarkFolderId>> folder_to_ascendants
    // 1b. prepare a map of source bookmark ids to their parent folder ids:
    //    - std::map<BookmarkId, BookmarkFolderId> bookmark_to_parent_folder
    // 2. skip each source folder that is descendant of any other source folder
    // 3. skip each source bookmark that is descendant of any source folder
    // 4. copy each source folder recursively to the target folder (appending to it)
    // 5. copy each source bookmark to the target folder (appending to it)

    std::set<BookmarkFolderId> source_folder_id_set; // to avoid processing the same source folder multiple times
    std::map<BookmarkId, BookmarkFolderId> bookmark_to_parent_folder;
    std::map<BookmarkFolderId, std::set<BookmarkFolderId>> folder_to_ascendants;
    std::set<BookmarkId> bookmarks_to_skip;
    
    {
        // 1a. prepare a map of source folder ids to their ascendant folders
        for (const auto& folder_id : query.source_folder_ids) {
            if (source_folder_id_set.contains(folder_id)) {
                continue; // already processed
            }
            std::set<BookmarkFolderId> ascendants;
            VERIFY(find_bookmark_folder_ascendants(folder_id, &ascendants));
            for (const auto& ascendant : ascendants) {
                folder_to_ascendants[folder_id].insert(ascendant);
            }
            source_folder_id_set.insert(folder_id);
        }
    }
    {
        // 1b. prepare a map of source bookmark ids to their parent folder ids
        std::vector<BookmarkDto> bookmarks;
        VERIFY(get_bookmarks(GetBookmarksQuery{.ids=query.source_bookmark_ids}, &bookmarks));
        for (const auto& bookmark : bookmarks) {
            bookmark_to_parent_folder[bookmark.id] = bookmark.parent_folder_id.value;
        }
    }
    {
        // 2. skip each source folder that is descendant of any other source folder
        for (const auto& [folder_id, ascendants] : folder_to_ascendants) {
            for (const auto& other_folder_id : query.source_folder_ids) {
                if (folder_id != other_folder_id && ascendants.contains(other_folder_id)) {
                    folders_to_skip.insert(folder_id);
                    break; // no need to check other ascendants
                }
            }
        }
        log_verbose("copy_bookmarks: folders_to_skip=%s", to_string(folders_to_skip).c_str());
    }
    {
        // 3. skip each source bookmark that is descendant of any source folder
        for (const auto& [folder_id, ascendants] : folder_to_ascendants) {
            for (const auto& bookmark_id : query.source_bookmark_ids) {
                RAISE_CHECK(bookmark_to_parent_folder.contains(bookmark_id));
                const auto& parent_folder_id = bookmark_to_parent_folder[bookmark_id];
                if (source_folder_id_set.contains(parent_folder_id) || ascendants.contains(parent_folder_id)) {
                    bookmarks_to_skip.insert(bookmark_id);
                    break; // no need to check other ascendants
                }
            }
        }
        log_verbose("copy_bookmarks: bookmarks_to_skip=%s", to_string(bookmarks_to_skip).c_str());
    }
    
    DbTx tx;
    VERIFY(m_db->begin(&tx));

    // 4. copy each source folder recursively to the target folder (appending to it)

    // We need to prepare snapshot of source folders and bookmarks (to avoid infinite recursion)

    std::map<BookmarkFolderId, BookmarkFolderId> folder_map; // maps source folder id to new folder id
    for (const auto& folder_id : query.source_folder_ids) {
        if (folders_to_skip.contains(folder_id)) {
            continue; // skip folders that are descendants of other source folders
        }

        BookmarkHierarchy hierarchy{};
        VERIFY(find_bookmark_folder_descendants(folder_id, &hierarchy));

        // Prepare a recursive function to walk through descendants
        std::function<Error(const BookmarkItem&, const std::function<Error(const BookmarkItem&)>&)> walk_descendants =
            [&walk_descendants, &hierarchy](const BookmarkItem& item, const std::function<Error(const BookmarkItem&)>& callback
        ) {
            VERIFY(callback(item));
            for (const auto& child_key : item.children) {
                BookmarkItem child{};
                VERIFY(hierarchy.find(child_key, &child));
                VERIFY(walk_descendants(child, callback));
            }
            SUCCEED();
        };

        for (const auto& root_key : hierarchy.roots()) {
            BookmarkItem root{};
            VERIFY(hierarchy.find(root_key, &root));
            VERIFY(walk_descendants(root, [](const BookmarkItem& item) {
                log_verbose("copy_bookmarks: item: id=%lld: type=%s parent_folder_id=%lld children=%zu",
                    item.id,
                    item.type == BookmarkItemType::folder ? "folder" : "bookmark",
                    item.parent_folder_id.value,
                    item.children.size()
                );
                SUCCEED();
            }));
        }

        for (const auto& root_key : hierarchy.roots()) {
            BookmarkItem root{};
            VERIFY(hierarchy.find(root_key, &root));
            VERIFY(walk_descendants(root, [&](const BookmarkItem& child) {
                VERIFY(copy_bookmark_folder_item(child, target_folder.id, folder_change, bookmark_change, &folder_map));
                SUCCEED();
            }));
        }
    }

    // 5. copy each source bookmark to the target folder (appending to it),
    // skip each bookmark that is a descendant of any source folder
    for (const auto& bookmark_id : query.source_bookmark_ids) {
        if (bookmarks_to_skip.contains(bookmark_id)) {
            continue; // skip bookmarks that are descendants of source folders
        }

        BookmarkItem item{};
        item.id = bookmark_id.value;
        item.type = BookmarkItemType::bookmark;
        // No need to set other BookmarkItem fields here
        
        VERIFY(copy_bookmark_folder_item(item, target_folder.id, folder_change, bookmark_change, &folder_map));
    }
    
    VERIFY(tx.commit());
    SUCCEED();
}

Error
ProfilesDb::copy_bookmark_folder_item(
    const BookmarkItem& descendant, BookmarkFolderId target_folder_id, BookmarkFoldersChange* folder_change,
    BookmarksChange* bookmark_change, std::map<BookmarkFolderId, BookmarkFolderId>* folder_map)
{
    if (descendant.type == BookmarkItemType::folder) {
        std::vector<BookmarkFolderDto> folders;
        VERIFY(get_bookmark_folders({BookmarkFolderId(descendant.id)}, &folders));
        RAISE_CHECK(folders.size() == 1, std::format("bookmark folder with id {} not found", descendant.id).c_str());
        auto folder = folders[0];

        CreateBookmarkFolderQuery create_query{};
        create_query.profile_id = folder.profile_id;
        create_query.name = folder.name; // the name can be suffixed later (e.g., " (copy)")
        // The target is always a folder, we append to it, leave seq_num empty
        create_query.seq_num = std::nullopt;
        create_query.insert_mode = BookmarkInsertMode::after; // always append to the target folder

        if (folder_map->find(folder.parent_folder_id) != folder_map->end()) {
            // parent folder already copied, use the existing id
            create_query.parent_folder_id = (*folder_map)[folder.parent_folder_id];
        } else {
            // the folder should be created as a child of the target folder directly
            create_query.parent_folder_id = target_folder_id;
        }

        BookmarkFolderId new_folder_id{};
        VERIFY(create_bookmark_folder(create_query, &new_folder_id, folder_change));
        (*folder_map)[folder.id] = new_folder_id;
    } else {
        // This is a bookmark
        std::vector<BookmarkDto> bookmarks;
        VERIFY(get_bookmarks(GetBookmarksQuery{.ids={BookmarkId(descendant.id)}}, &bookmarks));
        RAISE_CHECK(bookmarks.size() == 1, std::format("bookmark with id {} not found", descendant.id).c_str());
        auto bookmark = bookmarks[0];

        CreateBookmarkQuery create_query{};
        create_query.profile_id = bookmark.profile_id;
        create_query.name = bookmark.name; // the name can be suffixed later (e.g., " (copy)")
        create_query.address = bookmark.address;
        create_query.invocation_keyword = bookmark.invocation_keyword;
        create_query.seq_num = bookmark.seq_num;
        create_query.insert_mode = BookmarkInsertMode::after; // always append to the target folder

        if (folder_map->find(bookmark.parent_folder_id) != folder_map->end()) {
            // parent folder already copied, use the existing id
            create_query.parent_folder_id = (*folder_map)[bookmark.parent_folder_id];
        } else {
            // the bookmark should be created as a child of the target folder directly
            create_query.parent_folder_id = target_folder_id;
        }

        VERIFY(create_bookmark(create_query, nullptr, bookmark_change));
    }
    SUCCEED();
}

Error
ProfilesDb::find_bookmark_folder_ascendants(BookmarkFolderId folder_id, std::set<BookmarkFolderId>* ascendants)
{
    if (folder_id == BookmarkFolderId(0)) {
        RAISE(ErrorCode::argument, "folder_id is not set");
    }

    DbStmt stmt;
    VERIFY(m_db->query(R"(
        WITH RECURSIVE ascendants AS (
            SELECT id, parent_folder_id FROM bookmark_folders WHERE id = ?1
            UNION ALL
            SELECT bf.id, bf.parent_folder_id FROM bookmark_folders bf
            INNER JOIN ascendants a ON bf.id = a.parent_folder_id
        )
        SELECT id FROM ascendants;
    )", {folder_id.value}, &stmt));

    while (stmt.next()) {
        ascendants->insert(BookmarkFolderId(stmt.integer("id")));
    }
    VERIFY(stmt.error());
    SUCCEED();
}

Error
ProfilesDb::find_bookmark_folder_descendants(BookmarkFolderId folder_id, BookmarkHierarchy* hierarchy)
{
    if (folder_id == BookmarkFolderId(0)) {
        RAISE(ErrorCode::argument, "folder_id is not set");
    }

    DbStmt stmt;
    VERIFY(m_db->query(R"(
        WITH RECURSIVE folder_tree AS (
            SELECT id, 'folder' AS type, 0 AS prio, seq_num, parent_folder_id
            FROM bookmark_folders
            WHERE id = ?1  -- starting folder ID

            UNION ALL

            SELECT f.id, 'folder', 0 AS prio, f.seq_num, f.parent_folder_id
            FROM bookmark_folders f
            JOIN folder_tree ft ON f.parent_folder_id = ft.id
        ),
        all_items AS (
            SELECT id, type, 0 AS prio, seq_num, parent_folder_id FROM folder_tree
            UNION ALL
            SELECT b.id, 'bookmark', 1 AS prio, b.seq_num, b.parent_folder_id
            FROM bookmarks b
            JOIN folder_tree ft ON b.parent_folder_id = ft.id
        )
        SELECT id, type, prio, seq_num, parent_folder_id FROM all_items
        ORDER BY prio, parent_folder_id, seq_num;
    )", {folder_id.value}, &stmt));

    while (stmt.next()) {
        BookmarkItem item{};
        item.id = stmt.integer("id");
        if (stmt.text("type") == "folder") {
            item.type = BookmarkItemType::folder;
        } else {
            item.type = BookmarkItemType::bookmark;
        }
        if (stmt.is_null("parent_folder_id")) {
            item.parent_folder_id = BookmarkFolderId(0); // root level
        } else {
            item.parent_folder_id = BookmarkFolderId(stmt.integer("parent_folder_id"));
        }
        item.seq_num = stmt.real("seq_num");

        VERIFY(hierarchy->add(item));
    }
    VERIFY(stmt.error());
    VERIFY(hierarchy->build());
    SUCCEED();
}

std::string
ProfilesDb::to_string(const std::vector<int64_t>& ids) const
{
    std::string result;
    for (const auto& id : ids) {
        if (!result.empty()) {
            result += " ";
        }
        result += std::to_string(id);
    }
    return "[" + result + "]";
}

std::string
ProfilesDb::to_string(const std::vector<TabId>& ids) const
{
    std::vector<int64_t> int_ids;
    for (const auto& id : ids) {
        int_ids.push_back(id.value);
    }
    return to_string(int_ids);
}

std::string
ProfilesDb::to_string(const std::vector<BookmarkFolderId>& ids) const
{
    std::vector<int64_t> int_ids;
    for (const auto& id : ids) {
        int_ids.push_back(id.value);
    }
    return to_string(int_ids);
}

std::string
ProfilesDb::to_string(const std::vector<BookmarkId>& ids) const
{
    std::vector<int64_t> int_ids;
    for (const auto& id : ids) {
        int_ids.push_back(id.value);
    }
    return to_string(int_ids);
}

std::string
ProfilesDb::to_string(const std::set<BookmarkFolderId>& ids) const
{
    std::vector<int64_t> int_ids;
    for (const auto& id : ids) {
        int_ids.push_back(id.value);
    }
    return to_string(int_ids);
}

std::string
ProfilesDb::to_string(const std::set<BookmarkId>& ids) const
{
    std::vector<int64_t> int_ids;
    for (const auto& id : ids) {
        int_ids.push_back(id.value);
    }
    return to_string(int_ids);
}

Error
ProfilesDb::find_unique_bookmark_folder_name(
    ProfileId profile_id,
    const std::string& folder_name_prefix,
    const BookmarkFolderId* parent_folder_id,
    std::string* out_unique_name)
{
    RAISE_CHECK(out_unique_name);
    std::string sql = R"(
        SELECT name FROM bookmark_folders
        WHERE profile_id = ?1 AND parent_folder_id IS ?2 AND name LIKE ?3
        ORDER BY seq_num;
    )";
    std::vector<DbVariant> args{
        profile_id.value,
        parent_folder_id ? DbVariant(parent_folder_id->value) : DbVariant(nullptr),
        Db::escape_like(folder_name_prefix, '\\') + "%",
    };
    DbStmt stmt{};
    VERIFY(m_db->query(sql, args, &stmt));
    std::set<std::string> existing_names;
    while (stmt.next()) {
        existing_names.insert(stmt.text("name"));
    }
    VERIFY(stmt.error());

    if (existing_names.empty()) {
        *out_unique_name = folder_name_prefix;
        SUCCEED();
    }

    for (int suffix = 2; suffix < 1000; suffix++) {
        std::string candidate_name = std::format("{} ({})", folder_name_prefix, suffix);
        if (!existing_names.contains(candidate_name)) {
            *out_unique_name = candidate_name;
            SUCCEED();
        }
    }

    RAISE(
        ErrorCode::not_found,
        "cannot find unique folder name with prefix '%s' for profile_id %lld and parent_folder_id %s",
        folder_name_prefix.c_str(),
        profile_id.value,
        parent_folder_id ? std::to_string(parent_folder_id->value).c_str() : "NULL"
    );
}

} // namespace orion