#include <core/profiles_db/profiles_db.h>
#include <format>

namespace orion {

//-------------------------------------------------------------------------
// Profiles
//-------------------------------------------------------------------------

Error
ProfilesDb::get_profile_ids(std::vector<ProfileId>* ids)
{
    DbStmt stmt;
    VERIFY(m_db->query("SELECT id FROM profiles ORDER BY seq_num;", {}, &stmt));

    while (stmt.next()) {
        ids->push_back(ProfileId(stmt.integer("id")));
    }
    VERIFY(stmt.error());
    SUCCEED();
}

Error
ProfilesDb::get_default_profile_id(ProfileId* profile_id)
{
    DbStmt stmt;
    VERIFY(m_db->query("SELECT id FROM profiles WHERE is_default = 1;", {}, &stmt));

    bool has_row = stmt.next();
    VERIFY(stmt.error());
    if (!has_row) {
        RAISE(ErrorCode::not_found, "no default profile found");
    }
    *profile_id = ProfileId(stmt.integer("id"));
    SUCCEED();
}

Error
ProfilesDb::get_internal_profile_id(ProfileId* profile_id)
{
    DbStmt stmt;
    VERIFY(m_db->query("SELECT id FROM profiles WHERE is_internal = 1;", {}, &stmt));

    bool has_row = stmt.next();
    VERIFY(stmt.error());
    if (!has_row) {
        RAISE(ErrorCode::not_found, "no internal profile found");
    }
    *profile_id = ProfileId(stmt.integer("id"));
    SUCCEED();
}

std::vector<std::string>
ProfilesDb::get_profile_columns() const
{
    return {
        "id",
        "name",
        "icon_name",
        "show_in_address_bar",
        "show_badge_in_app_icon",
        "keyboard_shortcut",
        "seq_num",
        "is_internal",
        "is_default",
        "selected_window_id",
        "shared_pinned_tabs_window_id",
        "is_test",
    };
}

void
ProfilesDb::load_profile_from_stmt(const DbStmt& stmt, ProfileDto* profile) const
{
    profile->id = ProfileId(stmt.integer("id"));
    profile->name = stmt.text("name");
    profile->icon_name = stmt.text("icon_name");
    profile->show_in_address_bar = stmt.integer("show_in_address_bar");
    profile->show_badge_in_app_icon = stmt.integer("show_badge_in_app_icon");
    profile->keyboard_shortcut = stmt.text("keyboard_shortcut");
    profile->seq_num = stmt.real("seq_num");
    profile->is_internal = stmt.integer("is_internal");
    profile->is_default = stmt.integer("is_default");
    profile->selected_window_id = WindowId(stmt.integer("selected_window_id"));
    profile->shared_pinned_tabs_window_id = WindowId(stmt.integer("shared_pinned_tabs_window_id"));
    profile->is_test = stmt.integer("is_test");
}

Error
ProfilesDb::get_profiles(const std::vector<ProfileId>& ids, std::vector<ProfileDto>* profiles)
{
    auto args = to_db_variant_vector(ids);
    std::string sql = std::format(R"(
        SELECT {}
        FROM profiles
        WHERE id IN ({})
        ORDER BY seq_num;
    )", Db::join_strings(get_profile_columns(), ", "), Db::make_placeholders(ids.size()));

    DbStmt stmt;
    VERIFY(m_db->query(sql, args, &stmt));

    while (stmt.next()) {
        auto profile = ProfileDto{};
        load_profile_from_stmt(stmt, &profile);
        profiles->emplace_back(profile);
    }
    VERIFY(stmt.error());
    SUCCEED();
}

Error
ProfilesDb::create_default_overflow_menu_items(ProfileId profile_id, OverflowMenuItemsChange* change)
{
    std::vector<orion::OverflowMenuItemDto> items{
        {
            .profile_id = profile_id,
            .action = "summarize_page",
            .idx = 1,
        },
        {
            .profile_id = profile_id,
            .action = "programmable_button",
            .idx = 2,
        },
    };

    for (const auto& item : items) {
        std::string sql = std::format(R"(
            INSERT INTO overflow_menu_items (profile_id, action, idx)
            VALUES (?, ?, ?)
            RETURNING {};
        )", Db::join_strings(get_overflow_menu_item_columns(), ", "));

        std::vector<DbVariant> args = {
            item.profile_id.value, item.action, static_cast<int64_t>(item.idx),
        };

        DbStmt stmt;
        VERIFY(m_db->query(sql, args, &stmt));
        bool has_row = stmt.next();
        VERIFY(stmt.error());
        RAISE_CHECK(has_row);
        if (change) {
            OverflowMenuItemDto created_item{};
            load_overflow_menu_item_from_stmt(stmt, &created_item);
            change->created.push_back(created_item);
        }
    }
    SUCCEED();
}

Error
ProfilesDb::create_profile(
    CreateProfileQuery query, ProfileId* profile_id, BookmarkFoldersChange* bookmark_folders_change,
    ProfilesChange* profiles_change, OverflowMenuItemsChange* overflow_menu_items_change,
    ToolbarItemsChange* toolbar_items_change, ProgrammableButtonsChange* prog_buttons_change)
{
    // set seq_num to max(seq_num) rounded up + 1 (the profile will be the last)
    std::string sql = std::format(R"(
        INSERT INTO profiles (name, icon_name, keyboard_shortcut, is_test, seq_num)
        VALUES (?, ?, ?, ?, COALESCE((SELECT CEIL(MAX(seq_num)) + 1 FROM profiles), 1))
        RETURNING {};
    )", Db::join_strings(get_profile_columns(), ", "));

    std::vector<DbVariant> args = {
        query.name,
        query.icon_name,
        query.keyboard_shortcut,
        query.is_test,
    };

    DbStmt stmt;
    VERIFY(m_db->query(sql, args, &stmt));

    bool has_row = stmt.next();
    VERIFY(stmt.error());
    RAISE_CHECK(has_row);

    ProfileId id = ProfileId(stmt.integer("id"));
    if (profile_id) {
        *profile_id = id;
    }

    ProfileDto profile{};
    load_profile_from_stmt(stmt, &profile);

    if (profiles_change) {
        profiles_change->created.emplace_back(profile);
    }

    // create bookmarks bar and favorites folders for the new profile
    CreateBookmarkFolderQuery folder_query{};
    folder_query.profile_id = ProfileId(id);
    folder_query.name = "Bookmarks Bar";
    // leave seq_num empty, it will be set to the end of the folder list
    folder_query.parent_folder_id = BookmarkFolderId(0); // root level
    create_bookmark_folder_internal(folder_query, true, BookmarkFolderType::bookmarks_bar, nullptr, bookmark_folders_change);

    folder_query.name = "Favorites";
    create_bookmark_folder_internal(folder_query, true, BookmarkFolderType::favorites, nullptr, bookmark_folders_change);

    //create a window that will hold pinned tabs shared between unsaved windows
    {
        // This special Shared Pinned Tab Group is never private (private windows cannot have pinned tabs)
        CreateWindowQuery window_query {};
        window_query.profile_id = profile.id;
        WindowId window_id;
        create_window(window_query, &window_id, NULL);
        
        std::string sql = std::format(R"(
            UPDATE profiles
            SET shared_pinned_tabs_window_id = ?
            WHERE id = ?
            RETURNING {};
        )", Db::join_strings(get_profile_columns(), ", "));

        
        std::vector<DbVariant> args = {
            window_id.value,
            profile.id.value,
        };

        DbStmt stmt;
        VERIFY(m_db->exec(sql, args));
    }

    // Set default settings in new profile
    VERIFY(set_profile_default_settings(id));
    VERIFY(create_default_overflow_menu_items(id, overflow_menu_items_change));
    VERIFY(create_default_toolbar_items(id, TxMode::no_tx, toolbar_items_change, prog_buttons_change));
    SUCCEED();
}

Error
ProfilesDb::set_profile_default_settings(ProfileId profile_id)
{
    if (profile_id == ProfileId(0)) {
        RAISE(ErrorCode::argument, "profile_id is not set");
    }

    // Set Favorites folder as default bookmark folder for the new profile
    std::string sql = R"(
        INSERT INTO settings (profile_id, key, value, type)
        SELECT f.profile_id, 'settings::dialogs::add_bookmark::default_bookmark_folder_id', f.id, 'integer'
            FROM bookmark_folders f
            WHERE f.profile_id = ?1 AND f.type = 2 -- Favorites is type 2
        ON CONFLICT DO NOTHING;
    )";

    DbStmt stmt;
    VERIFY(m_db->query(sql, {profile_id.value}, &stmt));

    SUCCEED();
}

Error
ProfilesDb::update_profile(UpdateProfileQuery query, ProfilesChange* profiles_change)
{
    if (query.id == ProfileId(0)) {
        RAISE(ErrorCode::argument, "profile_id is not set");
    }

    std::vector<std::string> set_columns;
    std::vector<DbVariant> args;

    if (query.update_name) {
        args.push_back(query.name);
        set_columns.push_back("name");
    }

    if (query.update_icon_name) {
        args.push_back(query.icon_name);
        set_columns.push_back("icon_name");
    }

    if (query.update_show_in_address_bar) {
        args.push_back(query.show_in_address_bar);
        set_columns.push_back("show_in_address_bar");
    }

    if (query.update_show_badge_in_app_icon) {
        args.push_back(query.show_badge_in_app_icon);
        set_columns.push_back("show_badge_in_app_icon");
    }

    if (query.update_keyboard_shortcut) {
        args.push_back(query.keyboard_shortcut);
        set_columns.push_back("keyboard_shortcut");
    }

    if (query.update_seq_num) {
        args.push_back(query.seq_num);
        set_columns.push_back("seq_num");
    }

    if (set_columns.empty()) {
        SUCCEED(); // nothing to update
    }

    args.push_back(query.id.value);

    std::string sql = std::format(R"(
        UPDATE profiles SET {}
        WHERE id = ?
        RETURNING {};
    )", Db::join_into_set_clause(set_columns), Db::join_strings(get_profile_columns(), ", "));

    DbStmt stmt;
    VERIFY(m_db->query(sql, args, &stmt));

    while (stmt.next()) {
        ProfileDto profile{};
        load_profile_from_stmt(stmt, &profile);

        if (profiles_change) {
            profiles_change->updated.emplace_back(profile);
        }
    }
    VERIFY(stmt.error());
    SUCCEED();
}

Error
ProfilesDb::move_profile(MoveProfileQuery query, ProfilesChange* profiles_change)
{
    if (query.profile_id == ProfileId(0)) {
        RAISE(ErrorCode::argument, "profile_id is not set");
    }

    double new_seq_num = 0;
    if (query.next_profile_id == ProfileId(0)) {
        std::string sql = R"(
            SELECT MAX(seq_num) as prev
            FROM profiles;
        )";

        DbStmt stmt{};
        VERIFY(m_db->query(sql, {}, &stmt));
        int rc = SQLITE_OK;
        if (stmt.next(&rc)) {
            auto prev = stmt.real_optional("prev");
            new_seq_num = prev.value_or(0) + 8;
        } else {
            RAISE(ErrorCode::db_step, "failed to execute database step, sqlite error code: %d", rc);
        }
    } else {
        std::string sql = R"(
            SELECT
            i1.seq_num as next,
            (
                SELECT MAX(i2.seq_num)
                FROM profiles i2
                WHERE i2.seq_num < i1.seq_num
            ) as prev
            FROM profiles i1
            WHERE i1.id = ?1;
        )";

        std::vector<DbVariant> args = {query.next_profile_id.value};
        DbStmt stmt;
        VERIFY(m_db->query(sql, args, &stmt));

        if (stmt.next()) {
            double next_seq_num = stmt.real("next");
            auto prev_optional = stmt.real_optional("prev");
            double prev_seq_num = prev_optional.value_or(next_seq_num - 8);
            new_seq_num = (prev_seq_num + next_seq_num) / 2.0;
        } else {
            RAISE(ErrorCode::argument, "failed to find seq nums for inserting before profile %lld", query.next_profile_id.value);
        }
    }

    UpdateProfileQuery update_query{
        .id = query.profile_id,
        .seq_num = new_seq_num,
        .update_seq_num = true,
    };
    VERIFY(update_profile(update_query, profiles_change));
    SUCCEED();
}

Error
ProfilesDb::set_default_profile(ProfileId profile_id, ProfilesChange* profiles_change)
{
    log_verbose("profiles_db: set_default_profile: %lld", profile_id.value);

    if (profile_id == ProfileId(0)) {
        RAISE(ErrorCode::argument, "profile_id is not set");
    }

    {
        // check if the profile exists
        DbRow row;
        Error err = m_db->query_row("SELECT id FROM profiles WHERE id = ?1", {profile_id.value}, &row);
        if (err.code() == ErrorCode::db_no_rows) {
            RAISE(ErrorCode::not_found, "profile with id %lld not found", profile_id.value);
        }
        VERIFY(err);
    }

    // set is_default to true for the given profile and false for all others
    DbTx tx;
    VERIFY(m_db->begin(&tx));

    auto load_record = [this](DbStmt* stmt, ProfilesChange* change) -> Error {
        while (stmt->next()) {
            ProfileDto profile{};
            load_profile_from_stmt(*stmt, &profile);
            if (change) {
                change->updated.emplace_back(profile);
            }
        }
        VERIFY(stmt->error());
        SUCCEED();
    };

    {
        std::string sql = std::format(R"(
            UPDATE profiles SET is_default = 0
            WHERE is_default = 1 AND id != ?1
            RETURNING {};
        )", Db::join_strings(get_profile_columns(), ", "));

        DbStmt stmt;
        VERIFY(m_db->query(sql, {profile_id.value}, &stmt));

        VERIFY(load_record(&stmt, profiles_change));
    }
    {
        std::string sql = std::format(R"(
            UPDATE profiles SET is_default = 1
            WHERE id = ?1
            RETURNING {};
        )", Db::join_strings(get_profile_columns(), ", "));

        DbStmt stmt;
        VERIFY(m_db->query(sql, {profile_id.value}, &stmt));

        VERIFY(load_record(&stmt, profiles_change));
    }
    VERIFY(tx.commit());
    SUCCEED();
}

Error
ProfilesDb::delete_profiles(const std::vector<ProfileId>& ids, ProfilesChange* profiles_change, WindowsChange* windows_change, TabsChange* tabs_change)
{
    // When a default profile is deleted, the only internal profile becomes a default one,
    // see "reassign_default_profile_on_delete" trigger.
    {
        // When a (non-internal) default profile is deleted (internal profile cannot be deleted),
        // we need to notify about the change of the default profile.
        std::string sql = std::format(R"(
            SELECT id FROM profiles
            WHERE is_default = 1 AND id IN ({}) AND is_internal = 0;
        )", Db::make_placeholders(ids.size()));

        DbStmt stmt;
        VERIFY(m_db->query(sql, to_db_variant_vector(ids), &stmt));

        bool has_row = stmt.next();
        VERIFY(stmt.error());

        if (has_row) {
            // The default profile is being deleted, the internal profile will become the new default.
            ProfileId internal_profile_id{0};
            VERIFY(get_internal_profile_id(&internal_profile_id));
            if (profiles_change) {
                VERIFY(get_profiles({internal_profile_id}, &profiles_change->updated));
                // The internal profile will become the default one, but only after the deletion
                // of the current default profile, so we have to set the is_default flag manually here.
                profiles_change->updated.back().is_default = true;
            }
        }
    }

    std::vector<WindowId> window_ids{};
    for(const auto& id : ids)
    {
        VERIFY(get_window_ids({.profile_id=id}, &window_ids));
    }

    VERIFY(delete_windows(std::move(window_ids), orion::TxMode::no_tx, windows_change, tabs_change, profiles_change));

    std::string sql = std::format(R"(
        DELETE FROM profiles
            WHERE id IN ({}) AND is_internal = 0
            RETURNING id;
    )", Db::make_placeholders(ids.size()));

    DbStmt stmt;
    VERIFY(m_db->query(sql, to_db_variant_vector(ids), &stmt));

    while (stmt.next()) {
        auto id = ProfileId(stmt.integer("id"));
        if (profiles_change) {
            profiles_change->deleted.push_back(id);
        }
    }
    VERIFY(stmt.error());
    SUCCEED();
}

Error
ProfilesDb::set_selected_window_id(SetSelectedWindowIdQuery query, ProfilesChange* profiles_change)
{
    if (query.profile_id == ProfileId(0)) {
        RAISE(ErrorCode::argument, "profile_id is not set");
    }
    if (query.window_id == WindowId(0)) {
        RAISE(ErrorCode::argument, "window_id is not set");
    }

    {
        // Check if the profile exists
        std::vector<ProfileDto> profiles;
        VERIFY(get_profiles({query.profile_id}, &profiles));
        if (profiles.empty()) {
            RAISE(ErrorCode::not_found, "profile with id %lld not found", query.profile_id.value);
        }
    }

    {
        // Check if the window exists
        std::vector<WindowDto> windows;
        VERIFY(get_windows({query.window_id}, &windows));
        if (windows.empty()) {
            RAISE(ErrorCode::not_found, "window with id %lld not found", query.window_id.value);
        }
        const WindowDto& window = windows.front();

        // Check if the window belongs to the profile
        if (window.profile_id != query.profile_id) {
            RAISE(ErrorCode::argument, "window with id %lld does not belong to profile with id %lld",
                query.window_id.value,
                query.profile_id.value
            );
        }
    }

    {
        // Check if it changes anything
        WindowId current_window_id{};
        bool exists = false;

        VERIFY(get_selected_window_id(query.profile_id, &current_window_id, &exists, nullptr));
        if (current_window_id == query.window_id) {
            SUCCEED(); // nothing to change
        }
    }
    
    orion::log_verbose("profiles_db: set_selected_window_id: profile_id=%lld, window_id=%lld",
        query.profile_id.value,
        query.window_id.value
    );

    std::string sql = std::format(R"(
        UPDATE profiles
        SET selected_window_id = ?1
        WHERE id = ?2
        RETURNING {};
    )", Db::join_strings(get_profile_columns(), ", "));

    std::vector<DbVariant> args = {
        query.window_id.value,
        query.profile_id.value,
    };

    DbStmt stmt;
    VERIFY(m_db->query(sql, args, &stmt));
    bool has_row = stmt.next();
    VERIFY(stmt.error());
    RAISE_CHECK(has_row);

    if (profiles_change) {
        ProfileDto profile{};
        load_profile_from_stmt(stmt, &profile);
        profiles_change->updated.push_back(profile);
    }
    SUCCEED();
}

Error
ProfilesDb::get_selected_window_id(ProfileId profile_id, WindowId* window_id, bool* exists, bool* is_private)
{
    if (profile_id == ProfileId(0)) {
        RAISE(ErrorCode::argument, "profile_id is not set");
    }
    RAISE_CHECK(window_id);
    RAISE_CHECK(exists);

    std::string sql = R"(
        SELECT selected_window_id FROM profiles WHERE id = ?1;
    )";
    DbStmt stmt;
    VERIFY(m_db->query(sql, {profile_id.value}, &stmt));
    bool has_row = stmt.next();
    VERIFY(stmt.error());
    if (!has_row) {
        *window_id = WindowId(0);
        *exists = false;
        *is_private = false;
        SUCCEED();
    }

    *window_id = WindowId(stmt.integer("selected_window_id"));

    // Check if the window_id exists
    std::vector<WindowDto> windows;
    VERIFY(get_windows({*window_id}, &windows));
    *exists = !windows.empty();
    
    if (is_private) {
        if (windows.empty()) {
            *is_private = false;
        } else {
            *is_private = windows.front().is_private;
        }
    }
    SUCCEED();
}

Error
ProfilesDb::reset_selected_window_ids(const std::vector<WindowId>& window_ids, ProfilesChange* profiles_change)
{
    if (window_ids.empty()) {
        SUCCEED(); // nothing to do
    }

    std::string sql = std::format(R"(
        UPDATE profiles
        SET selected_window_id = 0
        WHERE selected_window_id IN ({})
        RETURNING {};
    )", Db::make_placeholders(window_ids.size()),
        Db::join_strings(get_profile_columns(), ", ")
    );

    DbStmt stmt;
    VERIFY(m_db->query(sql, to_db_variant_vector(window_ids), &stmt));

    while (stmt.next()) {
        ProfileDto profile{};
        load_profile_from_stmt(stmt, &profile);
        if (profiles_change) {
            profiles_change->updated.emplace_back(profile);
        }
    }
    VERIFY(stmt.error());
    SUCCEED();
}

Error
ProfilesDb::window_id_get_profile_id(const WindowId window_id, ProfileId* profile_id)
{
    {
        std::string sql = R"(
            SELECT profile_id FROM windows
            WHERE id = ?1;
        )";

        DbStmt stmt;
        VERIFY(m_db->query(sql, {window_id.value}, &stmt));

        if (stmt.next()) {
            *profile_id = ProfileId(stmt.integer("profile_id"));
        }
        VERIFY(stmt.error());
    }
    SUCCEED();
}

Error
ProfilesDb::profile_id_get_shared_pinned_tabs_window_id(const ProfileId profile_id, WindowId* window_id)
{
    
    std::string sql = R"(
        SELECT shared_pinned_tabs_window_id FROM profiles
        WHERE id = ?1;
    )";

    DbStmt stmt;
    VERIFY(m_db->query(sql, {profile_id.value}, &stmt));

    WindowId tmp_id;
    if (stmt.next()) {
        tmp_id = WindowId(stmt.integer("shared_pinned_tabs_window_id"));

        if(tmp_id == WindowId(0))
        {
            // This special Shared Pinned Tab Group is never private (private windows cannot have pinned tabs)
            CreateWindowQuery window_query {};
            window_query.profile_id = profile_id;
            VERIFY(create_window(window_query, &tmp_id, NULL));
            
            std::string sql = R"(
                UPDATE profiles
                SET shared_pinned_tabs_window_id = ?1
                WHERE id = ?2;
            )";

            
            std::vector<DbVariant> args = {
                tmp_id.value,
                profile_id.value,
            };

            VERIFY(m_db->exec(sql, args));
        }
        *window_id = tmp_id;
    }
    VERIFY(stmt.error());
    SUCCEED();
}

} // namespace orion