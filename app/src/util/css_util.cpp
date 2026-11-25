#include "app.h"

#include "css_util.h"

#include <core/settings.h>

#include <common/src/string_util.h>

namespace orion {
    
std::string
font_name_to_css_class_name(orion::settings::Font font)
{
    switch(font)
    {
        case orion::settings::Font::Merriweather:
        case orion::settings::Font::Charter:
        case orion::settings::Font::Gelasio:
        case orion::settings::Font::TeXGyrePagella:
        case orion::settings::Font::Ubuntu:
        case orion::settings::Font::Vollkorn:
        case orion::settings::Font::LiberationSerif:
            {
                std::string name = rfl::enum_to_string(font);
                std::string result{};
                result.reserve(name.size());
                for(unsigned char c : name)
                {
                    result.push_back(std::tolower(c));
                }
                return result;
            }
        case orion::settings::Font::NewYork:
            return "uiserif";
        case orion::settings::Font::Dyslexic:
            return "dyslectic";
        case orion::settings::Font::not_set:
        case orion::settings::Font::System:
            return "system";
    }
    return "system";
}

std::string
font_name_to_css_font_family(orion::settings::Font font)
{
    switch(font)
    {
        case orion::settings::Font::Merriweather:
        case orion::settings::Font::Charter:
        case orion::settings::Font::Gelasio:
        case orion::settings::Font::Ubuntu:
        case orion::settings::Font::Vollkorn:
            return rfl::enum_to_string(font);
        case orion::settings::Font::TeXGyrePagella:
            return "TeX Gyre Pagella";
        case orion::settings::Font::NewYork:
            return "ui-serif";
        case orion::settings::Font::LiberationSerif:
            return "Liberation Serif";
        case orion::settings::Font::Dyslexic:
            return "OpenDyslexic3";
        case orion::settings::Font::not_set:
        case orion::settings::Font::System:
            return "";
    }
    return "";
}

std::string
reader_mode_width_script(int64_t new_width)
{
    //orion::log_debug("%li", new_width);
    std::string script = std::format(R"(
        document.getElementById('article').style.maxWidth = '{}px';
        reader.layOutContent();
    )", new_width);
    return script;
}



std::string page_tweaks_element_blocker_css_from_selectors_setting(const std::string& setting)
{
    if(setting.length() <= 1)
    {
        return "";
    }
    
    const std::string_view rule = R"(
        {
            display: none !important;
        }
    )";
    std::string result;

    result.reserve(setting.length() + rule.length());
    result.append(setting);
    result.append(rule);
    return result;
}

std::string_view
page_tweaks_font_size_css_from_font_size_setting(orion::settings::PageZoom setting_val)
{
    //return R"(main, article, .post-content {zoom: calc((" #val " / 100.0)); })"
#define RETURN_STRING_CASE(val)                                                             \
case orion::settings::PageZoom::zoom_##val:                                                 \
    return                                                                                  \
        "p, h1, h2, h3, h4, h5, h6, li, blockquote, td, th, figcaption, label, button {\n"  \
        "   zoom:" #val "%;\n"                                                              \
        "}\n"                                                                               \
        "img, video, iframe, table {\n"                                                     \
        "   max-width: 100% !important;\n"                                                  \
        "   height: auto;\n"                                                                \
        "}\n"                                                                               \
        "body {\n"                                                                          \
        "   overflow-wrap: break-word;\n"                                                   \
        "   word-wrap: break-word;\n"                                                       \
        "}\n"                                                                               \
        "li li, li p {\n"                                                                   \
        "   zoom: 1 !important;\n"                                                          \
        "}"

    switch(setting_val)
    {
        RETURN_STRING_CASE(50);
        RETURN_STRING_CASE(75);
        RETURN_STRING_CASE(80);
        RETURN_STRING_CASE(85);
        RETURN_STRING_CASE(90);
        RETURN_STRING_CASE(95);
        //RETURN_STRING_CASE(100);
        RETURN_STRING_CASE(115);
        RETURN_STRING_CASE(125);
        RETURN_STRING_CASE(150);
        RETURN_STRING_CASE(175);
        RETURN_STRING_CASE(200);
        RETURN_STRING_CASE(250);
        RETURN_STRING_CASE(300);
        case orion::settings::PageZoom::zoom_100:
        case orion::settings::PageZoom::not_set:
            return "";
    }

#undef RETURN_STRING_CASE

    return "";
}

std::string
page_tweaks_font_family_css_from_font_family_setting(const std::string& val)
{
    if(val.length())
    {
        return std::format("* {{ font-family: {}; }}", val);
    }
    else
    {
        return "";
    }
}



std::vector<char*>
webkit_allow_list_for_hostname_vec(const std::vector<std::string>& hostnames, const std::vector<std::string>& disabled_hostnames)
{
    
    std::vector<char*> actual_filter;
    actual_filter.reserve((hostnames.size()) * 2 + 1);
    for(const std::string& website : hostnames)
    {
        auto it = std::find(disabled_hostnames.cbegin(), disabled_hostnames.cend(), website);
        if(it == disabled_hostnames.cend())
        {
            orion::log_verbose("website: %s", website.c_str());
            actual_filter.push_back(g_strdup_printf("*://%s/*", website.c_str()));
            actual_filter.push_back(g_strdup_printf("*://*.%s/*", website.c_str()));
        }
    }

    actual_filter.push_back(NULL);
    return actual_filter;
}

void
webkit_allow_list_free(std::vector<char*>& allow_list)
{
    for(auto it = allow_list.begin(); it != allow_list.end()-1; ++it)
    {
        g_free(*it);
    }
}



unsigned long count_css_selectors(std::string_view sel)
{
    //TODO: FIXME: implement handling parenthesis and such
    return orion::str::count_occurences_of_char(sel, ',') + ((sel.length())?1:0);
}





CssClass::CssClass(std::string id, std::string css) : m_id(id), m_css(css) {
}

CssClass::CssClass(const CssClass& other) {
    m_id = other.m_id;
    m_css = other.m_css;
}

CssClass::CssClass(CssClass&& other) {
    m_id = std::move(other.m_id);
    m_css = std::move(other.m_css);
}

CssClass::~CssClass() {

}

std::string CssClass::id() {
    return m_id;
}

std::string CssClass::css() {
    return m_css;
}

std::string CssClass::full_class() {
    return std::format(R"(
        <style id="{}">
        {}
        </style>
    )", m_id, m_css);
}

} // namespace orion