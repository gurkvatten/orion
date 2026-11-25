#include <core/profiles_db/profiles_db.h>
#include <format>

#include <core/settings.h>

#include <common/src/string_util.h>

namespace orion {

//-------------------------------------------------------------------------
// Settings
//-------------------------------------------------------------------------

Error
ProfilesDb::set_setting(
    const SetSettingQuery& query, std::string* old_value, SettingsChange* settings_change,
    SingleSettingAnyChange* single_change)
{
    log_verbose("ProfilesDb::set_setting: profile_id=%lld key=%s value=%s default_value=%s",
        query.profile_id.value,
        query.key.c_str(),
        query.value.c_str(),
        query.default_value.c_str()
    );

    // Get existing value and type first (if exists)
    bool has_row = false;
    std::string existing_value;
    std::string existing_type;
    {
        std::string sql = R"(
            SELECT value, type FROM settings
            WHERE profile_id = ?1 AND key = ?2;
        )";

        std::vector<DbVariant> args = {
            query.profile_id.value,
            query.key,
        };

        DbStmt stmt;
        VERIFY(m_db->query(sql, args, &stmt));
        has_row = stmt.next();
        VERIFY(stmt.error());
        
        if (has_row) {
            existing_value = stmt.text("value");
            existing_type = stmt.text("type");
        } else {
            existing_value = query.default_value; // Default value if no row exists
            existing_type = query.type;           // Use the type from the query
        }
    }

    if (old_value) {
        *old_value = existing_value; // Store the old value if requested
    }

    if (has_row) {
        if (existing_type == query.type && existing_value == query.value) {
            // No change in value or type, nothing to update or notify
            SUCCEED();
        }
    }

    // Insert a new row or update the existing row
    {
        std::string sql = R"(
            INSERT INTO settings (profile_id, key, type, value)
            VALUES (?, ?, ?, ?)
            ON CONFLICT(profile_id, key) DO UPDATE SET
                type = excluded.type,
                value = excluded.value;
        )";

        std::vector<DbVariant> args{
            query.profile_id.value,
            query.key,
            query.type,
            query.value,
        };

        VERIFY(m_db->exec(sql, args));
    }

    if (single_change) {
        single_change->profile_id = query.profile_id;
        single_change->key = query.key;
        single_change->realm = query.realm;
        single_change->key_name = query.key_name;
        single_change->type = query.type;
        single_change->default_value = query.default_value;
        single_change->old_value = existing_value;
        single_change->new_value = query.value;
    }

    if (settings_change) {        
        SingleSettingAnyChange change{};
        change.profile_id = query.profile_id;
        change.key = query.key;
        change.realm = query.realm;
        change.key_name = query.key_name;
        change.type = query.type;
        change.default_value = query.default_value;
        change.old_value = existing_value;
        change.new_value = query.value;
        settings_change->updated.push_back(change);
    }

    SUCCEED();
}

Error
ProfilesDb::set_setting_or_default(
    const SetSettingOrDefaultQuery& query,
    std::string* old_value,
    std::string* default_value,
    SettingsChange* settings_change,
    SingleSettingAnyChange* single_change)
{
    RAISE_CHECK(old_value);
    RAISE_CHECK(default_value);

    if (query.profile_id == ProfileId(0)) {
        RAISE(ErrorCode::argument, "profile_id is not set");
    }

    log_verbose("ProfilesDb::set_setting_or_default: profile_id=%lld key=%s default_setting_key=%s",
        query.profile_id.value,
        query.key.c_str(),
        query.default_setting_key.c_str()
    );

    // Resolve the default value by reading default_setting from the database.
    std::string sql_default = R"(
        SELECT value, type FROM settings
        WHERE profile_id = ?1 AND key = ?2;
    )";

    std::vector<DbVariant> default_args = {
        query.profile_id.value,
        query.default_setting_key,
    };

    DbStmt default_stmt;
    VERIFY(m_db->query(sql_default, default_args, &default_stmt));
    bool has_default_row = default_stmt.next();
    VERIFY(default_stmt.error());

    std::string resolved_default;
    if (!has_default_row) {
        resolved_default = query.default_value;
    } else {
        std::string db_default_type = default_stmt.text("type");
        if (query.default_setting_type != db_default_type) {
            RAISE(ErrorCode::argument, "expected type %s of key %s, got %s from database",
                query.default_setting_type.c_str(), query.default_setting_key.c_str(), db_default_type.c_str()
            );
        }
        resolved_default = default_stmt.text("value");
    }
    *default_value = resolved_default;

    // Now set the setting using the resolved default.
    SetSettingQuery set_query{};
    set_query.profile_id = query.profile_id;
    set_query.key = query.key;
    set_query.realm = query.realm;
    set_query.key_name = query.key_name;
    set_query.type = query.type;
    set_query.value = query.value;
    set_query.default_value = resolved_default;

    VERIFY(set_setting(set_query, old_value, settings_change, single_change));
    SUCCEED();
}

Error
ProfilesDb::increment_and_clamp_setting(
    const IncrementAndClampSettingQuery& query, int64_t* new_value, int64_t* old_value,
    SettingsChange* settings_change, SingleSettingAnyChange* single_change)
{
    log_verbose("ProfilesDb::increment_and_clamp_setting: profile_id=%lld key=%s by=%li lower=%li upper=%li default_value=%li",
        query.profile_id.value,
        query.key.c_str(),
        query.by,
        query.lower,
        query.upper,
        query.default_value
    );
    if(query.by == 0)
    {
        SUCCEED(); //should probably never be called like this
    }

    // Get existing value and type first (if exists)
    bool has_row = false;
    int64_t existing_value;
    std::string existing_value_string;
    std::string existing_type;
    {
        std::string sql = R"(
            SELECT value, type FROM settings
            WHERE profile_id = ?1 AND key = ?2;
        )";

        std::vector<DbVariant> args = {
            query.profile_id.value,
            query.key,
        };

        DbStmt stmt;
        VERIFY(m_db->query(sql, args, &stmt));
        has_row = stmt.next();
        VERIFY(stmt.error());
        
        if (has_row) {
            existing_type = stmt.text("type");
            if(existing_type != "integer")
            {
                RAISE(ErrorCode::invalid_type, "attempted to increment non integer setting");
            }
            existing_value_string = stmt.text("value");
            VERIFY(settings::unmarshal_value(existing_value_string, &existing_value));
        } else {
            existing_value_string = settings::marshal_value(query.default_value);
            existing_value = query.default_value; // Default value if no row exists
            existing_type = "integer";           // Use the type from the query
        }
    }

    if (old_value) {
        *old_value = existing_value; // Store the old value if requested
    }

    if(existing_value == query.unset_value) {
        existing_value = query.starting_value;
    }

    int64_t new_value_internal = std::clamp(existing_value + query.by, query.lower, query.upper);
    std::string new_value_string = std::to_string(new_value_internal);

    if (has_row) {
        if (existing_type == "integer" &&
            (query.by > 0)?(existing_value == query.upper):(existing_value == query.lower) &&
            !(query.delete_if_eq_starting && new_value_internal == query.starting_value))
        {
            // No change in value or type, nor deleting, nothing to update or notify
            
            SUCCEED();
        }
    }
    

    if(query.delete_if_eq_starting && new_value_internal == query.starting_value)
    {
        if(new_value) *new_value = query.default_value;

        new_value_string = std::to_string(query.default_value);
        std::string sql = R"(
            DELETE FROM settings
            WHERE profile_id = ?1 AND key = ?2;
        )"; 

        std::vector<DbVariant> args{
            query.profile_id.value,
            query.key
        };

        VERIFY(m_db->exec(sql, args));
    }
    else
    {
        // Insert a new row or update the existing row
        if(new_value) *new_value = new_value_internal;
        std::string sql = R"(
            INSERT INTO settings (profile_id, key, type, value)
            VALUES (?, ?, ?, ?)
            ON CONFLICT(profile_id, key) DO UPDATE SET
                type = excluded.type,
                value = excluded.value;
        )";

        std::vector<DbVariant> args{
            query.profile_id.value,
            query.key,
            "integer",
            std::move(new_value_string),
        };

        VERIFY(m_db->exec(sql, args));
    }

    if (single_change) {
        single_change->profile_id = query.profile_id;
        single_change->key = query.key;
        single_change->realm = query.realm;
        single_change->key_name = query.key_name;
        single_change->type = "integer";
        single_change->default_value = query.default_value;
        single_change->old_value = existing_value_string;
        single_change->new_value = new_value_string;
    }

    if (settings_change) {        
        SingleSettingAnyChange change{};
        change.profile_id = query.profile_id;
        change.key = query.key;
        change.realm = query.realm;
        change.key_name = query.key_name;
        change.type = "integer";
        change.default_value = query.default_value;
        change.old_value = existing_value_string;
        change.new_value = new_value_string;
        settings_change->updated.push_back(change);
    }

    SUCCEED();
}

Error
ProfilesDb::append_to_string_setting(
    const AppendToStringSettingQuery& query,
    const TxMode tx_mode,
    std::string* old_value,
    std::string* default_value,
    SettingsChange* settings_change,
    SingleSettingAnyChange* single_change)
{
    log_verbose("ProfilesDb::append_to_string_setting: profile_id=%lld key=%s value=%s default_value=%s",
        (long long)query.profile_id.value,
        query.key.c_str(),
        query.value.c_str(),
        query.default_value.c_str()
    );
    if(query.value.length() == 0)
    {
        SUCCEED(); //should probably never be called like this
    }

    DbTx tx;
    if (tx_mode == TxMode::use_tx) {
        VERIFY(tx.begin(m_db));
    }

    // Get existing value and type first (if exists)
    bool has_row = false;
    std::string existing_value;
    std::string existing_type;
    {
        std::string sql = R"(
            SELECT value, type FROM settings
            WHERE profile_id = ?1 AND key = ?2;
        )";

        std::vector<DbVariant> args = {
            query.profile_id.value,
            query.key,
        };

        DbStmt stmt;
        VERIFY(m_db->query(sql, args, &stmt));
        has_row = stmt.next();
        VERIFY(stmt.error());
        
        if (has_row) {
            existing_type = stmt.text("type");
            if(existing_type != "text")
            {
                RAISE(ErrorCode::invalid_type, "attempted to append string to non text setting");
            }
            std::string value_text = stmt.text("value");
            VERIFY(settings::unmarshal_value(value_text, &existing_value));
        } else {
            existing_value = query.default_value; // Default value if no row exists
            existing_type = "text";           // Use the type from the query
        }
    }

    if (old_value) {
        *old_value = existing_value; // Store the old value if requested
    }

    std::string new_value = existing_value + query.value;
    // Insert a new row or update the existing row
    {
        std::string sql = R"(
            INSERT INTO settings (profile_id, key, type, value)
            VALUES (?, ?, ?, ?)
            ON CONFLICT(profile_id, key) DO UPDATE SET
                type = excluded.type,
                value = excluded.value;
        )";

        std::vector<DbVariant> args{
            query.profile_id.value,
            query.key,
            "text",
            new_value,
        };

        VERIFY(m_db->exec(sql, args));
    }

    if (single_change) {
        single_change->profile_id = query.profile_id;
        single_change->key = query.key;
        single_change->realm = query.realm;
        single_change->key_name = query.key_name;
        single_change->type = "integer";
        single_change->default_value = query.default_value;
        single_change->old_value = existing_value;
        single_change->new_value = new_value;
    }

    if (settings_change) {        
        SingleSettingAnyChange change{};
        change.profile_id = query.profile_id;
        change.key = query.key;
        change.realm = query.realm;
        change.key_name = query.key_name;
        change.type = "integer";
        change.default_value = query.default_value;
        change.old_value = existing_value;
        change.new_value = new_value;
        settings_change->updated.push_back(change);
    }


    if (tx_mode == TxMode::use_tx) {
        VERIFY(tx.commit());
    }
    SUCCEED();
}

Error
ProfilesDb::remove_all_occurrences_of_string_from_setting(
    const RemoveAllOccurrencesOfStringFromSettingQuery& query,
    const TxMode tx_mode,
    std::string* old_value,
    std::string* default_value,
    SettingsChange* settings_change,
    SingleSettingAnyChange* single_change
)
{
    log_verbose("ProfilesDb::remove_all_occurences_of_string_from_setting: profile_id=%lld key=%s substring=%s default_value=%s",
        (long long)query.profile_id.value,
        query.key.c_str(),
        query.substring.c_str(),
        query.default_value.c_str()
    );
    if(query.substring.length() == 0)
    {
        SUCCEED(); //should probably never be called like this
    }

    DbTx tx;
    if (tx_mode == TxMode::use_tx) {
        VERIFY(tx.begin(m_db));
    }

    // Get existing value and type first (if exists)
    bool has_row = false;
    std::string existing_value;
    std::string existing_type;
    {
        std::string sql = R"(
            SELECT value, type FROM settings
            WHERE profile_id = ?1 AND key = ?2;
        )";

        std::vector<DbVariant> args = {
            query.profile_id.value,
            query.key,
        };

        DbStmt stmt;
        VERIFY(m_db->query(sql, args, &stmt));
        has_row = stmt.next();
        VERIFY(stmt.error());
        
        if (has_row) {
            existing_type = stmt.text("type");
            if(existing_type != "text")
            {
                RAISE(ErrorCode::invalid_type, "attempted to remove substrings from non text setting");
            }
            std::string value_text = stmt.text("value");
            VERIFY(settings::unmarshal_value(value_text, &existing_value));
        } else {
            existing_value = query.default_value; // Default value if no row exists
            existing_type = "text";           // Use the type from the query
        }
    }

    if (old_value) {
        *old_value = existing_value; // Store the old value if requested
    }

    std::string new_value = orion::str::remove_all_occurrences_of_substring_in_string(existing_value, query.substring);
    // Insert a new row or update the existing row
    {
        std::string sql = R"(
            INSERT INTO settings (profile_id, key, type, value)
            VALUES (?, ?, ?, ?)
            ON CONFLICT(profile_id, key) DO UPDATE SET
                type = excluded.type,
                value = excluded.value;
        )";

        std::vector<DbVariant> args{
            query.profile_id.value,
            query.key,
            "text",
            new_value,
        };

        VERIFY(m_db->exec(sql, args));
    }

    if (single_change) {
        single_change->profile_id = query.profile_id;
        single_change->key = query.key;
        single_change->realm = query.realm;
        single_change->key_name = query.key_name;
        single_change->type = "integer";
        single_change->default_value = query.default_value;
        single_change->old_value = existing_value;
        single_change->new_value = new_value;
    }

    if (settings_change) {        
        SingleSettingAnyChange change{};
        change.profile_id = query.profile_id;
        change.key = query.key;
        change.realm = query.realm;
        change.key_name = query.key_name;
        change.type = "integer";
        change.default_value = query.default_value;
        change.old_value = existing_value;
        change.new_value = new_value;
        settings_change->updated.push_back(change);
    }


    if (tx_mode == TxMode::use_tx) {
        VERIFY(tx.commit());
    }
    SUCCEED();
}

Error
ProfilesDb::set_common_setting(
    const SetCommonSettingQuery& query,
    std::string* old_value,
    CommonSettingsChange* settings_change,
    SingleCommonSettingAnyChange* single_change)
{
    log_verbose("ProfilesDb::set_common_setting: key=%s value=%s default_value=%s",
        query.key.c_str(),
        query.value.c_str(),
        query.default_value.c_str()
    );

    // Get existing value and type first (if exists)
    bool has_row = false;
    std::string existing_value;
    std::string existing_type;
    {
        std::string sql = R"(
            SELECT value, type FROM common_settings
            WHERE key = ?1;
        )";

        std::vector<DbVariant> args{
            query.key,
        };

        DbStmt stmt;
        VERIFY(m_db->query(sql, args, &stmt));
        has_row = stmt.next();
        VERIFY(stmt.error());

        if (has_row) {
            existing_value = stmt.text("value");
            existing_type = stmt.text("type");
        } else {
            existing_value = query.default_value; // Default value if no row exists
            existing_type = query.type;           // Use the type from the query
        }
    }

    if (old_value) {
        *old_value = existing_value; // Store the old value if requested
    }

    if (has_row) {
        if (existing_type == query.type && existing_value == query.value) {
            // No change in value or type, nothing to update or notify
            SUCCEED();
        }
    }

    // Insert a new row or update the existing row
    {
        std::string sql = R"(
            INSERT INTO common_settings (key, type, value)
            VALUES (?, ?, ?)
            ON CONFLICT(key) DO UPDATE SET
                type = excluded.type,
                value = excluded.value;
        )";

        std::vector<DbVariant> args{
            query.key,
            query.type,
            query.value,
        };

        VERIFY(m_db->exec(sql, args));
    }

    if (single_change) {
        single_change->key = query.key;
        single_change->realm = query.realm;
        single_change->key_name = query.key_name;
        single_change->type = query.type;
        single_change->default_value = query.default_value;
        single_change->old_value = existing_value;
        single_change->new_value = query.value;
    }

    if (settings_change) {        
        SingleCommonSettingAnyChange change{
            .key = query.key,
            .realm = query.realm,
            .key_name = query.key_name,
            .type = query.type,
            .default_value = query.default_value,
            .old_value = existing_value,
            .new_value = query.value,
        };
        settings_change->updated.push_back(change);
    }

    SUCCEED();
}

Error
ProfilesDb::get_setting(const GetSettingQuery& query, std::string* value)
{
    RAISE_CHECK(value);

    if (query.profile_id == ProfileId(0)) {
        RAISE(ErrorCode::argument, "profile_id is not set");
    }

    log_verbose("ProfilesDb::get_setting: profile_id=%lld key=%s default_value=%s",
        query.profile_id.value,
        query.key.c_str(),
        query.default_value.c_str()
    );

    std::string sql = R"(
        SELECT value, type FROM settings
        WHERE profile_id = ?1 AND key = ?2;
    )";

    std::vector<DbVariant> args = {
        query.profile_id.value,
        query.key,
    };

    DbStmt stmt;
    VERIFY(m_db->query(sql, args, &stmt));

    bool has_row = stmt.next();
    VERIFY(stmt.error());

    if (!has_row) {
        // setting not found - this means we should return the default value
        *value = query.default_value;
        SUCCEED();
    }

    std::string db_type = stmt.text("type");

    if (query.type != db_type) {
        RAISE(ErrorCode::argument, "expected type %s of key %s, got %s from database",
            query.type.c_str(), query.key.c_str(), db_type.c_str()
        );
    }

    *value = stmt.text("value");

    SUCCEED();
}

Error
ProfilesDb::get_setting_or_default(const GetSettingOrDefaultQuery& query, std::string* value, std::string* default_value)
{
    RAISE_CHECK(value);
    RAISE_CHECK(default_value);

    if (query.profile_id == ProfileId(0)) {
        RAISE(ErrorCode::argument, "profile_id is not set");
    }

    log_verbose("ProfilesDb::get_setting_or_default: profile_id=%lld key=%s default_setting_key=%s default_value=%s",
        query.profile_id.value,
        query.key.c_str(),
        query.default_setting_key.c_str(),
        query.default_value.c_str()
    );

    std::string sql = R"(
        SELECT
            setting.value AS setting_value,
            setting.type AS setting_type,
            default_setting.value AS default_setting_value,
            default_setting.type AS default_setting_type
        FROM (SELECT 1)
        LEFT JOIN settings AS setting
            ON setting.profile_id = ?1 AND setting.key = ?2
        LEFT JOIN settings AS default_setting
            ON default_setting.profile_id = ?1 AND default_setting.key = ?3;
    )";

    std::vector<DbVariant> args = {
        query.profile_id.value,
        query.key,
        query.default_setting_key,
    };

    DbStmt stmt;
    VERIFY(m_db->query(sql, args, &stmt));

    bool has_row = stmt.next();
    VERIFY(stmt.error());
    RAISE_CHECK(has_row);

    if (stmt.is_null("default_setting_value")) {
        *default_value = query.default_value;
    } else {
        std::string db_default_type = stmt.text("default_setting_type");
        if (query.default_setting_type != db_default_type) {
            RAISE(ErrorCode::argument, "expected type %s of key %s, got %s from database",
                query.default_setting_type.c_str(), query.default_setting_key.c_str(), db_default_type.c_str()
            );
        }
        *default_value = stmt.text("default_setting_value");
    }

    if (stmt.is_null("setting_value")) {
        *value = *default_value;
        SUCCEED();
    }

    std::string db_type = stmt.text("setting_type");
    if (query.type != db_type) {
        RAISE(ErrorCode::argument, "expected type %s of key %s, got %s from database",
            query.type.c_str(), query.key.c_str(), db_type.c_str()
        );
    }

    *value = stmt.text("setting_value");

    SUCCEED();
}

Error
ProfilesDb::get_common_setting(const GetCommonSettingQuery& query, std::string* value)
{
    RAISE_CHECK(value);

    log_verbose("ProfilesDb::get_common_setting: key=%s default_value=%s",
        query.key.c_str(),
        query.default_value.c_str()
    );

    std::string sql = R"(
        SELECT value, type FROM common_settings
        WHERE key = ?1;
    )";

    std::vector<DbVariant> args = {
        query.key,
    };

    DbStmt stmt;
    VERIFY(m_db->query(sql, args, &stmt));

    bool has_row = stmt.next();
    VERIFY(stmt.error());

    if (!has_row) {
        // setting not found - this means we should return the default value
        *value = query.default_value;
        SUCCEED();
    }

    std::string db_type = stmt.text("type");

    if (query.type != db_type) {
        RAISE(ErrorCode::argument, "expected type %s of key %s, got %s from database",
            query.type.c_str(), query.key.c_str(), db_type.c_str()
        );
    }

    *value = stmt.text("value");

    SUCCEED();
}

Error
ProfilesDb::get_setting_by_key(ProfileId profile_id, const std::string& key, std::optional<RawSettingDto>* out_setting)
{
    RAISE_CHECK(out_setting);

    if (profile_id == ProfileId(0)) {
        RAISE(ErrorCode::argument, "profile_id is not set");
    }

    std::string sql = R"(
        SELECT key, type, value FROM settings
        WHERE profile_id = ?1 AND key = ?2;
    )";

    std::vector<DbVariant> args{
        profile_id.value,
        key,
    };

    DbStmt stmt;
    VERIFY(m_db->query(sql, args, &stmt));

    bool has_row = stmt.next();
    VERIFY(stmt.error());

    if (has_row) {
        *out_setting = RawSettingDto{
            .key = stmt.text("key"),
            .type = stmt.text("type"),
            .value = stmt.text("value"),
        };
    }

    SUCCEED();
}

Error
ProfilesDb::get_settings_by_key_prefix(ProfileId profile_id, const std::string& key_prefix, std::vector<RawSettingDto>* settings)
{
    RAISE_CHECK(settings);

    if (profile_id == ProfileId(0)) {
        RAISE(ErrorCode::argument, "profile_id is not set");
    }

    settings->clear();

    std::string sql = R"(
        SELECT key, type, value FROM settings
        WHERE profile_id = ?1 AND key LIKE ?2
        ORDER BY key COLLATE NOCASE;
    )";

    std::vector<DbVariant> args = {
        profile_id.value,
        key_prefix + "%",
    };

    DbStmt stmt;
    VERIFY(m_db->query(sql, args, &stmt));

    while (stmt.next()) {
        settings->push_back(RawSettingDto{
            .key = stmt.text("key"),
            .type = stmt.text("type"),
            .value = stmt.text("value"),
        });
    }
    VERIFY(stmt.error());

    SUCCEED();
}

namespace {
    struct WebsiteDefaultSettingInfo {
        std::shared_ptr<settings::ISetting> default_isetting{};
        // For bool or enum settings, the integer value that represents "true" or "enabled"
        int64_t enum_value_of_true = -1;
    };
}

static Error
find_website_default_setting_info(const std::string& key_name, WebsiteDefaultSettingInfo* info)
{
    RAISE_CHECK(info);

    if (key_name == settings::website::specific::enable_content_blockers.key_name()) {
        info->default_isetting = settings::websites::content_blockers_setting_for_all_websites.to_isetting();
        info->enum_value_of_true = static_cast<int64_t>(settings::ContentBlockersWebsiteSetting::on);
    } else if (key_name == settings::website::specific::enable_intelligent_tracking_prevention.key_name()) {
        info->default_isetting = settings::websites::tracking_prevention_setting_for_all_websites.to_isetting();
        info->enum_value_of_true = static_cast<int64_t>(settings::TrackingPreventionWebsiteSetting::on);
    } else if (key_name == settings::website::specific::block_web_fonts.key_name()) {
        info->default_isetting = settings::websites::web_fonts_setting_for_all_websites.to_isetting();
        info->enum_value_of_true = static_cast<int64_t>(settings::WebFontsWebsiteSetting::off); // inverted
    } else if (key_name == settings::website::specific::enable_javascript.key_name()) {
        info->default_isetting = settings::websites::javascript_setting_for_all_websites.to_isetting();
        info->enum_value_of_true = static_cast<int64_t>(settings::JavaScriptWebsiteSetting::on);
    } else if (key_name == settings::website::specific::enable_cookies.key_name()) {
        info->default_isetting = settings::websites::cookies_setting_for_all_websites.to_isetting();
        info->enum_value_of_true = static_cast<int64_t>(settings::CookiesWebsiteSetting::on);
    } else if (key_name == settings::website::specific::enable_compatibility_mode.key_name()) {
        info->default_isetting = settings::websites::compatibility_mode_default_setting_for_all_websites.to_isetting();
        info->enum_value_of_true = static_cast<int64_t>(settings::CompatibilityModeWebsiteSetting::on);
    } else if (key_name == settings::website::specific::page_zoom.key_name()) {
        info->default_isetting = settings::websites::page_zoom_default_setting_for_all_websites.to_isetting();
    } else if (key_name == settings::website::specific::autoplay_default_setting.key_name()) {
        info->default_isetting = settings::websites::autoplay_default_setting_for_all_websites.to_isetting();
    } else if (key_name == settings::website::specific::media_key_system_default_setting.key_name()) {
        info->default_isetting = settings::websites::media_key_system_default_setting_for_all_websites.to_isetting();
    } else if (key_name == settings::website::specific::pointer_lock_default_setting.key_name()) {
        info->default_isetting = settings::websites::pointer_lock_default_setting_for_all_websites.to_isetting();
    } else if (key_name == settings::website::specific::website_data_access_default_setting.key_name()) {
        info->default_isetting = settings::websites::website_data_access_default_setting_for_all_websites.to_isetting();
    } else if (key_name == settings::website::specific::user_agent.key_name()) {
        info->default_isetting = settings::websites::user_agent_default_setting_for_all_websites.to_isetting();
    } else if (key_name == settings::website::specific::ssl_certificate_validation_setting.key_name()) {
        info->default_isetting = settings::websites::ssl_certificate_validation_setting_for_all_websites.to_isetting();
    } else if (key_name == settings::website::specific::picture_in_picture_default_setting.key_name()) {
        info->default_isetting = settings::websites::picture_in_picture_default_setting_for_all_websites.to_isetting();
    } else if (key_name == settings::website::specific::camera_default_setting.key_name()) {
        info->default_isetting = settings::websites::camera_default_setting_for_all_websites.to_isetting();
    } else if (key_name == settings::website::specific::microphone_default_setting.key_name()) {
        info->default_isetting = settings::websites::microphone_default_setting_for_all_websites.to_isetting();
    } else if (key_name == settings::website::specific::screen_sharing_default_setting.key_name()) {
        info->default_isetting = settings::websites::screen_sharing_default_setting_for_all_websites.to_isetting();
    } else if (key_name == settings::website::specific::location_default_setting.key_name()) {
        info->default_isetting = settings::websites::location_default_setting_for_all_websites.to_isetting();
    } else if (key_name == settings::website::specific::notifications_default_setting.key_name()) {
        info->default_isetting = settings::websites::notifications_default_setting_for_all_websites.to_isetting();
    } else if (key_name == settings::website::specific::downloads_default_setting.key_name()) {
        info->default_isetting = settings::websites::downloads_default_setting_for_all_websites.to_isetting();
    } else if (key_name == settings::website::specific::external_apps_default_setting.key_name()) {
        info->default_isetting = settings::websites::external_apps_default_setting_for_all_websites.to_isetting();
    } else if (key_name == settings::website::specific::popup_windows_default_setting.key_name()) {
        info->default_isetting = settings::websites::popup_windows_default_setting_for_all_websites.to_isetting();
    } else if (key_name == settings::website::specific::clipboard_default_setting.key_name()) {
        info->default_isetting = settings::websites::clipboard_default_setting_for_all_websites.to_isetting();
    } else if (key_name == settings::website::specific::device_info_default_setting.key_name()) {
        info->default_isetting = settings::websites::device_info_default_setting_for_all_websites.to_isetting();
    } else if (key_name == settings::website::specific::enable_page_tweaks.key_name()) {
        // This is not user-configurable
        info->default_isetting = settings::websites::page_tweaks_default_setting_for_all_websites.to_isetting();
    } else if (key_name == settings::website::specific::page_tweaks_force_dark_mode.key_name()) {
        // This is not user-configurable
        info->default_isetting = settings::websites::page_tweaks_force_dark_mode_default_setting_for_all_websites.to_isetting();
    } else if (key_name == settings::website::specific::page_tweaks_font_size.key_name()) {
        // This is not user-configurable
        info->default_isetting = settings::websites::page_tweaks_font_size_default_setting_for_all_websites.to_isetting();
    } else if (key_name == settings::website::specific::page_tweaks_font_family.key_name()) {
        // This is not user-configurable
        info->default_isetting = settings::websites::page_tweaks_font_family_default_setting_for_all_websites.to_isetting();
    }  else if (key_name == settings::website::specific::page_tweaks_disable_sticky.key_name()) {
        // This is not user-configurable
        info->default_isetting = settings::websites::page_tweaks_disable_sticky_default_setting_for_all_websites.to_isetting();
    }  else if (key_name == settings::website::specific::page_tweaks_element_blocker_list.key_name()) {
        // This is not user-configurable
        info->default_isetting = settings::websites::page_tweaks_element_blocker_list_default_setting_for_all_websites.to_isetting();
    } else if (key_name == settings::website::specific::enable_reader_mode.key_name()) {
        info->default_isetting = settings::websites::enable_reader_mode_default_setting_for_all_websites.to_isetting();
    } else {
        RAISE(ErrorCode::not_found, "unknown key_name: %s", key_name.c_str());
    }

    RAISE_CHECK(info->default_isetting != nullptr); // sanity check

    SUCCEED();
}

Error
ProfilesDb::get_setting_for_website_or_default(const GetSettingForWebsiteOrDefaultQuery& query, std::string* value)
{
    RAISE_CHECK(value);

    if (query.profile_id == ProfileId(0)) {
        RAISE(ErrorCode::argument, "profile_id is not set");
    }

    log_verbose("ProfilesDb::get_setting_for_website_or_default: profile_id=%lld website=%s key_name=%s",
        query.profile_id.value,
        query.website.c_str(),
        query.key_name.c_str()
    );

    // Try to find specific setting for the website first
    {
        std::string sql = R"(
            SELECT value, type FROM settings
            WHERE profile_id = ?1 AND key = ?2;
        )";

        std::string key = std::format("settings::website::specific::{}::{}", query.website, query.key_name);

        std::vector<DbVariant> args = {
            query.profile_id.value,
            key,
        };

        DbStmt stmt;
        VERIFY(m_db->query(sql, args, &stmt));

        bool has_row = stmt.next();
        VERIFY(stmt.error());

        if (has_row) {
            *value = stmt.text("value");
            SUCCEED();
        }
    }

    // Specific setting not found, fall back to default setting
    {
        if (query.key_name == settings::website::specific::enable_page_tweaks.key_name()) {
            // Default value is not user-configurable
            *value = settings::website::specific::enable_page_tweaks.default_value_as_string();
            SUCCEED();
        }

        WebsiteDefaultSettingInfo info{};
        VERIFY(find_website_default_setting_info(query.key_name, &info));

        RAISE_CHECK(info.default_isetting != nullptr); // sanity check

        std::string default_value;
        VERIFY(get_setting(GetSettingQuery{
            .profile_id = query.profile_id,
            .key = info.default_isetting->key(),
            .realm = info.default_isetting->realm(),
            .key_name = info.default_isetting->key_name(),
            .type = info.default_isetting->type(),
            .default_value = info.default_isetting->default_value_as_string(),
        }, &default_value));

        if (info.enum_value_of_true != -1) {
            if (default_value == std::to_string(info.enum_value_of_true)) {
                *value = "1";
            } else {
                *value = "0";
            }
        } else {
            *value = default_value;
        }
    }

    SUCCEED();
}

Error
ProfilesDb::get_all_values_for_website_setting(
    const GetAllValuesForWebsiteSettingQuery& query, GetAllValuesForWebsiteSettingReply* reply)
{
    if(query.profile_id == ProfileId(0)) {
        RAISE(ErrorCode::argument, "profile_id is not set");
    }
    if(query.key_name.length() == 0)
    {
        RAISE(ErrorCode::argument, "key_name is empty");
    }


    {
        std::string sql = R"(
            SELECT key, value, type FROM settings
            WHERE profile_id = ?1 AND key GLOB ?2
            ORDER BY key COLLATE NOCASE;
        )";

        std::vector<DbVariant> args = {
            query.profile_id.value,
            "settings::website::specific::*::" + query.key_name
        };

        DbStmt stmt;
        VERIFY(m_db->query(sql, args, &stmt));

        while(stmt.next())
        {
            reply->settings.emplace_back(
                stmt.text("key"),
                stmt.text("type"),
                stmt.text("value"));
        }
        VERIFY(stmt.error());
    }
    
    SUCCEED();
}

Error
ProfilesDb::delete_setting_for_website(
    const DeleteSettingForWebsiteQuery& query,
    SettingsChange* settings_change,
    SingleSettingAnyChange* single_change)
{
    if (query.profile_id == ProfileId(0)) {
        RAISE(ErrorCode::argument, "profile_id is not set");
    }

    log_verbose("ProfilesDb::delete_setting_for_website: profile_id=%lld website=%s key_name=%s",
        query.profile_id.value,
        query.website.c_str(),
        query.key_name.c_str()
    );

    WebsiteDefaultSettingInfo default_info{};
    VERIFY(find_website_default_setting_info(query.key_name, &default_info));

    std::string default_value;

    // Get current default value for all websites
    {
        std::string sql = R"(
            SELECT value, type FROM settings
            WHERE profile_id = ?1 AND key = ?2;
        )";

        std::vector<DbVariant> args = {
            query.profile_id.value,
            default_info.default_isetting->key(),
        };

        DbStmt stmt;
        VERIFY(m_db->query(sql, args, &stmt));
        bool has_row = stmt.next();
        VERIFY(stmt.error());
        std::string value;
        if (!has_row) {
            value = default_info.default_isetting->default_value_as_string();
        } else {
            value = stmt.text("value");
        }

        if (default_info.enum_value_of_true != -1) {
            if (value == std::to_string(default_info.enum_value_of_true)) {
                default_value = "1";
            } else {
                default_value = "0";
            }
        } else {
            default_value = value;
        }
    }

    std::string realm = "settings::website::specific";
    std::string key = std::format("{}::{}::{}", realm, query.website, query.key_name);
    std::string type;
    bool has_old_value = false;
    std::string old_value{};
    {
        std::string sql = R"(
            SELECT value, type FROM settings
            WHERE profile_id = ?1 AND key = ?2;
        )";

        std::vector<DbVariant> args = {
            query.profile_id.value,
            key,
        };

        DbStmt stmt;
        VERIFY(m_db->query(sql, args, &stmt));
        has_old_value = stmt.next();
        VERIFY(stmt.error());

        if (has_old_value) {
            old_value = stmt.text("value");
            type = stmt.text("type");
        }
    }

    if (has_old_value) {
        std::string sql = R"(
            DELETE FROM settings
            WHERE profile_id = ?1 AND key = ?2;
        )";

        std::vector<DbVariant> args = {
            query.profile_id.value,
            key,
        };

        VERIFY(m_db->exec(sql, args));

        if (single_change) {
            single_change->profile_id = query.profile_id;
            single_change->key = key;
            single_change->realm = realm;
            single_change->key_name = query.key_name;
            single_change->type = type;
            single_change->default_value = default_value;
            single_change->old_value = old_value;
            single_change->new_value = default_value;
        }

        if (settings_change) {
            SingleSettingAnyChange change{};
            change.profile_id = query.profile_id;
            change.key = key;
            change.realm = realm;
            change.key_name = query.key_name;
            change.type = type;
            change.default_value = default_value;
            change.old_value = old_value;
            change.new_value = default_value;
            settings_change->updated.push_back(change);
        }
    }

    SUCCEED();
}

Error
ProfilesDb::delete_settings(ProfileId profile_id)
{
    if (profile_id == ProfileId(0)) {
        RAISE(ErrorCode::argument, "profile_id is not set");
    }

    std::string sql = R"(
        DELETE FROM settings
        WHERE profile_id = ?1;
    )";

    std::vector<DbVariant> args = {
        profile_id.value,
    };

    VERIFY(m_db->exec(sql, args));

    SUCCEED();
}

Error
settings::get_isetting_for_setting_key(std::string key, const settings::ISetting** out_setting)
{
    if(!out_setting) RAISE(ErrorCode::argument, "got nullptr for out parameter");

#define REALM(__realm) \
    do { \
        if(key.rfind(__realm::realm_name(), 0) == 0) \
        { \
            for(const auto& setting : __realm::all()) \
            { \
                if(setting->key() == key) \
                { \
                    *out_setting = setting; \
                    SUCCEED(); \
                } \
            } \
        } \
    } while(0);

    REALM(settings::general);
    REALM(settings::appearance);
    REALM(settings::tabs);
    REALM(settings::browsing);
    REALM(settings::sync);
    REALM(settings::passwords);
    REALM(settings::privacy);
    REALM(settings::search);
    REALM(settings::websites);
    REALM(settings::dialogs::add_bookmark);
    REALM(settings::website::specific); //TODO: perhaps reimplement this, so that we can handle website settings better
    REALM(settings::advanced);
    REALM(settings::reset_orion);
    REALM(settings::filter_lists);
    REALM(settings::windows);
    REALM(settings::reader_mode);
    REALM(settings::test);
    REALM(settings::test::subrealm);
    REALM(settings::common::onboarding);

    
    RAISE(ErrorCode::not_found, "failed to get default value for setting key: %s", key.c_str());

#undef CHECK
}

} // namespace orion
