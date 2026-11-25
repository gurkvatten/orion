#include "click_util.h"

#include <common/src/log.h>
#include <common/src/defer.h>
#include <vector>
#include <functional>
#include <cstring>

#include "window/main_window/main_window.h"
#include "window/search_recommendations.h"
#include "custom_uri_util.h"

namespace orion {

bool
custom_uri_get_corresponding_widget(std::string_view uri, GtkWidget** widget, std::string* title, GIcon** icon)
{
    if(uri=="orion://newtab/")
    {
        *title = custom_uri_get_corresponding_title(uri);
        *icon = custom_uri_get_corresponding_icon(uri);
        *widget = reinterpret_cast<GtkWidget*>(search_recommendations_new());
        return true;
    }
    else
    {
        return false;
    }
}

std::string
custom_uri_get_corresponding_title(std::string_view uri)
{
    if(uri=="orion://newtab/")
    {
        return std::string(_("Start Page"));
    }
    else
    {
        return "";
    }
}

GIcon*
custom_uri_get_corresponding_icon(std::string_view uri)
{
    GIcon* icon = NULL;
    GError* err = NULL;
    if(uri=="orion://newtab/")
    {
        icon = g_icon_new_for_string("compass-symbolic", &err);
    }
    if(err)
    {
        defer([err]{ g_error_free(err); });
        orion::log_error("custom_uri_get_corresponding_icon: error: " + std::string(err->message));
    }
    return icon;
}

} // namespace orion::gtk