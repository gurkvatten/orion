#pragma once

#include <cstdint>
#include <string>
#include <common/src/dto/dto_member.h>
#include <core/dto/profile_dto.h>

namespace orion::ExtensionStorageAreas {

struct KeyValue {
    DTO_MEMBER(key, std::string);
    DTO_MEMBER(value, std::optional<std::string>);
};

struct GetQuery {
    DTO_MEMBER(extension_id, ExtensionId);
    DTO_MEMBER(storage_id, std::string);
    DTO_MEMBER(key_values, std::vector<KeyValue>);
};

struct GetReply {
    DTO_MEMBER(err, Error);
    DTO_MEMBER(key_values, std::vector<KeyValue>);
};

struct GetAllQuery {
    DTO_MEMBER(extension_id, ExtensionId);
    DTO_MEMBER(storage_id, std::string);
};

struct GetAllReply {
    DTO_MEMBER(err, Error);
    DTO_MEMBER(key_values, std::vector<KeyValue>);
};

struct GetKeysQuery {
    DTO_MEMBER(extension_id, ExtensionId);
    DTO_MEMBER(storage_id, std::string);
};

struct GetKeysReply {
    DTO_MEMBER(err, Error);
    DTO_MEMBER(keys, std::vector<std::string>);
};

struct GetBytesInUseQuery {
    DTO_MEMBER(extension_id, ExtensionId);
    DTO_MEMBER(storage_id, std::string);
    DTO_MEMBER(keys, std::optional<std::vector<std::string>>);
};

struct GetBytesInUseReply {
    DTO_MEMBER(err, Error);
    DTO_MEMBER(bytes_in_use, uint64_t);
};

struct Quota {
    DTO_MEMBER(bytes, std::optional<uint64_t>);
    DTO_MEMBER(bytes_per_item, std::optional<uint64_t>);
    DTO_MEMBER(max_items, std::optional<uint64_t>);
};

struct SetQuery {
    DTO_MEMBER(extension_id, ExtensionId);
    DTO_MEMBER(storage_id, std::string);
    DTO_MEMBER(key_values, std::vector<KeyValue>);
    DTO_MEMBER(quota, std::optional<Quota>);
};

struct RemoveQuery {
    DTO_MEMBER(extension_id, ExtensionId);
    DTO_MEMBER(storage_id, std::string);
    DTO_MEMBER(keys, std::vector<std::string>);
};

struct ClearQuery {
    DTO_MEMBER(extension_id, ExtensionId);
    DTO_MEMBER(storage_id, std::string);
};

struct Change {
    DTO_MEMBER(key, std::string);
    DTO_MEMBER(old_value, std::optional<std::string>);
    DTO_MEMBER(new_value, std::optional<std::string>);
};

struct MutationReply {
    DTO_MEMBER(err, Error);
    DTO_MEMBER(changes, std::vector<Change>);
};

} // namespace orion::ExtensionStorageAreas

namespace orion::ExtensionStorageAccessLevels {

struct GetQuery {
    DTO_MEMBER(extension_id, ExtensionId);
    DTO_MEMBER(storage_id, std::string);
    DTO_MEMBER(default_allow_untrusted_contexts, bool);
};

struct GetReply {
    DTO_MEMBER(err, Error);
    DTO_MEMBER(allow_untrusted_contexts, bool);
};

struct SetQuery {
    DTO_MEMBER(extension_id, ExtensionId);
    DTO_MEMBER(storage_id, std::string);
    DTO_MEMBER(allow_untrusted_contexts, bool);
};

} // namespace orion::ExtensionStorageAccessLevels
