#include "app_password_storage.h"

#include "platform/libsecret/libsecret.h"

namespace orion {

namespace {

class LibsecretPasswordStorage final : public PasswordStorage {
public:
    Error
    upsert_password_items(const char* app_id, const std::vector<PasswordItemDto>& items) override
    {
        return libsecret::upsert_password_items(app_id, items);
    }

    Error
    get_password_items(const char* app_id, std::vector<PasswordItemDto>* out_items) override
    {
        return libsecret::get_password_items(app_id, out_items);
    }
};

LibsecretPasswordStorage g_libsecret_password_storage;

} // namespace

PasswordStorage*
app_password_storage()
{
    return &g_libsecret_password_storage;
}

} // namespace orion
