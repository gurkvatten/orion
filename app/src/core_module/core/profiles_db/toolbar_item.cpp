#include <format>

#include "profiles_db.h"

namespace orion {

//-------------------------------------------------------------------------
// Toolbar Items
//-------------------------------------------------------------------------

std::vector<std::string>
ProfilesDb::get_toolbar_item_columns() const
{
    return {
        "id",
        "profile_id",
        "item_type",
        "position",
        "in_overflow_menu",
        "is_compact_tab_layout",
    };
}

void
ProfilesDb::load_toolbar_item_from_stmt(const DbStmt& stmt, ToolbarItemDto* item) const
{
    item->id = ToolbarItemId(stmt.integer("id"));
    item->profile_id = ProfileId(stmt.integer("profile_id"));
    item->item_type = ToolbarItemType(stmt.integer("item_type"));
    item->position = stmt.integer("position");
    item->in_overflow_menu = stmt.integer("in_overflow_menu") != 0;
    item->is_compact_tab_layout = stmt.integer("is_compact_tab_layout") != 0;
}

Error
ProfilesDb::get_default_toolbar_items(GetDefaultToolbarItemsQuery query, std::vector<ToolbarItemDto>* out_items)
{
    RAISE_CHECK(out_items);

    if (query.in_toolbar) {
        std::vector<ToolbarItemType> types;

        if (query.is_compact_tab_layout) {
            types = {
                ToolbarItemType::sidebar,
                ToolbarItemType::tab_groups,
                ToolbarItemType::back_forward,
                ToolbarItemType::flexible_space,
                ToolbarItemType::address_and_search,
                ToolbarItemType::flexible_space,
                ToolbarItemType::overflow_menu,
            };
        } else {
            types = {
                ToolbarItemType::sidebar,
                ToolbarItemType::tab_groups,
                ToolbarItemType::back_forward,
                ToolbarItemType::flexible_space,
                ToolbarItemType::privacy,
                ToolbarItemType::page_tweaks,
                ToolbarItemType::website_settings,
                ToolbarItemType::address_and_search,
                ToolbarItemType::bookmark,
                ToolbarItemType::new_tab,
                ToolbarItemType::flexible_space,
                ToolbarItemType::downloads,
                ToolbarItemType::share,
                ToolbarItemType::tab_overview,
                ToolbarItemType::overflow_menu,
            };
        }

        for (ToolbarItemType type : types) {
            ToolbarItemDto item{
                .item_type = type,
                .is_compact_tab_layout = query.is_compact_tab_layout,
            };
            out_items->push_back(item);
        }
    }

    if (query.in_overflow_menu) {
        std::vector<ToolbarItemDto> items;
        VERIFY(get_default_overflow_menu_items(query.is_compact_tab_layout, &items));

        for (ToolbarItemDto item : items) {
            out_items->push_back(item);
        }
    }
    SUCCEED();
}

Error
ProfilesDb::get_default_overflow_menu_items(bool is_compact_tab_layout, std::vector<ToolbarItemDto>* out_items)
{
    RAISE_CHECK(out_items);

    std::vector<ToolbarItemType> types;

    if (is_compact_tab_layout) {
        types = {
            ToolbarItemType::website_settings,
            ToolbarItemType::bookmark,
            ToolbarItemType::share,
            ToolbarItemType::tab_overview,
            ToolbarItemType::summarize_page,
            ToolbarItemType::programmable_button,
        };
    } else {
        types = {
            ToolbarItemType::summarize_page,
            ToolbarItemType::programmable_button,
        };
    }

    for (ToolbarItemType type : types) {
        ToolbarItemDto item{
            .item_type = type,
            .is_compact_tab_layout = is_compact_tab_layout,
            .in_overflow_menu = true,
        };
        out_items->push_back(item);
    }
    SUCCEED();
}

Error
ProfilesDb::get_available_toolbar_items(GetAvailableToolbarItemsQuery query, std::vector<ToolbarItemDto>* out_items)
{
    RAISE_CHECK(out_items);

    std::vector<ToolbarItemType> types{
        ToolbarItemType::flexible_space,
        ToolbarItemType::address_and_search,
        ToolbarItemType::back_forward,
        ToolbarItemType::privacy,
        ToolbarItemType::home,
        ToolbarItemType::start_page,
        ToolbarItemType::notes,
        ToolbarItemType::website_settings,
        ToolbarItemType::downloads,
        ToolbarItemType::share,
        ToolbarItemType::new_tab,
        ToolbarItemType::new_window,
        ToolbarItemType::new_private_window,
        ToolbarItemType::history,
        ToolbarItemType::sidebar,
        ToolbarItemType::tab_groups,
        ToolbarItemType::bookmark,
        ToolbarItemType::zoom,
        ToolbarItemType::print,
        ToolbarItemType::web_inspector,
        ToolbarItemType::overflow_menu,
        ToolbarItemType::extensions,
        ToolbarItemType::toggle_reader,
        ToolbarItemType::tab_overview,
        ToolbarItemType::programmable_button,
        ToolbarItemType::page_tweaks,
        ToolbarItemType::focus_mode,
        ToolbarItemType::summarize_page,
        ToolbarItemType::kagi_privacy_pass,
    };

    if (query.is_compact_tab_layout) {
        std::erase(types, ToolbarItemType::new_tab);
    }

    for (ToolbarItemType type : types) {
        ToolbarItemDto item{
            .item_type = type,
        };
        out_items->push_back(item);
    }

    // TODO: add extension toolbar items for the specified profile
    SUCCEED();
}

Error
ProfilesDb::create_default_toolbar_items(
    ProfileId profile_id,
    TxMode tx_mode,
    ToolbarItemsChange* change,
    ProgrammableButtonsChange* prog_buttons_change)
{    
    DbTx tx;
    if (tx_mode == TxMode::use_tx) {
        VERIFY(tx.begin(m_db));
    }

    for (bool is_compact_tab_layout : {false, true}) {
        std::vector<ToolbarItemDto> default_items;

        GetDefaultToolbarItemsQuery query{
            .is_compact_tab_layout = is_compact_tab_layout,
            .in_toolbar = true,
            .in_overflow_menu = true,
        };
        VERIFY(get_default_toolbar_items(query, &default_items));

        for (ToolbarItemDto item : default_items) {
            CreateToolbarItemQuery query{
                .profile_id = profile_id,
                .item_type = item.item_type,
                .is_compact_tab_layout = item.is_compact_tab_layout,
                .in_overflow_menu = item.in_overflow_menu,
            };
            VERIFY(create_toolbar_item(query, TxMode::no_tx, nullptr, change, prog_buttons_change));
        }
    }

    if (tx_mode == TxMode::use_tx) {
        VERIFY(tx.commit());
    }

    SUCCEED();
}

Error
ProfilesDb::get_toolbar_items(GetToolbarItemsQuery query, std::vector<ToolbarItemDto>* out_items)
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

    and_where_clauses.push_back("is_compact_tab_layout = ?");
    args.push_back(query.is_compact_tab_layout);

    if (query.exclude_in_overflow_menu) {
        and_where_clauses.push_back("in_overflow_menu = 0");
    }

    if (query.exclude_in_toolbar) {
        and_where_clauses.push_back("in_overflow_menu = 1");
    }

    std::string where_clause = "";
    if (!and_where_clauses.empty()) {
        where_clause = "WHERE " + Db::join_strings(and_where_clauses, " AND ");
    }

    std::string sql = std::format(R"(
        SELECT {} FROM toolbar_items
        {}
        ORDER BY position;
    )", Db::join_strings(get_toolbar_item_columns(), ", "), where_clause);

    DbStmt stmt;
    VERIFY(m_db->query(sql, args, &stmt));

    int64_t next_overflow_menu_position = 0;
    int64_t next_toolbar_position = 0;

    while (stmt.next()) {
        ToolbarItemDto item{};
        load_toolbar_item_from_stmt(stmt, &item);

        if (item.in_overflow_menu) {
            item.category_position = next_overflow_menu_position++;
        } else {
            item.category_position = next_toolbar_position++;
        }

        out_items->push_back(item);
    }
    VERIFY(stmt.error());
    SUCCEED();
}

struct GetMaxPositionForProfileQuery {
    std::shared_ptr<Db> db;
    ProfileId profile_id{};
    bool is_compact_tab_layout{};
    std::optional<bool> in_overflow_menu{};
};

// Returns -1 if there are no items for the profile. If in_overflow_menu is not nullptr, only items in the specified
// category (Overflow Menu / Toolbar) are considered.
static Error
get_max_position_for_profile(GetMaxPositionForProfileQuery query, int64_t* out_max_idx)
{
    RAISE_CHECK(out_max_idx);

    std::string and_where_clause = "WHERE profile_id = ? AND is_compact_tab_layout = ?";
    
    std::vector<DbVariant> args{
        query.profile_id.value,
        query.is_compact_tab_layout,
    };

    if (query.in_overflow_menu) {
        and_where_clause += " AND in_overflow_menu = ?";
        args.push_back(query.in_overflow_menu.value());
    }

    std::string sql = std::format(R"(
        SELECT COALESCE(MAX(position), -1) AS max_position
        FROM toolbar_items
        {}
    )", and_where_clause);

    DbStmt stmt;
    VERIFY(query.db->query(sql, args, &stmt));

    bool has_row = stmt.next();
    VERIFY(stmt.error());
    RAISE_CHECK(has_row);

    *out_max_idx = stmt.integer("max_position");
    SUCCEED();
}

struct GetMaxCategoryPositionForProfileQuery {
    std::shared_ptr<Db> db;
    ProfileId profile_id{};
    bool is_compact_tab_layout{};
    bool in_overflow_menu{};
};

// Returns -1 if there are no items for the profile in the specified category
static Error
get_max_category_position_for_profile(GetMaxCategoryPositionForProfileQuery query, int64_t* out_position)
{
    RAISE_CHECK(out_position);

    std::string sql = R"(
        SELECT COUNT(*)
        FROM toolbar_items
        WHERE profile_id = ? AND is_compact_tab_layout = ? AND in_overflow_menu = ?
        ORDER BY position;
    )";

    std::vector<DbVariant> args{
        query.profile_id.value,
        query.is_compact_tab_layout,
        query.in_overflow_menu,
    };

    int64_t count = 0;
    VERIFY(query.db->query_integer(sql, args, &count));

    *out_position = count - 1;
    SUCCEED();
}

struct GetCategoryPositionForItemQuery {
    std::shared_ptr<Db> db;
    ProfileId profile_id{};
    ToolbarItemId item_id{};
    bool is_compact_tab_layout{};
    bool in_overflow_menu{};
};

// Returns -1 if the item could not be found
static Error
get_category_position_for_item(GetCategoryPositionForItemQuery query, int64_t* out_position)
{
    RAISE_CHECK(out_position);

    std::string sql = R"(
        SELECT id
        FROM toolbar_items
        WHERE profile_id = ? AND is_compact_tab_layout = ? AND in_overflow_menu = ?
        ORDER BY position;
    )";

    std::vector<DbVariant> args{
        query.profile_id.value,
        query.is_compact_tab_layout,
        query.in_overflow_menu,
    };

    DbStmt stmt;
    VERIFY(query.db->query(sql, args, &stmt));

    int64_t category_position = -1;

    while (stmt.next()) {
        category_position++;
        if (ToolbarItemId{stmt.integer("id")} == query.item_id) {
            break;
        }
    }
    *out_position = category_position;
    SUCCEED();
}

Error
ProfilesDb::create_toolbar_item(
    CreateToolbarItemQuery query,
    TxMode tx_mode,
    ToolbarItemId* out_id,
    ToolbarItemsChange* change,
    ProgrammableButtonsChange* prog_buttons_change)
{
    if (query.profile_id.value == 0) {
        RAISE(ErrorCode::argument, "profile_id is not set");
    }

    // Find the next item to determine the position of the new item
    ToolbarItemDto next_item{};
    if (query.next_item_id.value != 0) {
        std::vector<ToolbarItemDto> items;
        GetToolbarItemsQuery get_query{
            .profile_id = query.profile_id,
            .ids = std::vector{query.next_item_id},
            .is_compact_tab_layout = query.is_compact_tab_layout,
        };
        VERIFY(get_toolbar_items(get_query, &items));
        if (items.empty()) {
            RAISE(ErrorCode::not_found, "toolbar item with id %lld not found", query.next_item_id.value);
        }
        next_item = items[0];
    }

    DbTx tx;
    if (tx_mode == TxMode::use_tx) {
        VERIFY(tx.begin(m_db));
    }

    if (query.next_item_id.value == 0) {
        // If there is no next item, the new item should be added to the end of the list
        int64_t max_position{};
        VERIFY(get_max_position_for_profile({
            .db = m_db,
            .profile_id = query.profile_id,
            .is_compact_tab_layout = query.is_compact_tab_layout,
            .in_overflow_menu = query.in_overflow_menu
        }, &max_position));

        next_item.position = max_position + 1;

        int64_t max_category_position{};
        VERIFY(get_max_category_position_for_profile({
            .db = m_db,
            .profile_id = query.profile_id,
            .is_compact_tab_layout = query.is_compact_tab_layout,
            .in_overflow_menu = query.in_overflow_menu
        }, &max_category_position));

        next_item.category_position = max_category_position + 1;
    }

    // Shift existing items with the same or higher index (if any) by 1 to make room for the new item.
    // It is possible that this is also needed to do when inserting at the end of the toolbar list
    // because all overflow menu items must be shifted as well.
    {
        std::string sql = std::format(R"(
            UPDATE toolbar_items
            SET position = position + 1
            WHERE profile_id = ? AND is_compact_tab_layout = ? AND position >= ?
            RETURNING {};
        )", Db::join_strings(get_toolbar_item_columns(), ", "));

        std::vector<DbVariant> args{
            query.profile_id.value,
            query.is_compact_tab_layout,
            next_item.position,
        };

        DbStmt stmt;
        VERIFY(m_db->query(sql, args, &stmt));

        while (stmt.next()) {
            ToolbarItemDto item{};
            load_toolbar_item_from_stmt(stmt, &item);

            VERIFY(get_category_position_for_item({
                .db = m_db,
                .profile_id = item.profile_id,
                .item_id = item.id,
                .is_compact_tab_layout = item.is_compact_tab_layout,
                .in_overflow_menu = item.in_overflow_menu,
            }, &item.category_position));

            if (change) {
                change->updated.push_back(item);
            }
        }
        VERIFY(stmt.error());
    }

    ToolbarItemDto dto{};
    {
        std::string sql = std::format(R"(
            INSERT INTO toolbar_items ({})
            VALUES ({})
            RETURNING {};
        )", Db::join_strings(get_toolbar_item_columns(), ", "),
            Db::make_placeholders(get_toolbar_item_columns().size()),
            Db::join_strings(get_toolbar_item_columns(), ", ")
        );

        std::vector<DbVariant> args{
            nullptr, // id will be auto-generated
            query.profile_id.value,
            static_cast<int64_t>(query.item_type),
            next_item.position,
            query.in_overflow_menu,
            query.is_compact_tab_layout,
        };

        DbStmt stmt;
        VERIFY(m_db->query(sql, args, &stmt));

        bool has_row = stmt.next();
        VERIFY(stmt.error());
        RAISE_CHECK(has_row);

        load_toolbar_item_from_stmt(stmt, &dto);

        VERIFY(get_category_position_for_item({
            .db = m_db,
            .profile_id = next_item.profile_id,
            .item_id = dto.id,
            .is_compact_tab_layout = dto.is_compact_tab_layout,
            .in_overflow_menu = dto.in_overflow_menu,
        }, &dto.category_position));

        if (out_id) {
            *out_id = dto.id;
        }
        
        if (change) {
            change->created.push_back(dto);
        }
        // stmt must be finalized before commit
    }

    if (query.item_type == ToolbarItemType::programmable_button)
    {
        CreateProgrammableButtonQuery programmable_button_query{};
        programmable_button_query.id = PROGRAMMABLE_BUTTON_ID(dto.id);
        programmable_button_query.profile_id = dto.profile_id;
        create_programmable_button(programmable_button_query, nullptr, prog_buttons_change);
    }
    
    if (tx_mode == TxMode::use_tx) {
        VERIFY(tx.commit());
    }
    SUCCEED();
}

Error
ProfilesDb::normalize_toolbar_item_positions(TxMode tx_mode, ToolbarItemsChange* change)
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
                is_compact_tab_layout,
                ROW_NUMBER() OVER (
                    PARTITION BY profile_id, is_compact_tab_layout
                    ORDER BY profile_id, in_overflow_menu, position
                ) - 1 AS new_position,
                position,
                in_overflow_menu
            FROM toolbar_items
        )
        UPDATE toolbar_items
        SET position = (
            SELECT new_position
            FROM reordered
            WHERE reordered.id = toolbar_items.id
        )
        WHERE position != (
            SELECT new_position
            FROM reordered
            WHERE reordered.id = toolbar_items.id
        )
        RETURNING {};
    )", Db::join_strings(get_toolbar_item_columns(), ", "));

    DbStmt stmt;
    VERIFY(m_db->query(sql, &stmt));

    while (stmt.next()) {
        ToolbarItemDto item{};
        load_toolbar_item_from_stmt(stmt, &item);

        VERIFY(get_category_position_for_item({
            .db = m_db,
            .profile_id = item.profile_id,
            .item_id = item.id,
            .is_compact_tab_layout = item.is_compact_tab_layout,
            .in_overflow_menu = item.in_overflow_menu
        }, &item.category_position));

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
ProfilesDb::move_toolbar_item(MoveToolbarItemQuery query, TxMode tx_mode, ToolbarItemsChange* change)
{
    if (query.id.value == 0) {
        RAISE(ErrorCode::argument, "id is not set");
    }

    if (query.id == query.next_item_id) {
        SUCCEED(); // nothing to do
    }

    // Find the next item
    ToolbarItemDto next_item{};
    if (query.next_item_id.value != 0) {
        std::vector<ToolbarItemDto> items;

        // Try to find the next item in both Compact Tabs layout and non-Compact Tabs layout
        GetToolbarItemsQuery get_query{
            .ids = std::vector{query.next_item_id},
            .is_compact_tab_layout = false,
        };
        VERIFY(get_toolbar_items(get_query, &items));
        get_query.is_compact_tab_layout = true;
        VERIFY(get_toolbar_items(get_query, &items));

        if (items.empty()) {
            RAISE(ErrorCode::not_found, "toolbar item with id %lld not found", query.next_item_id.value);
        }
        next_item = items[0];
    }

    // Get the profile_id and old index of the item being moved
    ProfileId profile_id{};
    int64_t old_position = -1;
    bool is_compact_tab_layout = false;
    bool in_overflow_menu = false;
    {
        std::vector<ToolbarItemDto> items;

        // Try to find the item in both Compact Tabs layout and non-Compact Tabs layout
        GetToolbarItemsQuery get_query{
            .ids = std::vector{query.id},
            .is_compact_tab_layout = false,
        };
        VERIFY(get_toolbar_items(get_query, &items));
        get_query.is_compact_tab_layout = true;
        VERIFY(get_toolbar_items(get_query, &items));

        if (items.empty()) {
            RAISE(ErrorCode::not_found, "toolbar item with id %lld not found", query.id.value);
        }
        profile_id = items[0].profile_id;
        old_position = items[0].position;
        in_overflow_menu = items[0].in_overflow_menu;
        is_compact_tab_layout = items[0].is_compact_tab_layout;
    }

    if (next_item.id.value != 0) {
        // A next item exists
        RAISE_CHECK(profile_id == next_item.profile_id);
        RAISE_CHECK(is_compact_tab_layout == next_item.is_compact_tab_layout);
    }

    RAISE_CHECK(old_position != -1);

    int64_t max_position_in_category{};
    VERIFY(get_max_position_for_profile({
        .db = m_db,
        .profile_id = profile_id,
        .is_compact_tab_layout = is_compact_tab_layout,
        .in_overflow_menu = in_overflow_menu
    }, &max_position_in_category));

    RAISE_CHECK(max_position_in_category != -1); // there should be at least one item for the profile (the moved item)

    int64_t new_position{0};
    
    if (old_position + 1 < next_item.position) {
        // Moving the item down the list
        new_position = next_item.position - 1; // the item should replace the position of the previous item
    } else {
        // Moving the item up the list
        new_position = next_item.position; // the item should replace the position of the next item
    }
    
    if (next_item.id.value == 0) {
        // There is no next item, the source item should be moved to the end of the list
        new_position = max_position_in_category;
    } else if (new_position == -1) {
        // The next item is the first item in the list, the source item should be moved to the beginning of the list
        new_position = 0;
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
        if (old_position < new_position) {
            // Moving down: shift items between old_position + 1 and new_position up by 1
            std::string sql = std::format(R"(
                UPDATE toolbar_items
                SET position = position - 1
                WHERE profile_id = ? AND is_compact_tab_layout = ? AND position > ? AND position <= ?
                RETURNING {};
            )", Db::join_strings(get_toolbar_item_columns(), ", "));

            std::vector<DbVariant> args{
                profile_id.value,
                is_compact_tab_layout,
                old_position,
                new_position,
            };
            VERIFY(m_db->query(sql, args, &stmt));
        } else {
            // Moving up: shift items between new_position and old_position - 1 down by 1
            std::string sql = std::format(R"(
                UPDATE toolbar_items
                SET position = position + 1
                WHERE profile_id = ? AND is_compact_tab_layout = ? AND position >= ? AND position < ?
                RETURNING {};
            )", Db::join_strings(get_toolbar_item_columns(), ", "));

            std::vector<DbVariant> args{
                profile_id.value,
                is_compact_tab_layout,
                new_position,
                old_position,
            };
            VERIFY(m_db->query(sql, args, &stmt));
        }

        while (stmt.next()) {
            ToolbarItemDto item{};
            load_toolbar_item_from_stmt(stmt, &item);

            if (change) {
                change->updated.push_back(item);
            }
        }
        VERIFY(stmt.error());
    }

    // Update the index of the moved item
    {
        std::string sql = std::format(R"(
            UPDATE toolbar_items
            SET position = ?
            WHERE id = ?
            RETURNING {};
        )", Db::join_strings(get_toolbar_item_columns(), ", "));

        DbStmt stmt;
        VERIFY(m_db->query(sql, {new_position, query.id.value}, &stmt));

        bool has_row = stmt.next();
        VERIFY(stmt.error());
        RAISE_CHECK(has_row);

        ToolbarItemDto item{};
        load_toolbar_item_from_stmt(stmt, &item);

        VERIFY(get_category_position_for_item({
            .db = m_db,
            .profile_id = item.profile_id,
            .item_id = item.id,
            .is_compact_tab_layout = item.is_compact_tab_layout,
            .in_overflow_menu = item.in_overflow_menu
        }, &item.category_position));

        if (change) {
            change->updated.push_back(item);
        }
    }

    // Fill category positions for shifted items
    if (change) {
        for (auto& item : change->updated) {
            if (item.id == query.id) {
                // The moved item already has its category_position set
                continue;
            }

            VERIFY(get_category_position_for_item({
                .db = m_db,
                .profile_id = item.profile_id,
                .item_id = item.id,
                .is_compact_tab_layout = item.is_compact_tab_layout,
                .in_overflow_menu = item.in_overflow_menu
            }, &item.category_position));
        }
    }

    if (tx_mode == TxMode::use_tx) {
        VERIFY(tx.commit());
    }
    SUCCEED();
}

Error
ProfilesDb::delete_toolbar_items(
    const std::vector<ToolbarItemId>& ids,
    TxMode tx_mode,
    ToolbarItemsChange* change,
    ProgrammableButtonsChange* prog_buttons_change)
{
    if (ids.empty()) {
        SUCCEED(); // nothing to delete
    }
    
    DbTx tx;
    if (tx_mode == TxMode::use_tx) {
        VERIFY(tx.begin(m_db));
    }

    std::vector<ProgrammableButtonId> prog_button_ids{};
    for(auto& id: ids)
    {
        prog_button_ids.push_back(PROGRAMMABLE_BUTTON_ID(id));
    }
    delete_programmable_buttons(prog_button_ids, prog_buttons_change);

    // In SQLite, we can combine DELETE with RETURNING clause. But we probably cannot use CTE to project/order
    // the returning rows - that would require additional SELECT query.
    std::string sql = std::format(R"(
        DELETE FROM toolbar_items
        WHERE id IN ({})
        RETURNING {};
    )", Db::make_placeholders(ids.size()),
        Db::join_strings(get_toolbar_item_columns(), ", ")
    );

    std::vector<DbVariant> args;
    for (const auto& id : ids) {
        args.emplace_back(id.value);
    }

    DbStmt stmt;
    VERIFY(m_db->query(sql, args, &stmt));

    while (stmt.next()) {
        ToolbarItemDto item{};
        load_toolbar_item_from_stmt(stmt, &item);

        if (change) {
            change->deleted.push_back(item);
        }
    }
    VERIFY(stmt.error());

    VERIFY(normalize_toolbar_item_positions(TxMode::no_tx, change));

    if (tx_mode == TxMode::use_tx) {
        VERIFY(tx.commit());
    }
    SUCCEED();
}

Error
ProfilesDb::update_toolbar_item(
    UpdateToolbarItemQuery query,
    TxMode tx_mode,
    ToolbarItemsChange* change,
    ProgrammableButtonsChange* prog_buttons_change)
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

    if (query.update_item_type) {
        set_clauses.push_back("item_type = ?");
        args.push_back(static_cast<int64_t>(query.item_type));
    }

    if (query.update_in_overflow_menu) {
        set_clauses.push_back("in_overflow_menu = ?");
        args.push_back(query.in_overflow_menu);
    }

    if (set_clauses.empty()) {
        SUCCEED(); // nothing to update
    }

    std::string sql = std::format(R"(
        UPDATE toolbar_items
        SET {}
        WHERE id = ?
        RETURNING {};
    )", Db::join_strings(set_clauses, ", "),
        Db::join_strings(get_toolbar_item_columns(), ", ")
    );

    args.push_back(query.id.value);

    {
        DbStmt stmt;
        VERIFY(m_db->query(sql, args, &stmt));

        bool has_row = stmt.next();
        VERIFY(stmt.error());
        RAISE_CHECK(has_row);

        ToolbarItemDto item{};
        load_toolbar_item_from_stmt(stmt, &item);

        // Note that this can append the change of the updated item
        VERIFY(normalize_toolbar_item_positions(TxMode::no_tx, change));
        
        if (change) {
            // Check if the updated item is already in the change.updated list. This happens when
            // the item's position was changed due to normalization
            bool found = false;
            for (const auto& changed_item : change->updated) {
                if (changed_item.id == item.id) {
                    found = true;
                    break;
                }
            }

            // Avoid duplicate change entries for the same item
            if (!found) {
                change->updated.push_back(item);
            }
        }

        if (query.update_item_type)
        {
            if (query.item_type == ToolbarItemType::programmable_button)
            {
                CreateProgrammableButtonQuery query{};
                query.id = PROGRAMMABLE_BUTTON_ID(item.id);
                query.profile_id = item.profile_id;
                create_programmable_button(query, nullptr, prog_buttons_change);
            }
            else
            {
                delete_programmable_buttons({PROGRAMMABLE_BUTTON_ID(item.id)}, prog_buttons_change);
            }
        }
    }

    if (tx_mode == TxMode::use_tx) {
        VERIFY(tx.commit());
    }
    
    SUCCEED();
}

Error
ProfilesDb::reset_toolbar_items(
    ResetToolbarItemsQuery query,
    TxMode tx_mode,
    ToolbarItemsChange* change,
    ProgrammableButtonsChange* prog_buttons_change)
{
    if (query.profile_id.value == 0) {
        RAISE(ErrorCode::argument, "profile_id is not set");
    }
    
    DbTx tx;
    if (tx_mode == TxMode::use_tx) {
        VERIFY(tx.begin(m_db));
    }

    GetToolbarItemsQuery get_query{
        .profile_id = query.profile_id,
        .is_compact_tab_layout = query.is_compact_tab_layout,
    };

    std::vector<ToolbarItemDto> items;
    VERIFY(get_toolbar_items(get_query, &items));

    std::vector<ToolbarItemId> ids_to_delete;

    for (const auto& item : items) {
        if (query.in_toolbar && !item.in_overflow_menu) {
            ids_to_delete.push_back(item.id);
        } else if (query.in_overflow_menu && item.in_overflow_menu) {
            ids_to_delete.push_back(item.id);
        }
    }

    VERIFY(delete_toolbar_items(ids_to_delete, TxMode::no_tx, change, prog_buttons_change));

    std::vector<ToolbarItemDto> default_items;
    ToolbarItemId next_item_id{0};

    if (query.in_toolbar) {
        GetDefaultToolbarItemsQuery default_query{
            .is_compact_tab_layout = query.is_compact_tab_layout,
            .in_toolbar = true,
            .in_overflow_menu = false,
        };
        VERIFY(get_default_toolbar_items(default_query, &default_items));

        // Get the first item in the overflow menu to determine the position of the next item
        GetToolbarItemsQuery overflow_query{
            .profile_id = query.profile_id,
            .exclude_in_toolbar = true,
            .is_compact_tab_layout = query.is_compact_tab_layout,
        };

        std::vector<ToolbarItemDto> overflow_items;
        VERIFY(get_toolbar_items(overflow_query, &overflow_items));

        if (!overflow_items.empty()) {
            next_item_id = overflow_items[0].id;
        }
    }

    if (query.in_overflow_menu) {
        VERIFY(get_default_overflow_menu_items(query.is_compact_tab_layout, &default_items));

        // Always append to the end of the toolbar items list
        next_item_id = 0;
    }

    for (const auto& item : default_items) {
        CreateToolbarItemQuery create_query{
            .profile_id = query.profile_id,
            .item_type = item.item_type,
            .next_item_id = next_item_id,
            .is_compact_tab_layout = item.is_compact_tab_layout,
            // in_overflow_menu will be set below
        };
        if (query.in_toolbar && !item.in_overflow_menu) {
            create_query.in_overflow_menu = false;
        } else if (query.in_overflow_menu && item.in_overflow_menu) {
            create_query.in_overflow_menu = true;
        }
        VERIFY(create_toolbar_item(create_query, TxMode::no_tx, nullptr, change, prog_buttons_change));
    }

    if (tx_mode == TxMode::use_tx) {
        VERIFY(tx.commit());
    }

    SUCCEED();
}

} // namespace orion