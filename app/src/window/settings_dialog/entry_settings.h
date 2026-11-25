#pragma once

#include <gtk/gtk.h>

void on_homepage_entry_map(GtkEntry* self, gpointer user_data);
void on_homepage_entry_notify_text(GtkEntry* self, GParamSpec* pspec, gpointer user_data);

void on_homepage_entry_map(GtkEntry* self, gpointer user_data);
void on_homepage_entry_notify_text(GtkEntry* self, GParamSpec* pspec, gpointer user_data);

void on_custom_user_agent_entry_map(GtkEntry* self, gpointer user_data);
void on_custom_user_agent_entry_notify_text(GtkEntry* self, GParamSpec* pspec, gpointer user_data);