#include <core/profiles_db/profiles_db.h>
#include <core/dto/password_item_dto.h>
#include <core/password_storage.h>
#include <common/src/string_util.h>
#include <common/src/csv_util.h>
#include <common/src/uri.h>
#include <common/src/crypto.h>

#include <format>
#include <regex>

namespace orion {

//-------------------------------------------------------------------------
// Password Items
//-------------------------------------------------------------------------

// Returns empty string if parsing fails or secret key is missing
std::string
parse_verification_key_from_otp_auth(const std::string& otp_auth)
{
    std::regex pattern(R"((?:\?|&)secret=([^&]+))");
    std::smatch match;

    if (std::regex_search(otp_auth, match, pattern)) {
        return match[1].str();
    }

    return "";
}

Error
ProfilesDb::fill_verification_code(PasswordItemDto* item)
{
    RAISE_CHECK(item);

    if (item->verification_key.empty()) {
        item->verification_code = "";
        SUCCEED();
    }

    GenerateTotpVerificationCodeQuery query{
        .verification_key = item->verification_key,
    };

    VERIFY(generate_totp_verification_code(query, &item->verification_code));
    SUCCEED();
}

Error
ProfilesDb::create_password_item(
    CreatePasswordItemQuery query, PasswordItemId* id, PasswordItemsChange* change, bool* already_exists)
{
    if (query.profile_id == ProfileId(0)) {
        RAISE(ErrorCode::argument, "profile_id is not set");
    }

    std::vector<PasswordItemDto> items;

    // load existing items
    VERIFY(m_password_storage->get_password_items(m_app_id.c_str(), &items));

    // check for existing url-username item
    auto it = std::find_if(items.begin(), items.end(), [&query](const PasswordItemDto& item) {
        return item.profile_id == query.profile_id &&
            item.url == query.url &&
            item.username == query.username;
    });

    if (id && it != items.end()) {
        *id = it->id;
    }

    if (already_exists) {
        *already_exists = (it != items.end());
    }

    if (!query.update_existing && it != items.end()) {
        // do not change anything
        SUCCEED();
    }

    int64_t max_id = 0;
    for (const auto& it : items) {
        if (max_id < it.id.value) {
            max_id = it.id.value;
        }
    }

    PasswordItemId new_id(max_id + 1);

    if (id) {
        *id = new_id;
    }

    PasswordItemDto dto{
        .id = new_id,
        .profile_id = query.profile_id,
        .url = query.url,
        .username = query.username,
        .password = query.password,
        .verification_key = query.verification_key,
        .verification_code = query.verification_code,
    };

    items.push_back(dto);

    // store modified items
    VERIFY(m_password_storage->upsert_password_items(m_app_id.c_str(), items));

    fill_verification_code(&dto);

    if (change) {
        change->created.push_back(dto);
    }
    SUCCEED();
}

Error
ProfilesDb::get_password_item_ids(GetPasswordItemIdsQuery query, std::vector<PasswordItemId>* ids)
{
    RAISE_CHECK(ids);

    if (query.profile_id == ProfileId(0)) {
        RAISE(ErrorCode::argument, "profile_id is not set");
    }

    std::vector<PasswordItemDto> items;
    VERIFY(m_password_storage->get_password_items(m_app_id.c_str(), &items));

    // sort by: profile_id ASC, id ASC
    std::sort(items.begin(), items.end(), [](const PasswordItemDto& a, const PasswordItemDto& b) {
        if (a.profile_id != b.profile_id) {
            return a.profile_id.value < b.profile_id.value;
        }
        return a.id.value < b.id.value;
    });

    for (const auto& item : items) {
        if (item.profile_id != query.profile_id) {
            continue;
        }

        // filter by search text
        if (!query.search_text.empty()) {
            if (!orion::str::contains_case_insensitive(item.url, query.search_text) &&
                !orion::str::contains_case_insensitive(item.username, query.search_text)) {
                continue;
            }
        }

        // filter by authority
        if (!query.authority.empty()) {
            Uri uri = Uri::parse_strict(item.url.c_str());
            if (uri.authority() != query.authority) {
                continue;
            }
        }

        // filter by username
        if (!query.username.empty()) {
            if (item.username != query.username) {
                continue;
            }
        }

        ids->push_back(item.id);
    }

    SUCCEED();
}

Error
ProfilesDb::get_password_items(const std::vector<PasswordItemId>& ids, std::vector<PasswordItemDto>* items)
{
    RAISE_CHECK(items);

    std::vector<PasswordItemDto> all_items;
    VERIFY(m_password_storage->get_password_items(m_app_id.c_str(), &all_items));

    std::set<PasswordItemId> id_set(ids.begin(), ids.end());

    // temporary buffer
    std::vector<PasswordItemDto> result;

    for (auto& item : all_items) {
        if (id_set.find(item.id) != id_set.end()) {
            VERIFY(fill_verification_code(&item));
            result.push_back(item);
        }
    }

    // sort temporary buffer by: profile_id ASC, id ASC
    std::sort(result.begin(), result.end(), [](const PasswordItemDto& a, const PasswordItemDto& b) {
        if (a.profile_id != b.profile_id) {
            return a.profile_id.value < b.profile_id.value;
        }
        return a.id.value < b.id.value;
    });

    // append to output vector
    items->insert(items->end(), result.begin(), result.end());

    SUCCEED();
}

Error
ProfilesDb::update_password_item(UpdatePasswordItemQuery query, PasswordItemsChange* change)
{
    if (query.id == PasswordItemId(0)) {
        RAISE(ErrorCode::argument, "id is not set");
    }

    std::vector<PasswordItemDto> all_items;
    VERIFY(m_password_storage->get_password_items(m_app_id.c_str(), &all_items));

    // find the item to update
    auto it = std::find_if(all_items.begin(), all_items.end(), [&query](const PasswordItemDto& item) {
        return item.id == query.id;
    });

    if (it == all_items.end()) {
        RAISE(ErrorCode::not_found, "password item with id %lld not found", query.id.value);
    }

    // update fields
    PasswordItemDto& item = *it;
    
    if (query.update_url) {
        item.url = query.url;
    }
    
    if (query.update_username) {
        item.username = query.username;
    }

    if (query.update_password) {
        item.password = query.password;
    }

    if (query.update_verification_key) {
        item.verification_key = query.verification_key;
    }

    if (query.update_verification_code) {
        item.verification_code = query.verification_code;
    }

    // store modified items
    VERIFY(m_password_storage->upsert_password_items(m_app_id.c_str(), all_items));

    VERIFY(fill_verification_code(&item));

    if (change) {
        change->updated.push_back(item);
    }
    SUCCEED();
}

Error
ProfilesDb::delete_password_items(const std::vector<PasswordItemId>& ids, PasswordItemsChange* change)
{
    if (ids.empty()) {
        log_warning("delete_password_items: called with empty ids vector");
        SUCCEED();
    }

    std::vector<PasswordItemDto> all_items;
    VERIFY(m_password_storage->get_password_items(m_app_id.c_str(), &all_items));

    // check that all ids exist
    for (const auto& id : ids) {
        auto it = std::find_if(all_items.begin(), all_items.end(), [&id](const PasswordItemDto& item) {
            return item.id == id;
        });
        if (it == all_items.end()) {
            log_warning("delete_password_items: password item with id %lld not found", id.value);
        }
    }

    std::set<PasswordItemId> id_set(ids.begin(), ids.end());
    std::vector<PasswordItemDto> remaining_items;

    for (const auto& item : all_items) {
        if (id_set.find(item.id) != id_set.end()) {
            // item to delete
            if (change) {
                change->deleted.push_back(item.id);
            }
        } else {
            // keep this item
            remaining_items.push_back(item);
        }
    }

    // store remaining items
    VERIFY(m_password_storage->upsert_password_items(m_app_id.c_str(), remaining_items));
    
    SUCCEED();
}

static std::string
get_otp_auth_csv_field(const PasswordItemDto& item)
{
    if (item.verification_key.empty()) {
        return "";
    }

    return std::format(
        "otpauth://totp/{}:{}?secret={}",
        item.url,
        item.username,
        item.verification_key
    );
}

Error
ProfilesDb::get_password_items_csv_export(const std::vector<PasswordItemId>& ids, std::string* out_csv)
{
    RAISE_CHECK(out_csv);

    std::vector<PasswordItemDto> items;
    VERIFY(get_password_items(ids, &items));

    std::vector<std::vector<std::string>> csv_rows;

    // header
    csv_rows.push_back({
        "Title", "URL", "Username", "Password", "OTPAuth"
    });

    for (const PasswordItemDto& item : items) {
        std::vector<std::string> fields;

        Uri uri = Uri::parse_strict(item.url.c_str());
        std::string website = item.url;
        if (!uri.authority().empty()) {
            website = uri.authority();
        }

        std::string otp_auth = get_otp_auth_csv_field(item);

        fields.push_back(website + " (" + item.username + ")");
        fields.push_back(item.url);
        fields.push_back(item.username);
        fields.push_back(item.password);
        fields.push_back(otp_auth);

        csv_rows.push_back(fields);
    }

    char delimiter = ',';
    std::string newline = "\n";    
    for (const std::vector<std::string>& row : csv_rows) {
        size_t n = 0;
        for (const std::string& field : row) {
            *out_csv += escape_csv_field(field, delimiter);
            n++;
            if (n < row.size()) {
                *out_csv += std::string(1, delimiter);
            }
        }
        *out_csv += newline;
    }
    SUCCEED();
}

// enum class CsvType {
//     unknown,
//     orion,
//     firefox,
//     chrome,
//     opera,
// };

static Error
parse_csv_row(
    const std::vector<std::string>& header, const std::vector<std::string>& row, PasswordItemDto* out_item)
{
    RAISE_CHECK(out_item);

    // std::map<CsvType, std::vector<std::string>> known_headers = {
    //     { CsvType::orion,   { "Title", "URL", "Username", "Password", "OTPAuth" } },
    //     { CsvType::firefox, { "url", "username", "password" } },
    //     { CsvType::chrome,  { "name", "url", "username", "password" } },
    //     { CsvType::opera,   { "name", "url", "username", "password" } },
    // };

    {
        auto it = std::find_if(header.begin(), header.end(), [](const std::string& col) {
            return orion::str::equals_case_insensitive(col, "title");
        });

        if (it != header.end()) {
            std::size_t idx = std::distance(header.begin(), it);
            if (idx < row.size()) {
                std::string title = row[idx];

                // Strip " (Passwords not saved)" suffix if present (used in Orion CSV export)
                std::string suffix = " (Passwords not saved)";
                if (title.size() >= suffix.size() &&
                    title.compare(title.size() - suffix.size(), suffix.size(), suffix) == 0)
                {
                    // No special field is needed at the moment:
                    //out_item->never_for_this_website = true;
                }
            }
        }
    }
    {
        auto it = std::find_if(header.begin(), header.end(), [](const std::string& col) {
            return orion::str::equals_case_insensitive(col, "url");
        });

        if (it != header.end()) {
            std::size_t idx = std::distance(header.begin(), it);
            if (idx < row.size()) {
                std::string url = row[idx];
                out_item->url = url;
            }
        }
    }
    {
        auto it = std::find_if(header.begin(), header.end(), [](const std::string& col) {
            return orion::str::equals_case_insensitive(col, "username");
        });

        if (it != header.end()) {
            std::size_t idx = std::distance(header.begin(), it);
            if (idx < row.size()) {
                out_item->username = row[idx];

                // If the username is "Passwords not saved", we should not offer to save passwords for this website
            }
        }
    }
    {
        auto it = std::find_if(header.begin(), header.end(), [](const std::string& col) {
            return orion::str::equals_case_insensitive(col, "password");
        });

        if (it != header.end()) {
            std::size_t idx = std::distance(header.begin(), it);
            if (idx < row.size()) {
                out_item->password = row[idx];
            }
        }
    }
    {
        auto it = std::find_if(header.begin(), header.end(), [](const std::string& col) {
            return orion::str::equals_case_insensitive(col, "otpauth");
        });

        if (it != header.end()) {
            std::size_t idx = std::distance(header.begin(), it);
            if (idx < row.size()) {
                std::string otp_auth = row[idx];
                std::string verification_key = parse_verification_key_from_otp_auth(otp_auth);
                out_item->verification_key = verification_key;
            }
        }
    }
    SUCCEED();
}

Error
ProfilesDb::import_password_items_from_csv(
    ImportPasswordItemsFromCsvQuery query, std::vector<PasswordItemDto>* out_imported,
    std::vector<PasswordItemDto>* out_not_imported, PasswordItemsChange* change)
{
    RAISE_CHECK(out_imported);
    RAISE_CHECK(out_not_imported);

    std::vector<std::vector<std::string>> csv_rows;
    VERIFY(parse_csv(query.csv_text, ',', &csv_rows));

    std::vector<PasswordItemDto> csv_items;
    std::vector<std::string> header;

    for (size_t i = 0; i < csv_rows.size(); i++) {
        if (i == 0) {
            header = csv_rows[i];
            continue;
        }

        const std::vector<std::string>& row = csv_rows[i];

        PasswordItemDto item{};
        VERIFY(parse_csv_row(header, row, &item));

        if (item.url.empty() || item.username.empty()) {
            size_t line_num = i + 1;
            log_warning("import_password_items_from_csv: skipping row %zu with empty url or username", line_num);
            continue;
        }
    
        csv_items.push_back(PasswordItemDto{
            .id = PasswordItemId(0), // will be assigned later
            .profile_id = query.profile_id,
            .url = item.url,
            .username = item.username,
            .password = item.password,
            .verification_key = item.verification_key,
            .verification_code = "",
        });
    }

    std::vector<PasswordItemId> existing_ids;
    VERIFY(get_password_item_ids({.profile_id = query.profile_id}, &existing_ids));
    std::vector<PasswordItemDto> existing_items;
    VERIFY(get_password_items(existing_ids, &existing_items));

    for (const PasswordItemDto& csv_item : csv_items) {
        // check for duplicates
        auto it = std::find_if(
            existing_items.begin(), existing_items.end(),
            [&csv_item](const PasswordItemDto& existing_item) {
                return existing_item.url == csv_item.url && existing_item.username == csv_item.username;
            });

        if (it != existing_items.end()) {
            // duplicate found
            if (it->password != csv_item.password) {
                // mark as not imported if password differs
                out_not_imported->push_back(csv_item);
            } else {
                // treat as already imported
                out_imported->push_back(*it);
            }
            continue;
        }

        // assign new id
        PasswordItemDto new_item = csv_item;
        new_item.id = PasswordItemId(0); // will be assigned in create_password_item

        // create the new password item
        PasswordItemId new_id;
        VERIFY(create_password_item(
            CreatePasswordItemQuery{
                .profile_id = new_item.profile_id,
                .url = new_item.url,
                .username = new_item.username,
                .password = new_item.password,
                .verification_key = new_item.verification_key,
                .verification_code = new_item.verification_code,
                .update_existing = true,
            },
            &new_id,
            change,
            nullptr /* already_exists */
        ));

        new_item.id = new_id;
        out_imported->push_back(new_item);
    }
    SUCCEED();
}

Error
ProfilesDb::generate_totp_verification_code(GenerateTotpVerificationCodeQuery query, std::string* out_code)
{
    uint64_t timestamp = query.timestamp;
    if (timestamp == 0) {
        timestamp = time(NULL);
    }

    std::string code;
    VERIFY(crypto::totp(
        query.verification_key.c_str(), query.digits, timestamp, query.period_seconds, query.algo, &code
    ));
    if (out_code) {
        *out_code = code;
    }
    SUCCEED();
}

} // namespace orion
