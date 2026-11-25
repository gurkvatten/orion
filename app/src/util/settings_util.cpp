#include "app.h"

#include "settings_util.h"
#include "variant_traits.h"

#include <core/settings.h>

namespace orion {

bool
parse_website_specific_setting_key(const std::string& key, std::string* website, std::string* key_name)
{
    if (key.compare(0, std::strlen(website_settings_key_prefix), website_settings_key_prefix) != 0) {
        return false;
    }

    size_t setting_delimiter = key.rfind("::");
    if (setting_delimiter == std::string::npos || setting_delimiter <= std::strlen(website_settings_key_prefix)) {
        return false;
    }

    if(website) *website = key.substr(std::strlen(website_settings_key_prefix), setting_delimiter - std::strlen(website_settings_key_prefix));
    if(key_name) *key_name = key.substr(setting_delimiter + 2);
    return (website?(!website->empty()):true) && (key_name?(!key_name->empty()):true);
}


namespace gtk {


void
set_setting_from_variant(std::string key, GVariant* var, orion::ProfileId profile_id)
{
    const orion::settings::ISetting* setting = nullptr;
    Error err = orion::settings::get_isetting_for_setting_key(key, &setting);
    if(err)
    {
        LOG_ERROR(err);
        return;
    }

    std::string value;
    
    const GVariantType* type = g_variant_get_type(var);

    if(!g_variant_is_of_type(var, VariantTraits<std::string>::type()))
    {
        log_error("got wrong gvariant type when attempting to set setting (type: %s), expected string", g_variant_get_type_string(var));
        return;
    }

    
    

    gsize len = 0;
    const gchar* c_str = g_variant_get_string(var, &len);


    orion::SetSettingQuery query {
        .profile_id = profile_id,
        .key = key,
        .realm = setting->realm(),
        .key_name = setting->key_name(),
        .type = setting->type(),
        .value = c_str ? std::string(c_str, len) : "",
        .default_value = setting->default_value_as_string()
    };



    
    global_core_api->set_setting_raw(query, [](orion::SetSettingReply<std::string> reply){
        VERIFY_LOG(reply.err);
    });
}

}


} // namespace orion