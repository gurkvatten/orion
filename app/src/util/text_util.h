#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include <string>
#include <vector>

namespace orion::text {

//calculates text width based on given pango context
int get_text_pixel_width(const char* text, PangoContext* context);

gboolean text_is_emoji(const char* text, PangoFontMap* font_map);

// Use positional arguments, for example:
//    "%1$s" - first argument as a string
//    "%2$d" - second argument as an integer
//    etc.
std::string translatef(const char* format, ...) /*__attribute__ ((__format__ (__gnu_printf__, 1, 2)))*/;

//caller takes ownership of the data and is responsible for freeing it using g_free
char* n_items_get_text(uint n);

std::string ngettext_string(const char* singular, const char* plural, uint n);

std::string escape_html_text(const std::string& text);

std::string unescape_html_text(const std::string& text);

std::string escape_uri(const std::string& text);

std::string unescape_uri(const std::string& text);

std::string ellipsize_string(const std::string& source, uint max_length);

} // namespace orion::text