#pragma once

#include "app.h"

namespace orion {

void register_accels(OrionApp* app);
void undo_tab_close_accel_set_enabled(OrionApp* app, bool is_enabled);
void register_ctrl_num_accels(OrionApp* app);

}