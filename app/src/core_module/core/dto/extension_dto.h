#pragma once

#include <string>
#include <common/src/dto/dto_member.h>
#include <core/dto/profile_dto.h>

namespace orion {

DECLARE_STRONG_TYPEDEF(ExtensionId, std::string); // UUID
// Possible SKU format: DEVELOPER:EXTENSION_TYPE:EXTENSION_NAME
//   - example: com.kagi.orion:firefox:custom-password-manager
DECLARE_STRONG_TYPEDEF(ExtensionSku, std::string);

enum class ExtensionType {
    not_set    = 0,
    firefox    = 1,
    chrome     = 2,
    gresources = 3, // local file embedded in GResources
};

struct ExtensionDto {
    DTO_MEMBER(id, ExtensionId); // UUID
    DTO_MEMBER(profile_id, ProfileId, ProfileId(0));
    DTO_MEMBER(sku, ExtensionSku);                           // the combination of profile_id, sku, and type must be unique
    DTO_MEMBER(type, ExtensionType, ExtensionType::not_set); // type of the extension (e.g., Firefox or Chrome)
    DTO_MEMBER(file_path, std::string);                      // local path to the extension package file (zip archive)
    DTO_MEMBER(name, std::string);
    DTO_MEMBER(description, std::string);
    DTO_MEMBER(icon_ref, std::string); // icon name, path (URI) to the icon file, or icon data (whichever is appropriate)
    DTO_MEMBER(is_enabled, bool, true);
    DTO_MEMBER(last_updated_at, UnixTimeUtc64, UnixTimeUtc64(0));
    DTO_MEMBER(activate_shortcut, std::string);              // keyboard shortcut to activate the extension
    DTO_MEMBER(lock_shortcut, std::string);                  // keyboard shortcut to lock the extension
    DTO_MEMBER(version, std::string);                        // version of the extension (eg., "1.62.0")
    DTO_MEMBER(do_not_update, bool, false);                  // if true, the extension will not be updated automatically
    DTO_MEMBER(has_options, bool, false);                    // if true, the extension has options page
    DTO_MEMBER(seq_num, double, 0.0);                        // cannot be updated explicitly
};

struct GetInstalledExtensionIdsQuery {
    DTO_MEMBER(profile_id, ProfileId, ProfileId(0));
};

struct GetAllInstalledExtensionIdsReply {
    DTO_MEMBER(err, Error);
    DTO_MEMBER(ids, std::vector<ExtensionId>);
};

struct GetInstalledExtensionsQuery {
    DTO_MEMBER(ids, std::vector<ExtensionId>);
};

struct GetInstalledExtensionsReply {
    DTO_MEMBER(err, Error);
    DTO_MEMBER(extensions, std::vector<ExtensionDto>);
};

struct InstallExtensionQuery {
    DTO_MEMBER(profile_id, ProfileId, ProfileId(0));
    DTO_MEMBER(sku, ExtensionSku);                           // see ExtensionDto::sku field
    DTO_MEMBER(type, ExtensionType, ExtensionType::not_set); // type of the extension (e.g., Firefox or Chrome)
    DTO_MEMBER(file_path, std::string);
    DTO_MEMBER(name, std::string);
    DTO_MEMBER(description, std::string);
    DTO_MEMBER(icon_ref, std::string);
};

struct InstallExtensionReply {
    DTO_MEMBER(err, Error);
    DTO_MEMBER(id, ExtensionId);
};

struct UninstallExtensionQuery {
    DTO_MEMBER(id, ExtensionId);
};

struct UninstallExtensionReply {
    DTO_MEMBER(err, Error);
};

struct UpdateExtensionQuery {
    DTO_MEMBER(id, ExtensionId); // UUID of the extension to update

    DTO_MEMBER(profile_id, ProfileId, ProfileId(0));
    DTO_MEMBER(sku, ExtensionSku); // see ExtensionDto fields
    DTO_MEMBER(type, ExtensionType, ExtensionType::not_set); // type of the extension (e.g., Firefox or Chrome)
    DTO_MEMBER(file_path, std::string);
    DTO_MEMBER(name, std::string);
    DTO_MEMBER(description, std::string);
    DTO_MEMBER(icon_ref, std::string); // icon name, path (URI) to the icon file, or icon data (whichever is appropriate)
    DTO_MEMBER(is_enabled, bool, true);
    DTO_MEMBER(last_updated_at, UnixTimeUtc64, UnixTimeUtc64(0));
    DTO_MEMBER(activate_shortcut, std::string);              // keyboard shortcut to activate the extension
    DTO_MEMBER(lock_shortcut, std::string);                  // keyboard shortcut to lock the extension
    DTO_MEMBER(version, std::string);                        // version of the extension (eg., "1.62.0")
    DTO_MEMBER(do_not_update, bool, false);                  // if true, the extension will not be updated automatically
    DTO_MEMBER(has_options, bool, false);                    // if true, the extension has options page

    DTO_MEMBER(update_profile_id, bool, false);
    DTO_MEMBER(update_sku, bool, false);
    DTO_MEMBER(update_type, bool, false);
    DTO_MEMBER(update_file_path, bool, false);
    DTO_MEMBER(update_name, bool, false);
    DTO_MEMBER(update_description, bool, false);
    DTO_MEMBER(update_icon_ref, bool, false);
    DTO_MEMBER(update_is_enabled, bool, false);
    DTO_MEMBER(update_last_updated_at, bool, false);
    DTO_MEMBER(update_activate_shortcut, bool, false);
    DTO_MEMBER(update_lock_shortcut, bool, false);
    DTO_MEMBER(update_version, bool, false);
    DTO_MEMBER(update_do_not_update, bool, false);
    DTO_MEMBER(update_has_options, bool, false);
};

struct UpdateExtensionReply {
    DTO_MEMBER(err, Error);
};

struct ExtensionsChange {
    DTO_MEMBER(created, std::vector<ExtensionDto>);
    DTO_MEMBER(updated, std::vector<ExtensionDto>);
    DTO_MEMBER(deleted, std::vector<ExtensionId>);

    DTO_MEMBER(updated_profile_id, bool, false);
    DTO_MEMBER(updated_sku, bool, false);
    DTO_MEMBER(updated_type, bool, false);
    DTO_MEMBER(updated_file_path, bool, false);
    DTO_MEMBER(updated_name, bool, false);
    DTO_MEMBER(updated_description, bool, false);
    DTO_MEMBER(updated_icon_ref, bool, false);
    DTO_MEMBER(updated_is_enabled, bool, false);
    DTO_MEMBER(updated_last_updated_at, bool, false);
    DTO_MEMBER(updated_activate_shortcut, bool, false);
    DTO_MEMBER(updated_lock_shortcut, bool, false);
    DTO_MEMBER(updated_version, bool, false);
    DTO_MEMBER(updated_do_not_update, bool, false);
    DTO_MEMBER(updated_has_options, bool, false);
    DTO_MEMBER(updated_seq_num, bool, false); // cannot be updated explicitly
};

} // namespace orion