#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>

#define ORION_CONTEXT_MENU_CONTROLLER_NAME "orion_context_menu_controller"

namespace orion {

//returns true if valid uri, sets locations pointed to by parameters to the corresponding widget, title, icon
//icon can be set to NULL if custom_uri_get_corresponding_icon fails
bool custom_uri_get_corresponding_widget(std::string_view uri, GtkWidget** widget, std::string* title, GIcon** icon);

//return value needs to be freed using g_free()
//returns NULL if invalid uri
std::string custom_uri_get_corresponding_title(std::string_view uri);


//returns NULL if invalid uri or something goes wrong for g_icon_new_for_string
GIcon* custom_uri_get_corresponding_icon(std::string_view uri);

} // namespace orion::gtk