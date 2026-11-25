#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "app.h"
#include <string>

#define WINDOW_ITEM_TYPE (window_item_get_type())

G_DECLARE_FINAL_TYPE(WindowItem, window_item, WINDOW, ITEM, GObject)

WindowItem* window_item_new(orion::WindowDto window);

// Getters

const char* window_item_get_title(WindowItem* item);
const char* window_item_get_icon_name(WindowItem* item);
const char* window_item_get_icon_label(WindowItem* item);
double window_item_get_seq_num(WindowItem* item);
orion::WindowId window_item_get_id(WindowItem* item);
bool window_item_get_is_suspended(WindowItem* item);
bool window_item_get_is_image_icon(WindowItem* item);

// Setters

void window_item_set_title(WindowItem* item, const char* title);
void window_item_set_icon_name(WindowItem* item, const char* icon_name);
void window_item_set_seq_num(WindowItem* item, double seq_num);
void window_item_set_id(WindowItem* item, orion::WindowId id);
void window_item_set_is_suspended(WindowItem* item, bool is_suspended);

// Other

int window_item_seq_num_compare(gconstpointer item_a, gconstpointer item_b, gpointer user_data);