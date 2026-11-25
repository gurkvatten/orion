#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "app.h"

#define PROFILES_ICON_SELECTION_BOX_TYPE (profiles_icon_selection_box_get_type ())

G_DECLARE_FINAL_TYPE(ProfilesIconSelectionBox, profiles_icon_selection_box, PROFILES, ICON_SELECTION_BOX, GtkBox)

ProfilesIconSelectionBox* profiles_icon_selection_box_new();
const char* profiles_icon_selection_get_current_selection(ProfilesIconSelectionBox* self);
void profiles_icon_selection_set_selection(ProfilesIconSelectionBox* self, const char* icon);
void profiles_icon_selection_bind_to_profile_id(ProfilesIconSelectionBox* self, orion::ProfileId id);
