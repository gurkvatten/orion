#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include <functional>
#include <core/core_api.h>

#include "window/main_window/main_window.h"

void update_profile_windows_submenu(
    GMenu* profile_windows_submenu, orion::ProfileId profile_id, MainWindow* win,
    std::function<void()> done_cb
);