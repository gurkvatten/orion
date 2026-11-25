#include "text_util.h"
#include <gtk/gtk.h>
#include "../window/main_window/main_window.h"
#include <libintl.h>
#include <common/src/log.h>
#include <vector>
#include <functional>
#include <cstring>
#include <algorithm>
#include <common/src/defer.h>

namespace orion::text {

int
get_text_pixel_width(const char* text, PangoContext* context)
{
    PangoLayout* layout = pango_layout_new(context);

    pango_layout_set_text(layout, text, strlen(text));
    int width, height;
    pango_layout_get_pixel_size(layout, &width, &height);

    g_object_unref(layout);
    return width;
}

gboolean
text_is_emoji(const char* text, PangoFontMap* font_map)
{
    //TODO: FIXME: currently just tests rendered dimensions because
    //even g_utf8_strlen doesn't return the actual length of the rendered string,
    //instead returning codepoint count
    PangoContext* context = pango_font_map_create_context(font_map);
    defer([context]{ g_object_unref(context); });

    int pixel_width = get_text_pixel_width(text, context);
    if(pixel_width < 25 && pixel_width>10)
    {
        return true;
    }
    else
    {
        return false;
    }
}

// Use positional arguments, eg., "%1$s".
std::string
translatef(const char* format, ...)
{
    const char* translated_format = _(format);

    std::vector<char> buffer(1024);
    va_list args;
    va_start(args, format);
    int a = vsnprintf(buffer.data(), buffer.size(), translated_format, args);
    g_assert(a >= 0 && (size_t) a <= buffer.size());
    va_end(args);

    return std::string(buffer.data());
}

char*
n_items_get_text(uint n)
{
    GString* string = g_string_new("");
    g_string_printf(string, ngettext("%u item", "%u items", n), n);
    return g_string_free(string, false);
}

std::string
ngettext_string(const char* singular, const char* plural, uint n)
{
    GString* string = g_string_new("");
    g_string_printf(string, ngettext(singular, plural, n), n);
    std::string res(string->str);
    g_string_free(string, true);
    return res;
}

std::string
join_strings(const std::vector<std::string>& parts, const std::string& sep)
{
    std::string result;
    for (size_t i = 0; i < parts.size(); ++i) {
        result += parts[i];
        if (i + 1 < parts.size()) {
            result += sep;
        }
    }
    return result;
}

std::string
escape_html_text(const std::string& text)
{
    gchar* escaped_text = g_markup_escape_text(text.c_str(), -1);
    defer([escaped_text]{ g_free(escaped_text); });
    std::string result{};
    if (escaped_text) {
        result = escaped_text;
    }
    return result;
}

std::string
unescape_html_text(const std::string& text)
{
    // g_markup_unescape_text does not exist, but g_markup_unescape_string should work well too
    return unescape_uri(text);
}

std::string
escape_uri(const std::string& text)
{
    char* escaped = g_uri_escape_string(text.c_str(), NULL, TRUE);
    defer([escaped]{ g_free(escaped); });
    std::string result{};
    if (escaped) {
        result = escaped;
    }
    return result;
}

std::string
unescape_uri(const std::string& text)
{
    char* unescaped = g_uri_unescape_string(text.c_str(), NULL);
    defer([unescaped]{ g_free(unescaped); });
    std::string result{};
    if (unescaped) {
        result = unescaped;
    }
    return result;
}

std::string
ellipsize_string(const std::string& source, uint max_length/*, bool is_pango_markup*/)
{
    uint length = 0;
    // if(is_pango_markup)
    // {
    //     gchar* plain_text = NULL;
    //     GError* err = NULL;
    //     pango_parse_markup(source.c_str(), source.length(), 0, NULL, &plain_text, NULL, &err);
    //     if(err)
    //     {
    //         orion::log_error("pango markup parsing failed while attempting ellipsis on: %s", source.c_str());
    //         g_error_free(err);
    //         return "";
    //     }
    //     defer([plain_text]{ g_free(plain_text); });
    //     length = g_utf8_strlen(source.c_str(), -1);
    // }
    // else
    {
        length = g_utf8_strlen(source.c_str(), -1);
    }
    std::string ellipsized{};
    if(length < max_length)
    {
        ellipsized = source;
    }
    else
    {
        char* utf8_substr = g_utf8_substring(source.c_str(), 0, max_length);
        ellipsized = utf8_substr;
        g_free(utf8_substr);
        ellipsized.append("…");
    }
    return ellipsized;
}

} // namespace orion::text