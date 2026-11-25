#pragma once

#include <core/dto/password_item_dto.h>

namespace orion::libsecret {

Error upsert_password_items(const char* app_id, const std::vector<PasswordItemDto>& items);
Error get_password_items(const char* app_id, std::vector<PasswordItemDto>* out_items);
void test_libsecret(const char* app_id);
void log_password_items(const char* app_id);

} // namespace orion::libsecret
