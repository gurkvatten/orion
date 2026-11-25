#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "app.h"
#include <string>

#define TAB_ITEM_TYPE (tab_item_get_type())

G_DECLARE_FINAL_TYPE(TabItem, tab_item, TAB, ITEM, GObject)

TabItem* tab_item_new(orion::TabDto tab);

GString* tab_item_get_title(TabItem* item);
GString* tab_item_get_address(TabItem* item);
double tab_item_get_seq_num(TabItem* item);
orion::TabId tab_item_get_id(TabItem* item);

void tab_item_set_title(TabItem* item, GString* title);
void tab_item_set_address(TabItem* item, GString* address);
void tab_item_set_seq_num(TabItem* item, double seq_num);
void tab_item_set_id(TabItem* item, orion::TabId id);

int tab_item_seq_num_compare(gconstpointer item_a, gconstpointer item_b, gpointer user_data);