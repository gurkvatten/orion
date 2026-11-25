#include <format>

#include "profiles_db.h"

namespace orion {

//-------------------------------------------------------------------------
// Overflow Menu Items
//-------------------------------------------------------------------------

std::vector<std::string>
ProfilesDb::get_overflow_menu_item_columns() const
{
    return {
        "id",
        "profile_id",
        "action",
        "command_id",
        "idx",
    };
}

void
ProfilesDb::load_overflow_menu_item_from_stmt(const DbStmt& stmt, OverflowMenuItemDto* item) const
{
    item->id = OverflowMenuItemId(stmt.integer("id"));
    item->profile_id = ProfileId(stmt.integer("profile_id"));
    item->action = stmt.text("action");
    item->command_id = stmt.integer("command_id");
    item->idx = stmt.integer("idx");
}

Error
ProfilesDb::get_overflow_menu_items(GetOverflowMenuItemsQuery query, std::vector<OverflowMenuItemDto>* out_items)
{
    RAISE_CHECK(out_items);

    std::vector<std::string> and_where_clauses;
    std::vector<DbVariant> args;

    if (query.profile_id.value != 0) {
        and_where_clauses.push_back("profile_id = ?");
        args.push_back(query.profile_id.value);
    }

    if (query.ids.has_value()) {
        and_where_clauses.push_back(std::format("id IN ({})", Db::make_placeholders(query.ids->size())));
        for (const auto& id : query.ids.value()) {
            args.push_back(id.value);
        }
    }

    std::string where_clause = "";
    if (!and_where_clauses.empty()) {
        where_clause = "WHERE " + Db::join_strings(and_where_clauses, " AND ");
    }

    std::string sql = std::format(R"(
        SELECT {} FROM overflow_menu_items
        {}
        ORDER BY idx;
    )", Db::join_strings(get_overflow_menu_item_columns(), ", "), where_clause);

    DbStmt stmt;
    VERIFY(m_db->query(sql, args, &stmt));

    while (stmt.next()) {
        OverflowMenuItemDto item{};
        load_overflow_menu_item_from_stmt(stmt, &item);
        out_items->push_back(item);
    }
    VERIFY(stmt.error());
    SUCCEED();
}

// Returns -1 if there are no items for the profile
static Error
get_max_idx_for_profile(std::shared_ptr<Db> db, ProfileId profile_id, int64_t* out_max_idx)
{
    std::string sql = R"(
        SELECT COALESCE(MAX(idx), -1) AS max_idx
        FROM overflow_menu_items
        WHERE profile_id = ?;
    )";

    DbStmt stmt;
    VERIFY(db->query(sql, {profile_id.value}, &stmt));

    bool has_row = stmt.next();
    VERIFY(stmt.error());
    RAISE_CHECK(has_row);

    *out_max_idx = stmt.integer("max_idx");
    SUCCEED();
}

Error
ProfilesDb::create_overflow_menu_item(
    CreateOverflowMenuItemQuery query, TxMode tx_mode, OverflowMenuItemId* out_id, OverflowMenuItemsChange* change)
{
    if (query.profile_id.value == 0) {
        RAISE(ErrorCode::argument, "profile_id is not set");
    }

    int64_t max_idx{};
    VERIFY(get_max_idx_for_profile(m_db, query.profile_id, &max_idx));

    if (query.idx == -1 || query.idx > max_idx + 1) {
        query.idx = max_idx + 1;
    }

    DbTx tx;
    if (tx_mode == TxMode::use_tx) {
        VERIFY(tx.begin(m_db));
    }

    // Shift existing items with the same or higher index (if any) by 1 to make room for the new item
    {
        std::string sql = std::format(R"(
            UPDATE overflow_menu_items
            SET idx = idx + 1
            WHERE profile_id = ? AND idx >= ?
            RETURNING {};
        )", Db::join_strings(get_overflow_menu_item_columns(), ", "));

        std::vector<DbVariant> args = {
            query.profile_id.value,
            query.idx,
        };

        DbStmt stmt;
        VERIFY(m_db->query(sql, args, &stmt));
        while (stmt.next()) {
            OverflowMenuItemDto item{};
            load_overflow_menu_item_from_stmt(stmt, &item);
            if (change) {
                change->updated.push_back(item);
            }
        }
        VERIFY(stmt.error());
    }

    std::string sql = std::format(R"(
        INSERT INTO overflow_menu_items ({})
        VALUES ({})
        RETURNING {};
    )", Db::join_strings(get_overflow_menu_item_columns(), ", "),
        Db::make_placeholders(get_overflow_menu_item_columns().size()),
        Db::join_strings(get_overflow_menu_item_columns(), ", ")
    );

    std::vector<DbVariant> args = {
        nullptr, // id will be auto-generated
        query.profile_id.value,
        query.action,
        query.command_id,
        query.idx,
    };

    DbStmt stmt;
    VERIFY(m_db->query(sql, args, &stmt));

    bool has_row = stmt.next();
    VERIFY(stmt.error());
    RAISE_CHECK(has_row);

    OverflowMenuItemDto dto{};
    load_overflow_menu_item_from_stmt(stmt, &dto);

    if (out_id) {
        *out_id = dto.id;
    }

    if (change) {
        change->created.push_back(dto);
    }
    
    if (tx_mode == TxMode::use_tx) {
        VERIFY(tx.commit());
    }
    SUCCEED();
}

Error
ProfilesDb::normalize_overflow_menu_item_indices(TxMode tx_mode, OverflowMenuItemsChange* change)
{
    DbTx tx;
    if (tx_mode == TxMode::use_tx) {
        VERIFY(tx.begin(m_db));
    }

    // Returns all items that need to be updated after deletion
    std::string sql = std::format(R"(
        WITH reordered AS (
            SELECT
                id,
                profile_id,
                ROW_NUMBER() OVER (
                    PARTITION BY profile_id
                    ORDER BY idx
                ) - 1 AS new_idx
            FROM overflow_menu_items
        )
        UPDATE overflow_menu_items
        SET idx = (
            SELECT new_idx
            FROM reordered
            WHERE reordered.id = overflow_menu_items.id
        )
        WHERE idx != (
            SELECT new_idx
            FROM reordered
            WHERE reordered.id = overflow_menu_items.id
        )
        RETURNING {};
    )", Db::join_strings(get_overflow_menu_item_columns(), ", "));

    DbStmt stmt;
    VERIFY(m_db->query(sql, &stmt));

    while (stmt.next()) {
        OverflowMenuItemDto item{};
        load_overflow_menu_item_from_stmt(stmt, &item);
        if (change) {
            change->updated.push_back(item);
        }
    }
    VERIFY(stmt.error());
    
    if (tx_mode == TxMode::use_tx) {
        VERIFY(tx.commit());
    }

    SUCCEED();
}

Error
ProfilesDb::move_overflow_menu_item(MoveOverflowMenuItemQuery query, TxMode tx_mode, OverflowMenuItemsChange* change)
{
    if (query.id.value == 0) {
        RAISE(ErrorCode::argument, "id is not set");
    }

    if (query.id == query.next_id) {
        SUCCEED(); // nothing to do
    }

    // Find the next item
    OverflowMenuItemDto next_item{};
    if (query.next_id.value != 0) {
        std::vector<OverflowMenuItemDto> items;
        VERIFY(get_overflow_menu_items({.ids = std::vector{query.next_id}}, &items));
        if (items.empty()) {
            RAISE(ErrorCode::not_found, "overflow menu item with id %lld not found", query.next_id.value);
        }
        next_item = items[0];
    }

    // Get the profile_id and old index of the item being moved
    ProfileId profile_id{};
    int64_t old_idx = -1;
    {
        std::vector<OverflowMenuItemDto> items;
        VERIFY(get_overflow_menu_items({.ids = std::vector{query.id}}, &items));
        if (items.empty()) {
            RAISE(ErrorCode::not_found, "overflow menu item with id %lld not found", query.id.value);
        }
        profile_id = items[0].profile_id;
        old_idx = items[0].idx;
    }

    RAISE_CHECK(old_idx != -1);

    int64_t max_idx{};
    VERIFY(get_max_idx_for_profile(m_db, profile_id, &max_idx));

    RAISE_CHECK(max_idx != -1); // there should be at least one item for the profile (the moved item)

    int64_t new_idx = next_item.idx - 1; // by default, the item will be moved before the next item
    if (next_item.id.value == 0) {
        // There is no next item, the source item should be moved to the end of the list
        new_idx = max_idx;
    } else if (new_idx == -1) {
        // The next item is the first item in the list, the source item should be moved to the beginning of the list
        new_idx = 0;
    }

    // If there already is an item at the specified index, the item will be inserted exactly at the specified
    // index and all existing items between the old and new index will be shifted by 1.

    DbTx tx;
    if (tx_mode == TxMode::use_tx) {
        VERIFY(tx.begin(m_db));
    }

    // Shift items between old_idx and new_idx by 1 to make room for the moved item
    {
        DbStmt stmt;
        if (old_idx < new_idx) {
            // Moving down: shift items between old_idx + 1 and new_idx up by 1
            std::string sql = std::format(R"(
                UPDATE overflow_menu_items
                SET idx = idx - 1
                WHERE profile_id = ? AND idx > ? AND idx <= ?
                RETURNING {};
            )", Db::join_strings(get_overflow_menu_item_columns(), ", "));
            VERIFY(m_db->query(sql, {profile_id.value, old_idx, new_idx}, &stmt));
        } else {
            // Moving up: shift items between new_idx and old_idx - 1 down by 1
            std::string sql = std::format(R"(
                UPDATE overflow_menu_items
                SET idx = idx + 1
                WHERE profile_id = ? AND idx >= ? AND idx < ?
                RETURNING {};
            )", Db::join_strings(get_overflow_menu_item_columns(), ", "));
            VERIFY(m_db->query(sql, {profile_id.value, new_idx, old_idx}, &stmt));
        }

        while (stmt.next()) {
            OverflowMenuItemDto item{};
            load_overflow_menu_item_from_stmt(stmt, &item);
            if (change) {
                change->updated.push_back(item);
            }
        }
        VERIFY(stmt.error());
    }

    // Update the index of the moved item
    {
        std::string sql = std::format(R"(
            UPDATE overflow_menu_items
            SET idx = ?
            WHERE id = ?
            RETURNING {};
        )", Db::join_strings(get_overflow_menu_item_columns(), ", "));

        DbStmt stmt;
        VERIFY(m_db->query(sql, {new_idx, query.id.value}, &stmt));

        bool has_row = stmt.next();
        VERIFY(stmt.error());
        RAISE_CHECK(has_row);

        OverflowMenuItemDto item{};
        load_overflow_menu_item_from_stmt(stmt, &item);
        if (change) {
            change->updated.push_back(item);
        }
    }

    if (tx_mode == TxMode::use_tx) {
        VERIFY(tx.commit());
    }
    SUCCEED();
}

Error
ProfilesDb::delete_overflow_menu_items(
    const std::vector<OverflowMenuItemId>& ids, TxMode tx_mode, OverflowMenuItemsChange* change)
{
    if (ids.empty()) {
        SUCCEED(); // nothing to delete
    }
    
    DbTx tx;
    if (tx_mode == TxMode::use_tx) {
        VERIFY(tx.begin(m_db));
    }

    std::string sql = std::format(R"(
        DELETE FROM overflow_menu_items
        WHERE id IN ({});
    )", Db::make_placeholders(ids.size()));

    std::vector<DbVariant> args;
    for (const auto& id : ids) {
        args.emplace_back(id.value);
    }

    VERIFY(m_db->exec(sql, args));

    if (change) {
        for (const auto& id : ids) {
            change->deleted.push_back(id);
        }
    }

    VERIFY(normalize_overflow_menu_item_indices(TxMode::no_tx, change));

    if (tx_mode == TxMode::use_tx) {
        VERIFY(tx.commit());
    }
    SUCCEED();
}

} // namespace orion