#pragma once

#include <common/src/error.h>
#include <core/dto/password_item_dto.h>
#include <vector>

namespace orion {

class PasswordStorage {
public:
    virtual ~PasswordStorage() = default;

    virtual Error upsert_password_items(const char* app_id, const std::vector<PasswordItemDto>& items) = 0;
    virtual Error get_password_items(const char* app_id, std::vector<PasswordItemDto>* out_items) = 0;
};

} // namespace orion
