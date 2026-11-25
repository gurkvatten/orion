#include <core/profiles_db/profiles_db.h>
#include <format>
#include <chrono>

//-------------------------------------------------------------------------
// Extension Storage Areas
//-------------------------------------------------------------------------

namespace orion {

std::shared_ptr<Db>
ProfilesDb::extension_storage_areas_db(const std::string& storage_id) const
{
    return storage_id == "session" ? m_extension_session_storage_db : m_db;
}

Error
ProfilesDb::extension_storage_areas_get(
    ExtensionStorageAreas::GetQuery query, std::vector<ExtensionStorageAreas::KeyValue>* out_key_values)
{
    RAISE_CHECK(out_key_values);

    if (query.extension_id.value.empty()) {
        RAISE(ErrorCode::argument, "extension_id is empty");
    }

    if (query.storage_id.empty()) {
        RAISE(ErrorCode::argument, "storage_id is empty");
    }

    if (query.key_values.empty()) {
        RAISE(ErrorCode::argument, "key_values is empty");
    }

    std::vector<DbVariant> args{
        query.extension_id.value,
        query.storage_id,
    };

    for (const ExtensionStorageAreas::KeyValue& kv : query.key_values) {
        args.push_back(kv.key);
    }

    std::string sql = std::format(R"(
        SELECT key, value
        FROM extension_storage_areas
        WHERE extension_id = ? AND storage_id = ? AND key IN ({})
        ORDER BY extension_id, storage_id, key
    )", Db::make_placeholders(query.key_values.size()));

    DbStmt stmt;
    VERIFY(extension_storage_areas_db(query.storage_id)->query(sql, args, &stmt));

    std::map<std::string, std::string> existing_key_values;
    while (stmt.next()) {
        existing_key_values[stmt.text("key")] = stmt.text("value");
    }
    VERIFY(stmt.error());

    for (const ExtensionStorageAreas::KeyValue& kv : query.key_values) {
        if (existing_key_values.find(kv.key) == existing_key_values.end()) {
            if(kv.value) {
                // fallback to default value
                out_key_values->push_back(kv);
            }
        } else {
            // use existing value
            out_key_values->push_back(ExtensionStorageAreas::KeyValue{
                .key = kv.key,
                .value = existing_key_values[kv.key],
            });
        }
    }
    SUCCEED();
}

Error
ProfilesDb::extension_storage_areas_get_all(
    ExtensionStorageAreas::GetAllQuery query, std::vector<ExtensionStorageAreas::KeyValue>* out_key_values)
{
    RAISE_CHECK(out_key_values);

    if (query.extension_id.value.empty()) {
        RAISE(ErrorCode::argument, "extension_id is empty");
    }

    if (query.storage_id.empty()) {
        RAISE(ErrorCode::argument, "storage_id is empty");
    }
    
    std::vector<DbVariant> args{
        query.extension_id.value,
        query.storage_id,
    };

    std::string sql = std::format(R"(
        SELECT key, value
        FROM extension_storage_areas
        WHERE extension_id = ?1 AND storage_id = ?2
        ORDER BY extension_id, storage_id, key
    )");

    DbStmt stmt;
    VERIFY(extension_storage_areas_db(query.storage_id)->query(sql, args, &stmt));

    std::map<std::string, std::string> existing_key_values;
    while (stmt.next()) {
        out_key_values->push_back(ExtensionStorageAreas::KeyValue{
            .key = stmt.text("key"),
            .value = stmt.text("value"),
        });
    }
    VERIFY(stmt.error());
    SUCCEED();
}

Error
ProfilesDb::extension_storage_areas_get_keys(
    ExtensionStorageAreas::GetKeysQuery query, std::vector<std::string>* out_keys)
{
    RAISE_CHECK(out_keys);

    if (query.extension_id.value.empty()) {
        RAISE(ErrorCode::argument, "extension_id is empty");
    }

    if (query.storage_id.empty()) {
        RAISE(ErrorCode::argument, "storage_id is empty");
    }
    
    std::vector<DbVariant> args{
        query.extension_id.value,
        query.storage_id,
    };

    std::string sql = std::format(R"(
        SELECT key
        FROM extension_storage_areas
        WHERE extension_id = ?1 AND storage_id = ?2
        ORDER BY extension_id, storage_id, key
    )");

    DbStmt stmt;
    VERIFY(extension_storage_areas_db(query.storage_id)->query(sql, args, &stmt));

    std::map<std::string, std::string> existing_key_values;
    while (stmt.next()) {
        out_keys->push_back(stmt.text("key"));
    }
    VERIFY(stmt.error());
    SUCCEED();
}

Error
ProfilesDb::extension_storage_areas_get_bytes_in_use(
    ExtensionStorageAreas::GetBytesInUseQuery query,
    uint64_t* out_bytes_in_use)
{
    RAISE_CHECK(out_bytes_in_use);
    if (query.extension_id.value.empty()) {
        RAISE(ErrorCode::argument, "extension_id is empty");
    }
    if (query.storage_id.empty()) {
        RAISE(ErrorCode::argument, "storage_id is empty");
    }

    *out_bytes_in_use = 0;
    if (query.storage_id == "managed") {
        SUCCEED();
    }
    if (query.keys && query.keys->empty()) {
        SUCCEED();
    }

    std::vector<ExtensionStorageAreas::KeyValue> key_values;
    if (query.keys) {
        ExtensionStorageAreas::GetQuery get_query{
            .extension_id = query.extension_id,
            .storage_id = query.storage_id,
        };
        for (const auto& key : *query.keys) {
            get_query.key_values.push_back({.key = key});
        }
        VERIFY(extension_storage_areas_get(std::move(get_query), &key_values));
    }
    else {
        VERIFY(extension_storage_areas_get_all({
            .extension_id = query.extension_id,
            .storage_id = query.storage_id,
        }, &key_values));
    }

    for (const auto& key_value : key_values) {
        if (key_value.value) {
            *out_bytes_in_use += key_value.key.size() + key_value.value->size();
        }
    }
    SUCCEED();
}

Error
ProfilesDb::extension_storage_areas_set(
    ExtensionStorageAreas::SetQuery query,
    std::vector<ExtensionStorageAreas::Change>* out_changes)
{
    if (out_changes) {
        out_changes->clear();
    }
    if (query.key_values.empty()) {
        SUCCEED();
    }

    const std::shared_ptr<Db> db = extension_storage_areas_db(query.storage_id);
    DbTx tx;
    VERIFY(tx.begin(db));

    std::map<std::string, std::string> new_values;
    for (const ExtensionStorageAreas::KeyValue& kv : query.key_values) {
        new_values[kv.key] = kv.value.value_or("");
    }

    std::vector<ExtensionStorageAreas::KeyValue> old_key_values;
    const bool needs_complete_state = query.quota
        && (query.quota->bytes || query.quota->max_items);
    if (needs_complete_state) {
        VERIFY(extension_storage_areas_get_all({
            .extension_id = query.extension_id,
            .storage_id = query.storage_id,
        }, &old_key_values));
    }
    else {
        ExtensionStorageAreas::GetQuery get_query{
            .extension_id = query.extension_id,
            .storage_id = query.storage_id,
        };
        for (const auto& [key, value] : new_values) {
            get_query.key_values.push_back({.key = key});
        }
        VERIFY(extension_storage_areas_get(
            std::move(get_query), &old_key_values));
    }
    std::map<std::string, std::string> old_values;
    for (const auto& kv : old_key_values) {
        if (kv.value) {
            old_values[kv.key] = *kv.value;
        }
    }

    if (query.quota) {
        std::map<std::string, std::string> resulting_values = old_values;
        for (const auto& [key, value] : new_values) {
            resulting_values[key] = value;
        }

        uint64_t bytes = 0;
        for (const auto& [key, value] : resulting_values) {
            const uint64_t item_bytes = key.size() + value.size();
            if (query.quota->bytes_per_item
                && item_bytes > *query.quota->bytes_per_item) {
                RAISE(ErrorCode::invalid_state,
                    "QUOTA_BYTES_PER_ITEM quota exceeded");
            }
            bytes += item_bytes;
        }
        if (query.quota->bytes && bytes > *query.quota->bytes) {
            RAISE(ErrorCode::invalid_state, "QUOTA_BYTES quota exceeded");
        }
        if (query.quota->max_items
            && resulting_values.size() > *query.quota->max_items) {
            RAISE(ErrorCode::invalid_state, "MAX_ITEMS quota exceeded");
        }
    }

    for (const auto& [key, value] : new_values) {
        std::vector<DbVariant> args{
            query.extension_id.value,
            query.storage_id,
            key,
            value,
        };

        std::string sql = R"(
            INSERT INTO extension_storage_areas (extension_id, storage_id, key, value)
            VALUES (?1, ?2, ?3, ?4)
            ON CONFLICT(extension_id, storage_id, key) DO UPDATE SET value = excluded.value
        )";

        VERIFY(db->exec(sql, args));
    }

    std::vector<ExtensionStorageAreas::Change> changes;
    for (const auto& [key, value] : new_values) {
        const auto old = old_values.find(key);
        if (old != old_values.end() && old->second == value) {
            continue;
        }
        changes.push_back({
            .key = key,
            .old_value = old == old_values.end()
                ? std::nullopt
                : std::optional(old->second),
            .new_value = value,
        });
    }

    VERIFY(tx.commit());
    if (out_changes) {
        *out_changes = std::move(changes);
    }
    SUCCEED();
}

Error
ProfilesDb::extension_storage_areas_remove(
    ExtensionStorageAreas::RemoveQuery query,
    std::vector<ExtensionStorageAreas::Change>* out_changes)
{
    if (out_changes) {
        out_changes->clear();
    }
    if (query.keys.empty()) {
        SUCCEED();
    }

    const std::shared_ptr<Db> db = extension_storage_areas_db(query.storage_id);
    DbTx tx;
    VERIFY(tx.begin(db));

    ExtensionStorageAreas::GetQuery get_query{
        .extension_id = query.extension_id,
        .storage_id = query.storage_id,
    };
    for (const auto& key : query.keys) {
        get_query.key_values.push_back({.key = key});
    }
    std::vector<ExtensionStorageAreas::KeyValue> old_key_values;
    VERIFY(extension_storage_areas_get(std::move(get_query), &old_key_values));

    std::vector<DbVariant> args{
        query.extension_id.value,
        query.storage_id,
    };

    for (const std::string& key : query.keys) {
        args.push_back(key);
    }

    std::string sql = std::format(R"(
        DELETE FROM extension_storage_areas
        WHERE extension_id = ?1 AND storage_id = ?2 AND key IN ({})
    )", Db::make_placeholders(query.keys.size()));

    VERIFY(db->exec(sql, args));
    VERIFY(tx.commit());

    if (out_changes) {
        for (auto& kv : old_key_values) {
            out_changes->push_back({
                .key = std::move(kv.key),
                .old_value = std::move(kv.value),
            });
        }
    }
    SUCCEED();
}

Error
ProfilesDb::extension_storage_areas_clear(
    ExtensionStorageAreas::ClearQuery query,
    std::vector<ExtensionStorageAreas::Change>* out_changes)
{
    if (out_changes) {
        out_changes->clear();
    }

    const std::shared_ptr<Db> db = extension_storage_areas_db(query.storage_id);
    DbTx tx;
    VERIFY(tx.begin(db));

    std::vector<ExtensionStorageAreas::KeyValue> old_key_values;
    VERIFY(extension_storage_areas_get_all({
        .extension_id = query.extension_id,
        .storage_id = query.storage_id,
    }, &old_key_values));

    std::vector<DbVariant> args{
        query.extension_id.value,
        query.storage_id,
    };

    std::string sql = R"(
        DELETE FROM extension_storage_areas
        WHERE extension_id = ?1 AND storage_id = ?2
    )";

    VERIFY(db->exec(sql, args));
    VERIFY(tx.commit());

    if (out_changes) {
        for (auto& kv : old_key_values) {
            out_changes->push_back({
                .key = std::move(kv.key),
                .old_value = std::move(kv.value),
            });
        }
    }
    SUCCEED();
}

Error
ProfilesDb::extension_session_storage_clear(ExtensionId extension_id)
{
    if (extension_id.value.empty()) {
        RAISE(ErrorCode::argument, "extension_id is empty");
    }
    VERIFY(extension_storage_areas_clear({
        .extension_id = std::move(extension_id),
        .storage_id = "session",
    }));
    SUCCEED();
}

Error
ProfilesDb::extension_storage_access_level_get(
    ExtensionStorageAccessLevels::GetQuery query,
    bool* out_allow_untrusted_contexts)
{
    RAISE_CHECK(out_allow_untrusted_contexts);
    if(query.extension_id.value.empty()) {
        RAISE(ErrorCode::argument, "extension_id is empty");
    }
    if(query.storage_id.empty()) {
        RAISE(ErrorCode::argument, "storage_id is empty");
    }

    const std::string sql = R"(
        SELECT allow_untrusted_contexts
        FROM extension_storage_access_levels
        WHERE extension_id = ?1 AND storage_id = ?2
    )";
    const std::vector<DbVariant> args{
        query.extension_id.value,
        query.storage_id,
    };
    DbStmt stmt;
    VERIFY(m_db->query(sql, args, &stmt));
    *out_allow_untrusted_contexts = query.default_allow_untrusted_contexts;
    if(stmt.next()) {
        *out_allow_untrusted_contexts
            = stmt.integer("allow_untrusted_contexts") != 0;
    }
    VERIFY(stmt.error());
    SUCCEED();
}

Error
ProfilesDb::extension_storage_access_level_set(
    ExtensionStorageAccessLevels::SetQuery query)
{
    if(query.extension_id.value.empty()) {
        RAISE(ErrorCode::argument, "extension_id is empty");
    }
    if(query.storage_id.empty()) {
        RAISE(ErrorCode::argument, "storage_id is empty");
    }

    const std::string sql = R"(
        INSERT INTO extension_storage_access_levels (
            extension_id, storage_id, allow_untrusted_contexts
        ) VALUES (?1, ?2, ?3)
        ON CONFLICT(extension_id, storage_id) DO UPDATE SET
            allow_untrusted_contexts = excluded.allow_untrusted_contexts
    )";
    const std::vector<DbVariant> args{
        query.extension_id.value,
        query.storage_id,
        static_cast<int64_t>(query.allow_untrusted_contexts),
    };
    VERIFY(m_db->exec(sql, args));
    SUCCEED();
}

} // namespace orion
