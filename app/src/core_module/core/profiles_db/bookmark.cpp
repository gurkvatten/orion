#include <core/profiles_db/profiles_db.h>
#include <format>
#include <chrono>

namespace orion {

//-------------------------------------------------------------------------
// Bookmarks
//-------------------------------------------------------------------------

Error
ProfilesDb::get_bookmark_ids(GetBookmarkIdsQuery query, std::vector<BookmarkId>* ids)
{
    if (query.profile_id == ProfileId(0)) {
        RAISE(ErrorCode::argument, "profile_id is not set");
    }

    std::vector<DbVariant> args = {
        query.profile_id.value
    };

    std::vector<std::string> like_clauses;

    if (!query.search_filter.empty()) {
        like_clauses.push_back("name LIKE '%' || ? || '%' ESCAPE '\\'");
        args.push_back(Db::escape_like(query.search_filter, '\\'));

        like_clauses.push_back("address LIKE '%' || ? || '%' ESCAPE '\\'");
        args.push_back(Db::escape_like(query.search_filter, '\\'));
    }

    std::string where_clause = "WHERE profile_id = ?1";
    
    if (!like_clauses.empty()) {
        where_clause += std::format(" AND ({})", Db::join_strings(like_clauses, " OR "));
    }

    if (query.parent_folder_id.has_value()) {
        where_clause += " AND parent_folder_id = ?";
        args.push_back(query.parent_folder_id->value);
    }

    if (query.created_before.has_value()) {
        where_clause += " AND created_at < ?";
        args.push_back(query.created_before->value);
    }

    std::string sql = std::format(R"(
        SELECT id FROM bookmarks
        {}
        ORDER BY parent_folder_id, seq_num;
    )", where_clause);

    DbStmt stmt;
    VERIFY(m_db->query(sql, args, &stmt));

    while (stmt.next()) {
        ids->push_back(BookmarkId(stmt.integer("id")));
    }
    VERIFY(stmt.error());
    SUCCEED();
}

Error
ProfilesDb::get_recent_bookmark_ids(GetRecentBookmarkIdsQuery query, std::vector<BookmarkId>* ids)
{
    if (query.profile_id == ProfileId(0)) {
        RAISE(ErrorCode::argument, "profile_id is not set");
    }

    DbStmt stmt;
    VERIFY(m_db->query(R"(
        SELECT id FROM bookmarks
        WHERE profile_id = ?1
        ORDER BY created_at DESC
        LIMIT 10;
    )", {query.profile_id.value}, &stmt));

    while (stmt.next()) {
        ids->push_back(BookmarkId(stmt.integer("id")));
    }
    VERIFY(stmt.error());
    SUCCEED();
}

Error
ProfilesDb::get_bookmarks(GetBookmarksQuery query, std::vector<BookmarkDto>* bookmarks)
{
    auto args = to_db_variant_vector(query.ids);

    std::string order_by = "parent_folder_id, seq_num"; // default order
    if (query.order_by == BookmarkOrderByMode::created_at_desc) {
        order_by = "created_at DESC";
    }

    std::string sql = std::format(R"(
        SELECT id, profile_id, name, address, invocation_keyword, parent_folder_id, seq_num
        FROM bookmarks
        WHERE id IN ({})
        ORDER BY {};
    )", Db::make_placeholders(query.ids.size()),
        order_by
    );

    DbStmt stmt;
    VERIFY(m_db->query(sql, args, &stmt));

    while (stmt.next()) {
        auto bookmark = BookmarkDto{};
        bookmark.id = BookmarkId(stmt.integer("id"));
        bookmark.profile_id = ProfileId(stmt.integer("profile_id"));
        bookmark.name = stmt.text("name");
        bookmark.address = stmt.text("address");
        bookmark.invocation_keyword = stmt.text("invocation_keyword");
        if (stmt.is_null("parent_folder_id")) {
            bookmark.parent_folder_id = BookmarkFolderId(0); // root level
        } else {
            bookmark.parent_folder_id = BookmarkFolderId(stmt.integer("parent_folder_id"));
        }
        bookmark.seq_num = stmt.real("seq_num");
        bookmarks->emplace_back(bookmark);
    }
    VERIFY(stmt.error());
    SUCCEED();
}

Error
ProfilesDb::create_bookmark(CreateBookmarkQuery query, BookmarkId* bookmark_id, BookmarksChange* change)
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
            .existing_id = nullptr, // no existing id, we are creating a new bookmark
            .parent_folder_id = parent_folder_id,
            .insert_mode = query.insert_mode,
            .seq_num = query.seq_num.value(),
        };
        VERIFY(refine_bookmark_seq_num(refine_query, &seq_num));
        log_verbose("create_bookmark: refine_bookmark_seq_num: old_seq_num=%f new_seq_num=%f",
            query.seq_num.value(), seq_num
        );
    } else {
        VERIFY(find_next_bookmark_seq_num(query.profile_id, parent_folder_id, &seq_num));
    }
    query.seq_num = seq_num;

    auto created_at = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    std::vector<DbVariant> args = {
        query.profile_id.value,
        query.name,
        query.address,
        query.invocation_keyword,
        parent_folder_id,
        query.seq_num.value(),
        created_at,
    };

    std::string sql = std::format(R"(
        INSERT INTO bookmarks (profile_id, name, address, invocation_keyword, parent_folder_id, seq_num, created_at)
        VALUES ({})
        RETURNING id;
    )", Db::make_placeholders(args.size()));

    DbStmt stmt;
    VERIFY(m_db->query(sql, args, &stmt));

    bool has_row = stmt.next();
    VERIFY(stmt.error());
    RAISE_CHECK(has_row);

    int64_t id = stmt.integer("id");
    if (bookmark_id) {
        *bookmark_id = BookmarkId(id);
    }
    
    // this is crucial to be able to drop index on bookmarks table (temporarily) when
    // normalizing seq_num - otherwise we would get "database table is locked" error
    VERIFY(stmt.finalize());

    if (change) {
        BookmarkDto bookmark{};
        bookmark.id = BookmarkId(id);
        bookmark.profile_id = query.profile_id;
        bookmark.name = query.name;
        bookmark.address = query.address;
        bookmark.invocation_keyword = query.invocation_keyword;
        bookmark.parent_folder_id = query.parent_folder_id;
        bookmark.seq_num = query.seq_num.value();
        change->created.emplace_back(bookmark);
    }

    SUCCEED();
}

Error
ProfilesDb::update_bookmark(UpdateBookmarkQuery query, BookmarksChange* change)
{
    if (query.id == BookmarkId(0)) {
        RAISE(ErrorCode::argument, "query.id is not set");
    }

    // check if the bookmark exists
    std::vector<BookmarkDto> bookmarks;
    VERIFY(get_bookmarks(GetBookmarksQuery{.ids={query.id}}, &bookmarks));
    if (bookmarks.empty()) {
        RAISE(ErrorCode::not_found, "bookmark with id %lld not found", query.id.value);
    }
    auto existing_bookmark = bookmarks[0];

    std::vector<std::string> set_columns;
    std::vector<DbVariant> args;

    if (query.update_name) {
        args.push_back(query.name);
        set_columns.push_back("name");
    }

    if (query.update_address) {
        args.push_back(query.address);
        set_columns.push_back("address");
    }

    if (query.update_invocation_keyword) {
        args.push_back(query.invocation_keyword);
        set_columns.push_back("invocation_keyword");
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
            VERIFY(find_next_bookmark_seq_num(existing_bookmark.profile_id, parent_folder_id, &next_seq_num));
            query.seq_num = next_seq_num;
            query.update_seq_num = true;
        }
    }

    if (query.update_seq_num) {
        // do not push back query.seq_num yet, we will will set it later after refining it
        set_columns.push_back("seq_num");
    }

    if (set_columns.empty()) {
        SUCCEED(); // nothing to update
    }

    if (query.update_parent_folder_id || query.update_seq_num) {
        RefineBookmarkSeqNumQuery refine_query{
            .profile_id = existing_bookmark.profile_id,
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
        log_verbose("update_bookmark: refine_bookmark_seq_num: old_seq_num=%f new_seq_num=%f",
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
        UPDATE bookmarks SET {}
        WHERE id = ?
        RETURNING id, profile_id, name, address, invocation_keyword, parent_folder_id, seq_num;
    )", Db::join_into_set_clause(set_columns));

    DbStmt stmt;
    VERIFY(m_db->query(sql, args, &stmt));

    while (stmt.next()) {
        BookmarkDto bookmark{};
        bookmark.id = BookmarkId(stmt.integer("id"));
        bookmark.profile_id = ProfileId(stmt.integer("profile_id"));
        bookmark.name = stmt.text("name");
        bookmark.address = stmt.text("address");
        bookmark.invocation_keyword = stmt.text("invocation_keyword");
        if (!stmt.is_null("parent_folder_id")) {
            bookmark.parent_folder_id = BookmarkFolderId(stmt.integer("parent_folder_id"));
        }
        bookmark.seq_num = stmt.real("seq_num");
        if (change) {
            change->updated.emplace_back(bookmark);
        }
    }
    VERIFY(stmt.error());
    SUCCEED();
}

Error
ProfilesDb::move_bookmark(MoveBookmarkQuery query, BookmarksChange* change)
{
    if (query.bookmark_id == BookmarkId(0)) {
        RAISE(ErrorCode::argument, "bookmark_id is not set");
    }

    std::vector<BookmarkDto> bookmarks;
    VERIFY(get_bookmarks(GetBookmarksQuery{.ids={query.bookmark_id}}, &bookmarks));
    if (bookmarks.empty()) {
        RAISE(ErrorCode::not_found, "bookmark with id %lld not found", query.bookmark_id.value);
    }
    ProfileId profile_id = bookmarks[0].profile_id;

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

    UpdateBookmarkQuery update_query{
        .id = query.bookmark_id,
        .parent_folder_id = query.target_parent_folder_id,
        .seq_num = new_seq_num,
        .update_parent_folder_id = true,
        .update_seq_num = true,
    };
    VERIFY(update_bookmark(update_query, change));
    SUCCEED();
}

Error
ProfilesDb::delete_bookmarks(const std::vector<BookmarkId>& ids, BookmarksChange* change)
{
    auto args = to_db_variant_vector(ids);
    std::string sql = std::format(R"(
        DELETE FROM bookmarks WHERE id IN ({})
        RETURNING id;
    )", Db::make_placeholders(ids.size()));

    DbStmt stmt;
    VERIFY(m_db->query(sql, args, &stmt));

    while (stmt.next()) {
        auto id = BookmarkId(stmt.integer("id"));
        if (change) {
            change->deleted.push_back(id);
        }
    }
    VERIFY(stmt.error());
    SUCCEED();
}

Error
ProfilesDb::normalize_bookmark_seq_num(
    ProfileId profile_id, BookmarkFolderId parent_folder_id, BookmarksChange* bookmarks_change,
    BookmarkFoldersChange* folders_change)
{
    auto start = std::chrono::high_resolution_clock::now();

    if (profile_id == ProfileId(0)) {
        RAISE(ErrorCode::argument, "profile_id is not set");
    }

    DbTx tx;
    VERIFY(m_db->begin(&tx));

    // Note that we have to drop the index before updating the seq_num in bulk, because otherwise we could
    // violate the unique constraint on seq_num midway through the update. This cannot be solved easily by
    // using a temporary column, nor it can be solved by using a temporary table because the order of updates
    // of each record is not guaranteed (and cannot be ordered easily). Then after the update, we recreate the
    // dropped index - all this can be done atomically in a transaction.

    // Query the index SQL to be able to recreate it after the update
    std::string bookmark_index_sql;
    VERIFY(m_db->query_text(R"(
        SELECT sql FROM sqlite_master WHERE type = 'index' AND name = 'idx_unique_bookmark_seq_num_root';
    )", {}, &bookmark_index_sql));

    std::string folder_index_sql;
    VERIFY(m_db->query_text(R"(
        SELECT sql FROM sqlite_master WHERE type = 'index' AND name = 'idx_unique_bookmark_folder_seq_num_root';
    )", {}, &folder_index_sql));

    // Also note that all statements that happened before this transaction started must already be finalized,
    // otherwise we will get "database table is locked" error when trying to drop the index.
    VERIFY(m_db->exec("DROP INDEX idx_unique_bookmark_seq_num_root;"));
    VERIFY(m_db->exec("DROP INDEX idx_unique_bookmark_folder_seq_num_root;"));

    // Note that we need to use CTE to be able to use ROW_NUMBER(). Also, we cannot use RETURNING clause here
    // in combination with UPDATE because we cannot ORDER BY seq_num in the RETURNING clause. So we perform
    // the update in a separate query and then query the updated rows.

    DbVariant parent_folder_id_variant(nullptr);
    if (parent_folder_id.value != 0) {
        parent_folder_id_variant = parent_folder_id.value;
    }

    std::vector<DbVariant> args = {
        profile_id.value,
        parent_folder_id_variant,
    };

    // Also note that when normalizing seq_num's, we have to take into account both bookmarks and bookmark folders,
    // because they share the same seq_num space if they share the same parent folder.

    auto num_bookmark_changes = 0;
    auto num_folder_changes = 0;

    {
        // handle bookmarks first
        VERIFY(m_db->exec(R"(
            WITH
                merged AS (
                    SELECT id AS bookmark_id, NULL AS folder_id, profile_id, parent_folder_id, seq_num
                    FROM bookmarks
                    WHERE profile_id = ?1 AND parent_folder_id IS ?2
                    UNION
                    SELECT NULL AS bookmark_id, id AS folder_id, profile_id, parent_folder_id, seq_num
                    FROM bookmark_folders
                    WHERE profile_id = ?1 AND parent_folder_id IS ?2
                    ORDER BY parent_folder_id, seq_num
                ),
                numbered AS (
                    SELECT folder_id, bookmark_id, seq_num, ROW_NUMBER() OVER (ORDER BY seq_num, folder_id, bookmark_id) AS new_seq_num
                    FROM merged
                )
                UPDATE bookmarks
                SET seq_num = numbered.new_seq_num
                FROM numbered
                WHERE bookmarks.id = numbered.bookmark_id;
        )", args));

        DbStmt stmt;
        VERIFY(m_db->query(R"(
            SELECT id, profile_id, name, address, invocation_keyword, parent_folder_id, seq_num
            FROM bookmarks
            WHERE profile_id = ?1 AND parent_folder_id IS ?2
            ORDER BY parent_folder_id, seq_num;
        )", args, &stmt));

        while (stmt.next()) {
            BookmarkDto bookmark{};
            bookmark.id = BookmarkId(stmt.integer("id"));
            bookmark.profile_id = ProfileId(stmt.integer("profile_id"));
            bookmark.name = stmt.text("name");
            bookmark.address = stmt.text("address");
            bookmark.invocation_keyword = stmt.text("invocation_keyword");
            if (!stmt.is_null("parent_folder_id")) {
                bookmark.parent_folder_id = BookmarkFolderId(stmt.integer("parent_folder_id"));
            }
            bookmark.seq_num = stmt.real("seq_num");

            num_bookmark_changes++;
            if (bookmarks_change) {
                bookmarks_change->updated.emplace_back(bookmark);
            }
        }
        VERIFY(stmt.error());
    }
    {
        // handle bookmark_folders second
        VERIFY(m_db->exec(R"(
            WITH
                merged AS (
                    SELECT id AS bookmark_id, NULL AS folder_id, profile_id, parent_folder_id, seq_num
                    FROM bookmarks
                    WHERE profile_id = ?1 AND parent_folder_id IS ?2
                    UNION
                    SELECT NULL AS bookmark_id, id AS folder_id, profile_id, parent_folder_id, seq_num
                    FROM bookmark_folders
                    WHERE profile_id = ?1 AND parent_folder_id IS ?2
                    ORDER BY parent_folder_id, seq_num
                ),
                numbered AS (
                    SELECT folder_id, bookmark_id, seq_num, ROW_NUMBER() OVER (ORDER BY seq_num, folder_id, bookmark_id) AS new_seq_num
                    FROM merged
                )
                UPDATE bookmark_folders
                SET seq_num = numbered.new_seq_num
                FROM numbered
                WHERE bookmark_folders.id = numbered.folder_id;
        )", args));

        DbStmt stmt;
        VERIFY(m_db->query(R"(
            SELECT id, profile_id, name, is_internal, parent_folder_id, seq_num, type
            FROM bookmark_folders
            WHERE profile_id = ?1 AND parent_folder_id IS ?2
            ORDER BY parent_folder_id, seq_num;
        )", args, &stmt));

        while (stmt.next()) {
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

            num_folder_changes++;
            if (folders_change) {
                folders_change->updated.emplace_back(folder);
            }
        }
        VERIFY(stmt.error());
    }
    
    // Recreate indexes after the update
    VERIFY(m_db->exec(folder_index_sql));
    VERIFY(m_db->exec(bookmark_index_sql));

    // Commit the transaction
    VERIFY(tx.commit());

    auto end = std::chrono::high_resolution_clock::now();
    log_debug("normalize_bookmark_seq_num: profile_id=%lld parent_folder_id=%s took=%lldus (updated %d bookmarks and %d bookmark folders)",
        profile_id.value,
        (parent_folder_id.value != 0 ? parent_folder_id.value : DbVariant(nullptr)).to_string().c_str(),
        std::chrono::duration_cast<std::chrono::microseconds>(end - start).count(),
        num_bookmark_changes,
        num_folder_changes
    );

    SUCCEED();
}

Error
ProfilesDb::is_current_tab_bookmarked(IsCurrentTabBookmarkedQuery query, IsCurrentTabBookmarkedReply* out_reply)
{
    if (!query.window_id.value) {
        RAISE(ErrorCode::argument, "window_id is not set");
    }
    RAISE_CHECK(out_reply);

    std::vector<WindowDto> windows;
    VERIFY(get_windows({query.window_id}, &windows));
    if (windows.empty()) {
        RAISE(ErrorCode::not_found, "window with id %lld not found", query.window_id.value);
    }
    const WindowDto& window = windows[0];

    if (!window.selected_tab_id.value) {
        // no selected tab, so cannot be bookmarked
        out_reply->is_bookmarked = false;
        SUCCEED();
    }

    std::vector<TabDto> tabs;
    VERIFY(get_tabs({window.selected_tab_id}, &tabs));
    if (tabs.empty()) {
        // selected tab does not exist anymore, so cannot be bookmarked
        out_reply->is_bookmarked = false;
        SUCCEED();
    }
    const TabDto& tab = tabs[0];

    Error err = m_db->query_bool(R"(
        SELECT 1 FROM bookmarks
        WHERE profile_id = ?1 AND address = ?2
        LIMIT 1;
    )", {window.profile_id.value, tab.uri}, &out_reply->is_bookmarked);
    if (err.code() == ErrorCode::db_no_rows) {
        out_reply->is_bookmarked = false;
    } else {
        VERIFY(err);
    }
    out_reply->current_tab_id = window.selected_tab_id;
    out_reply->address = tab.uri;
    SUCCEED();
}

} // namespace orion