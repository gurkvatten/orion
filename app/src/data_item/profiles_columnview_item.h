#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "app.h"
#include <string>

#define PROFILES_COLUMNVIEW_ITEM_TYPE (profiles_columnview_item_get_type())

G_DECLARE_FINAL_TYPE(ProfilesColumnviewItem, profiles_columnview_item, PROFILES_COLUMNVIEW, ITEM, GObject)

ProfilesColumnviewItem* profiles_columnview_item_new_from_dto(const orion::ProfileDto& dto, uint num_windows, uint num_tabs);

GString* profiles_columnview_item_get_icon(ProfilesColumnviewItem* item);
GString* profiles_columnview_item_get_profile(ProfilesColumnviewItem* item);
uint profiles_columnview_item_get_windows(ProfilesColumnviewItem* item);
uint profiles_columnview_item_get_tabs(ProfilesColumnviewItem* item);
bool profiles_columnview_item_get_default(ProfilesColumnviewItem* item);
bool profiles_columnview_item_get_synced(ProfilesColumnviewItem* item);
bool profiles_columnview_item_get_is_internal(ProfilesColumnviewItem* item);
double profiles_columnview_item_get_seq_num(ProfilesColumnviewItem* item);
orion::ProfileId profiles_columnview_item_get_id(ProfilesColumnviewItem* item);

void profiles_columnview_item_set_icon(ProfilesColumnviewItem* item, GString* icon);
void profiles_columnview_item_set_profile(ProfilesColumnviewItem* item, GString* profile);
void profiles_columnview_item_set_windows(ProfilesColumnviewItem* item, uint windows);
void profiles_columnview_item_set_tabs(ProfilesColumnviewItem* item, uint tabs);
void profiles_columnview_item_set_default(ProfilesColumnviewItem* item, bool is_default);
void profiles_columnview_item_set_synced(ProfilesColumnviewItem* item, bool is_synced);

int profiles_columnview_item_seq_num_compare(gconstpointer a, gconstpointer b, gpointer user_data);

typedef struct {
    GArray* array;
} ProfileArray;

GType profile_array_get_type (void) G_GNUC_CONST;
#define PROFILE_ARRAY_TYPE (profile_array_get_type())

ProfileArray* profile_array_new();
ProfileArray* profile_array_copy(ProfileArray* self);
void profile_array_free(ProfileArray* self);