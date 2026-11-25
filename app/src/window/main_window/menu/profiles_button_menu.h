#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include <core/core_api.h>
#include "window/main_window/main_window.h"

void recreate_profiles_menu(GMenu* menu, orion::ProfileId profile_id, MainWindow* win, std::function<void()> done_cb);