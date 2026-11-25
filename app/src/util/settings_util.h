#pragma once
#include <glib.h>
#include <gio/gio.h>
#include <string>
#include <core/settings_types.h>


namespace orion {

inline constexpr const char* website_settings_key_prefix = "settings::website::specific::";
bool parse_website_specific_setting_key(const std::string& key, std::string* website, std::string* key_name);

namespace gtk {



void set_setting_from_variant(std::string key, GVariant* var, orion::ProfileId profile_id); //only takes string variants (marshalled settings)

template <typename T>
GVariant* new_variant_from_setting_val(T raw_setting)
{
    return g_variant_new_string(orion::settings::marshal_value<T>(raw_setting).c_str());
}

constexpr std::string
setting_action_name_to_db_key(std::string name)
{
    std::string res;
    res.reserve(name.size());

    for(char c : name)
    {
        if(c == '.') {
            res.append("::");
        } else if (c == '-') {
            res.push_back('_');
        } else {
            res.push_back(c);
        }
    }

    return res;
}

constexpr std::string
setting_db_key_to_action_name(std::string key)
{
    //reformats double colons to dots, underscores to dashes

    std::string res;

    //will increase memory overhead as these strings will be kept fro the duration of the profile data holders existence
    //and the result will be shorter or same length as key, too small to realistically matter though
    res.reserve(key.size());

    for(size_t i = 0; i < key.size(); ++i) {
        if (key[i] == '_') {
            res.push_back('-');
        } 
        else if (key[i] == ':' && i + 1 < key.size() && key[i+1] == ':') {
            if(i + 1 < key.size() && key[i+1] == ':')
            {
                res.push_back('.');
                ++i;
            }
            else
            {
                orion::log_error("got unexpected setting key format: %s", key.c_str());
                res.push_back(key[i]);
            }
        } 
        else {
            res.push_back(key[i]);
        }
    }
    
    return res;
}




}

} // namespace orion