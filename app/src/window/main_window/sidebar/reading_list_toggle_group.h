#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>

#define READING_LIST_TOGGLE_GROUP_TYPE (reading_list_toggle_group_get_type())

G_DECLARE_FINAL_TYPE(ReadingListToggleGroup, reading_list_toggle_group, READING_LIST, TOGGLE_GROUP, GtkBox)

AdwToggleGroup* reading_list_toggle_group_get_unread_toggle_group(ReadingListToggleGroup *self);
