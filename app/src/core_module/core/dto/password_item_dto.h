#pragma once

#include <common/src/strong_typedef.h>
#include <core/dto/profile_dto.h>

namespace orion {

DECLARE_STRONG_TYPEDEF(PasswordItemId, int64_t);

struct PasswordItemDto {
    DTO_MEMBER(id, PasswordItemId);
    DTO_MEMBER(profile_id, ProfileId);
    DTO_MEMBER(url, std::string);
    DTO_MEMBER(username, std::string);
    DTO_MEMBER(password, std::string);
    DTO_MEMBER(verification_key, std::string);
    DTO_MEMBER(verification_code, std::string); // not used currently
};

struct CreatePasswordItemQuery {
    DTO_MEMBER(profile_id, ProfileId);
    DTO_MEMBER(url, std::string);
    DTO_MEMBER(username, std::string);
    DTO_MEMBER(password, std::string);
    DTO_MEMBER(verification_key, std::string);
    DTO_MEMBER(verification_code, std::string); // not used currently
    DTO_MEMBER(update_existing, bool); // if false, do not update existing url-username item if it exists
};

struct CreatePasswordItemReply {
    DTO_MEMBER(err, Error);
    DTO_MEMBER(id, PasswordItemId);
    DTO_MEMBER(already_exists, bool); // true if an existing url-username item was found
};

struct CreateNeverSavePasswordItemQuery {
    DTO_MEMBER(profile_id, ProfileId);
    DTO_MEMBER(url, std::string);
};

struct CreateNeverSavePasswordItemReply {
    DTO_MEMBER(err, Error);
    DTO_MEMBER(id, PasswordItemId);
    DTO_MEMBER(already_exists, bool); // true if this website was already marked as "never save password"
};

struct GetNeverSavePasswordItemQuery {
    DTO_MEMBER(profile_id, ProfileId);
    DTO_MEMBER(authority, std::string); // authority part of URI, see format: scheme://authority/path?query#fragment
};

struct GetNeverSavePasswordItemReply {
    DTO_MEMBER(err, Error);
    DTO_MEMBER(id, PasswordItemId);
    DTO_MEMBER(never_save, bool); // true if this website was found as "never save password"
};

struct GetPasswordItemIdsQuery {
    DTO_MEMBER(profile_id, ProfileId);
    DTO_MEMBER(search_text, std::string);
    DTO_MEMBER(authority, std::string); // authority part of URI, see format: scheme://authority/path?query#fragment
    DTO_MEMBER(username, std::string); // ignored if empty
};

struct GetPasswordItemIdsReply {
    DTO_MEMBER(err, Error);
    DTO_MEMBER(ids, std::vector<PasswordItemId>);
};

struct GetPasswordItemsQuery {
    DTO_MEMBER(ids, std::vector<PasswordItemId>);
};

struct GetPasswordItemsReply {
    DTO_MEMBER(err, Error);
    DTO_MEMBER(items, std::vector<PasswordItemDto>);
};

struct UpdatePasswordItemQuery {
    DTO_MEMBER(id, PasswordItemId);

    DTO_MEMBER(url, std::string);
    DTO_MEMBER(username, std::string);
    DTO_MEMBER(password, std::string);
    DTO_MEMBER(verification_key, std::string);
    DTO_MEMBER(verification_code, std::string); // not used currently

    DTO_MEMBER(update_url, bool);
    DTO_MEMBER(update_username, bool);
    DTO_MEMBER(update_password, bool);
    DTO_MEMBER(update_verification_key, bool);
    DTO_MEMBER(update_verification_code, bool); // not used currently
};

struct PasswordItemsChange {
    DTO_MEMBER(created, std::vector<PasswordItemDto>);
    DTO_MEMBER(updated, std::vector<PasswordItemDto>);
    DTO_MEMBER(deleted, std::vector<PasswordItemId>);
};

struct GetPasswordItemsCsvExportReply {
    DTO_MEMBER(err, Error);
    DTO_MEMBER(csv_text, std::string);
};

struct ImportPasswordItemsFromCsvQuery {
    DTO_MEMBER(profile_id, ProfileId);
    DTO_MEMBER(csv_text, std::string);
};

struct ImportPasswordItemsFromCsvReply {
    DTO_MEMBER(err, Error);
    DTO_MEMBER(imported, std::vector<PasswordItemDto>);

    // items that could not be imported because their url-username tuple was already present
    // but their password differed
    DTO_MEMBER(not_imported, std::vector<PasswordItemDto>);
};

} // namespace orion