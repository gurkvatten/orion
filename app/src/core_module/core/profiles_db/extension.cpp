#include <core/profiles_db/profiles_db.h>
#include <format>

namespace orion {

//-------------------------------------------------------------------------
// Extensions
//-------------------------------------------------------------------------

Error
ProfilesDb::get_extension_seq_num_map(ProfileId profile_id, std::map<ExtensionId, double>* seq_nums)
{
    if (profile_id == ProfileId(0)) {
        RAISE(ErrorCode::argument, "profile_id is not set");
    }

    if (seq_nums == nullptr) {
        RAISE(ErrorCode::argument, "seq_nums is not set");
    }

    // Preload seq_num of all existing extensions for this profile
    std::string sql = R"(
        SELECT id, seq_num FROM extensions
        WHERE profile_id = ?1
        ORDER BY seq_num;
    )";

    DbStmt stmt;
    VERIFY(m_db->query(sql, {profile_id.value}, &stmt));

    while (stmt.next()) {
        auto seq_num = stmt.real("seq_num");
        (*seq_nums)[static_cast<ExtensionId>(stmt.text("id"))] = seq_num;
    }
    VERIFY(stmt.error());
    SUCCEED();
}

Error
ProfilesDb::install_extension(InstallExtensionQuery query, ExtensionId* id, ExtensionsChange* change)
{
    if (query.profile_id == ProfileId(0)) {
        RAISE(ErrorCode::argument, "profile_id is not set");
    }

    if (query.sku.value.empty()) {
        RAISE(ErrorCode::argument, "sku is not set");
    }

    if (query.type == ExtensionType::not_set) {
        RAISE(ErrorCode::argument, "extension type is not set");
    }

    {
        // Check if the extension is already installed
        std::string sql = R"(
            SELECT sku FROM extensions
            WHERE profile_id = ?1 AND sku = ?2 AND type = ?3;
        )";

        std::vector<DbVariant> args = {
            query.profile_id.value,
            query.sku.value,
            static_cast<int64_t>(query.type),
        };

        DbStmt stmt;
        VERIFY(m_db->query(sql, args, &stmt));

        bool has_row = stmt.next();
        VERIFY(stmt.error());

        if (has_row) {
            RAISE(ErrorCode::argument, "extension with sku %s and type %lld is already installed for profile %lld",
                query.sku.value.c_str(), query.type, query.profile_id.value
            );
        }
    }

    // Preload seq_num of all existing extensions for this profile
    std::map<ExtensionId, double> old_seq_nums;
    VERIFY(get_extension_seq_num_map(query.profile_id, &old_seq_nums));

    std::vector<DbVariant> args = {
        query.profile_id.value,
        query.sku.value,
        static_cast<int64_t>(query.type),
        query.file_path,
        query.name,
        query.description,
        query.icon_ref
    };

    std::string sql = std::format(R"(
        INSERT INTO extensions (profile_id, sku, type, file_path, name, description, icon_ref)
        VALUES ({})
        RETURNING id, profile_id, sku, file_path, name, description, icon_ref, is_enabled, last_updated_at,
            activate_shortcut, lock_shortcut, type, version, do_not_update, has_options;
    )", Db::make_placeholders(args.size()));

    DbStmt stmt;
    VERIFY(m_db->query(sql, args, &stmt));

    bool has_row = stmt.next();
    VERIFY(stmt.error());
    RAISE_CHECK(has_row);

    if (id) {
        *id = ExtensionId(stmt.text("id"));
    }

    ExtensionDto extension{};
    extension.id = ExtensionId(stmt.text("id"));
    extension.profile_id = ProfileId(stmt.integer("profile_id"));
    extension.sku = ExtensionSku(stmt.text("sku"));
    extension.file_path = stmt.text("file_path");
    extension.name = stmt.text("name");
    extension.description = stmt.text("description");
    extension.icon_ref = stmt.text("icon_ref");
    extension.is_enabled = stmt.integer("is_enabled");
    extension.last_updated_at = UnixTimeUtc64(stmt.integer("last_updated_at"));
    extension.activate_shortcut = stmt.text("activate_shortcut");
    extension.lock_shortcut = stmt.text("lock_shortcut");
    extension.type = static_cast<ExtensionType>(stmt.integer("type"));
    extension.version = stmt.text("version");
    extension.do_not_update = stmt.integer("do_not_update");
    extension.has_options = stmt.integer("has_options");

    if (change) {
        VERIFY(update_extensions_seq_num(query.profile_id, &extension, old_seq_nums, change));
        change->created.emplace_back(extension);
    }

    SUCCEED();
}

Error
ProfilesDb::update_extensions_seq_num(
    ProfileId profile_id, ExtensionDto* extension, const std::map<ExtensionId, double>& old_seq_nums,
    ExtensionsChange* change)
{
    if (profile_id == ProfileId(0)) {
        RAISE(ErrorCode::argument, "profile_id is not set");
    }

    // Update seq_num of the newly installed extension and possibly other extensions in the profile
    std::string sql = R"(
        WITH numbered AS (
            SELECT id, ROW_NUMBER() OVER (PARTITION BY profile_id ORDER BY is_enabled DESC, name) AS rn
            FROM extensions
            WHERE profile_id = ?1
        )
        UPDATE extensions SET seq_num = (
            SELECT rn FROM numbered WHERE numbered.id = extensions.id
        )
        WHERE profile_id = ?1
        RETURNING id, profile_id, sku, file_path, name, description, icon_ref, is_enabled, last_updated_at,
            activate_shortcut, lock_shortcut, type, version, do_not_update, has_options, seq_num;
    )";

    DbStmt stmt;
    VERIFY(m_db->query(sql, {profile_id.value}, &stmt));

    while (stmt.next()) {
        auto id = ExtensionId(stmt.text("id"));
        auto new_seq_num = stmt.real("seq_num");

        if (extension && id == extension->id) {
            extension->seq_num = new_seq_num;
            // Skip the newly installed extension, we already handled it
            continue;
        }

        auto old_seq_num = old_seq_nums.find(id);
        if (old_seq_num == old_seq_nums.end()) {
            continue;
        }

        if (new_seq_num != old_seq_num->second) {
            if (change) {
                ExtensionDto update{};
                update.id = id;
                update.profile_id = ProfileId(stmt.integer("profile_id"));
                update.sku = ExtensionSku(stmt.text("sku"));
                update.file_path = stmt.text("file_path");
                update.name = stmt.text("name");
                update.description = stmt.text("description");
                update.icon_ref = stmt.text("icon_ref");
                update.is_enabled = stmt.integer("is_enabled");
                update.last_updated_at = UnixTimeUtc64(stmt.integer("last_updated_at"));
                update.activate_shortcut = stmt.text("activate_shortcut");
                update.lock_shortcut = stmt.text("lock_shortcut");
                update.type = static_cast<ExtensionType>(stmt.integer("type"));
                update.version = stmt.text("version");
                update.do_not_update = stmt.integer("do_not_update");
                update.has_options = stmt.integer("has_options");
                update.seq_num = new_seq_num;

                change->updated.emplace_back(update);
                change->updated_seq_num = true; // at least one extension's seq_num was updated
            }
        }
    }
    VERIFY(stmt.error());
    SUCCEED();
}

Error
ProfilesDb::get_installed_extension_ids(GetInstalledExtensionIdsQuery query, std::vector<ExtensionId>* ids)
{
    if (query.profile_id == ProfileId(0)) {
        RAISE(ErrorCode::argument, "profile_id is not set");
    }

    std::string sql = R"(
        SELECT id FROM extensions
        WHERE profile_id = ?1
        ORDER BY seq_num;
    )";

    DbStmt stmt;
    VERIFY(m_db->query(sql, {query.profile_id.value}, &stmt));

    while (stmt.next()) {
        ids->emplace_back(ExtensionId(stmt.text("id")));
    }
    VERIFY(stmt.error());
    SUCCEED();
}

Error
ProfilesDb::get_installed_extensions(GetInstalledExtensionsQuery query, std::vector<ExtensionDto>* extensions)
{
    std::vector<DbVariant> args;

    for (const auto& id : query.ids) {
        args.push_back(id.value);
    }

    std::string sql = std::format(R"(
        SELECT id, profile_id, sku, type, file_path, name, description, icon_ref, is_enabled, last_updated_at,
            activate_shortcut, lock_shortcut, version, do_not_update, has_options
        FROM extensions
        WHERE id IN ({})
        ORDER BY profile_id, seq_num;
    )", Db::make_placeholders(args.size()));

    DbStmt stmt;
    VERIFY(m_db->query(sql, args, &stmt));

    while (stmt.next()) {
        ExtensionDto extension{};
        extension.id = ExtensionId(stmt.text("id"));
        extension.profile_id = ProfileId(stmt.integer("profile_id"));
        extension.sku = ExtensionSku(stmt.text("sku"));
        extension.type = static_cast<ExtensionType>(stmt.integer("type"));
        extension.file_path = stmt.text("file_path");
        extension.name = stmt.text("name");
        extension.description = stmt.text("description");
        extension.icon_ref = stmt.text("icon_ref");
        extension.is_enabled = stmt.integer("is_enabled");
        extension.last_updated_at = UnixTimeUtc64(stmt.integer("last_updated_at"));
        extension.activate_shortcut = stmt.text("activate_shortcut");
        extension.lock_shortcut = stmt.text("lock_shortcut");
        extension.version = stmt.text("version");
        extension.do_not_update = stmt.integer("do_not_update");
        extension.has_options = stmt.integer("has_options");

        extensions->emplace_back(extension);
    }
    VERIFY(stmt.error());
    SUCCEED();
}

Error
ProfilesDb::uninstall_extension(UninstallExtensionQuery query, ExtensionsChange* change)
{
    if (query.id.value.empty()) {
        RAISE(ErrorCode::argument, "extension id is not set");
    }

    std::string sql = R"(
        DELETE FROM extensions
        WHERE id = ?1
        RETURNING id;
    )";

    std::vector<DbVariant> args = {
        query.id.value,
    };

    DbStmt stmt;
    VERIFY(m_db->query(sql, args, &stmt));

    bool has_row = stmt.next();
    VERIFY(stmt.error());

    if (!has_row) {
        RAISE(ErrorCode::not_found, "extension id %s not found",
            query.id.value.c_str()
        );
    }

    // Session storage is not part of the persistent database and therefore
    // cannot use the extensions table's ON DELETE CASCADE.
    VERIFY(extension_session_storage_clear(query.id));

    if (change) {
        change->deleted.emplace_back(stmt.text("id"));
    }

    SUCCEED();
}

Error
ProfilesDb::update_extension(UpdateExtensionQuery query, ExtensionsChange* change)
{
    if (query.id.value.empty()) {
        RAISE(ErrorCode::argument, "extension id is not set");
    }

    // Check if the extension exists
    ProfileId profile_id = query.profile_id;
    {
        std::string sql = R"(
            SELECT id, profile_id FROM extensions
            WHERE id = ?1;
        )";

        DbStmt stmt;
        VERIFY(m_db->query(sql, {query.id.value}, &stmt));

        bool has_row = stmt.next();
        VERIFY(stmt.error());

        if (!has_row) {
            RAISE(ErrorCode::not_found, "extension with id %s not found",
                query.id.value.c_str()
            );
        }

        profile_id = ProfileId(stmt.integer("profile_id"));
    }

    // Preload seq_num of all existing extensions for this profile
    std::map<ExtensionId, double> old_seq_nums;
    VERIFY(get_extension_seq_num_map(profile_id, &old_seq_nums));

    if (change) {
        change->updated_profile_id = query.update_profile_id;
        change->updated_sku = query.update_sku;
        change->updated_type = query.update_type;
        change->updated_file_path = query.update_file_path;
        change->updated_name = query.update_name;
        change->updated_description = query.update_description;
        change->updated_icon_ref = query.update_icon_ref;
        change->updated_is_enabled = query.update_is_enabled;
        change->updated_last_updated_at = query.update_last_updated_at;
        change->updated_activate_shortcut = query.update_activate_shortcut;
        change->updated_lock_shortcut = query.update_lock_shortcut;
        change->updated_version = query.update_version;
        change->updated_do_not_update = query.update_do_not_update;
        change->updated_has_options = query.update_has_options;
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

    if (query.update_sku) {
        if (query.sku.value.empty()) {
            RAISE(ErrorCode::argument, "sku is not set");
        }
        args.push_back(query.sku.value);
        set_columns.push_back("sku");
    }

    if (query.update_type) {
        if (query.type == ExtensionType::not_set) {
            RAISE(ErrorCode::argument, "extension type is not set");
        }
        args.push_back(static_cast<int64_t>(query.type));
        set_columns.push_back("type");
    }

    if (query.update_file_path) {
        args.push_back(query.file_path);
        set_columns.push_back("file_path");
    }

    if (query.update_name) {
        args.push_back(query.name);
        set_columns.push_back("name");
    }

    if (query.update_description) {
        args.push_back(query.description);
        set_columns.push_back("description");
    }

    if (query.update_icon_ref) {
        args.push_back(query.icon_ref);
        set_columns.push_back("icon_ref");
    }

    if (query.update_is_enabled) {
        args.push_back(query.is_enabled);
        set_columns.push_back("is_enabled");
    }

    if (query.update_last_updated_at) {
        args.push_back(query.last_updated_at.value);
        set_columns.push_back("last_updated_at");
    }

    if (query.update_activate_shortcut) {
        args.push_back(query.activate_shortcut);
        set_columns.push_back("activate_shortcut");
    }

    if (query.update_lock_shortcut) {
        args.push_back(query.lock_shortcut);
        set_columns.push_back("lock_shortcut");
    }

    if (query.update_version) {
        args.push_back(query.version);
        set_columns.push_back("version");
    }

    if (query.update_do_not_update) {
        args.push_back(query.do_not_update);
        set_columns.push_back("do_not_update");
    }

    if (query.update_has_options) {
        args.push_back(query.has_options);
        set_columns.push_back("has_options");
    }

    if (set_columns.empty()) {
        SUCCEED(); // nothing to update
    }

    args.push_back(query.id.value);

    std::string sql = std::format(R"(
        UPDATE extensions SET {}
        WHERE id = ?
        RETURNING id, profile_id, sku, type, file_path, name, description, icon_ref, is_enabled, last_updated_at,
            activate_shortcut, lock_shortcut, version, do_not_update, has_options;
    )", Db::join_into_set_clause(set_columns));

    DbStmt stmt;
    VERIFY(m_db->query(sql, args, &stmt));

    bool has_row = stmt.next();
    VERIFY(stmt.error());

    if (!has_row) {
        RAISE(ErrorCode::not_found, "extension with id %s ",
            query.id.value.c_str()
        );
    }

    ExtensionDto extension{};
    extension.id = ExtensionId(stmt.text("id"));
    extension.profile_id = ProfileId(stmt.integer("profile_id"));
    extension.sku = ExtensionSku(stmt.text("sku"));
    extension.type = static_cast<ExtensionType>(stmt.integer("type"));
    extension.file_path = stmt.text("file_path");
    extension.name = stmt.text("name");
    extension.description = stmt.text("description");
    extension.icon_ref = stmt.text("icon_ref");
    extension.is_enabled = stmt.integer("is_enabled");
    extension.last_updated_at = UnixTimeUtc64(stmt.integer("last_updated_at"));
    extension.activate_shortcut = stmt.text("activate_shortcut");
    extension.lock_shortcut = stmt.text("lock_shortcut");
    extension.version = stmt.text("version");
    extension.do_not_update = stmt.integer("do_not_update");
    extension.has_options = stmt.integer("has_options");

    const bool disabled = query.update_is_enabled && !extension.is_enabled;
    const bool resources_changed = query.update_file_path || query.update_type;
    if (disabled || resources_changed) {
        VERIFY(extension_session_storage_clear(extension.id));
    }

    if (change) {
        VERIFY(update_extensions_seq_num(profile_id, &extension, old_seq_nums, change));
        change->updated.emplace_back(extension);
    }

    SUCCEED();
}

} // namespace orion
