#pragma once

#include <glib.h>
#include <gio/gio.h>

#include <string>

#include <core/settings.h>

namespace orion {

std::string font_name_to_css_class_name(orion::settings::Font font);
std::string font_name_to_css_font_family(orion::settings::Font font);
std::string reader_mode_width_script(int64_t new_width);

std::string page_tweaks_element_blocker_css_from_selectors_setting(const std::string& setting);
std::string_view page_tweaks_font_size_css_from_font_size_setting(orion::settings::PageZoom setting_val);
std::string page_tweaks_font_family_css_from_font_family_setting(const std::string& val);

//resulting vector is the owner of these gchar* strings, either manually free or call webkit_allow_list_free
std::vector<char*> webkit_allow_list_for_hostname_vec(const std::vector<std::string>& hostnames, const std::vector<std::string>& disabled_hostnames);
void webkit_allow_list_free(std::vector<char*>& allow_list);

unsigned long count_css_selectors(std::string_view sel);

struct CssClass {
public:
    CssClass(std::string id, std::string css);
    CssClass(const CssClass&);
    CssClass(CssClass&&);
    ~CssClass();

    std::string id();
    std::string css();
    std::string full_class();
private:
    std::string m_id;
    std::string m_css;
};

} // namespace orion