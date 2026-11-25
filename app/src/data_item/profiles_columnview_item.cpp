#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "data_item/profiles_columnview_item.h"

struct _ProfilesColumnviewItem //getting too long, should move this away
{
	GObject parent;
    GString* icon;
    GString* profile;
    uint windows;
    uint tabs;
    orion::ProfileId id;
    bool is_default;
    bool is_synced;
    bool is_internal;
    double seq_num;
};

G_DEFINE_TYPE(ProfilesColumnviewItem, profiles_columnview_item, G_TYPE_OBJECT)

void
profiles_columnview_item_finalize(GObject* object)
{
    ProfilesColumnviewItem* item = PROFILES_COLUMNVIEW_ITEM(object);
    g_string_free(item->icon, TRUE);
    g_string_free(item->profile, TRUE);
    G_OBJECT_CLASS(profiles_columnview_item_parent_class)->finalize(object);
}

void
profiles_columnview_item_init(ProfilesColumnviewItem* item)
{

}

static void
profiles_columnview_item_class_init(ProfilesColumnviewItemClass* klass)
{
    G_OBJECT_CLASS(klass)->finalize = profiles_columnview_item_finalize;
}

ProfilesColumnviewItem*
profiles_columnview_item_new_from_dto(const orion::ProfileDto& dto, uint num_windows, uint num_tabs)
{
	ProfilesColumnviewItem* item = static_cast<ProfilesColumnviewItem*>(g_object_new(PROFILES_COLUMNVIEW_ITEM_TYPE, NULL));
    item->icon = g_string_new(dto.icon_name.c_str());
    item->profile = g_string_new(dto.name.c_str());
    item->seq_num = dto.seq_num;
    item->is_default = dto.is_default;
    item->is_internal = dto.is_internal;
    item->id = dto.id;
    item->windows = num_windows;
    item->tabs = num_tabs;
    return item;
}

GString*
profiles_columnview_item_get_icon(ProfilesColumnviewItem* item)
{
    return PROFILES_COLUMNVIEW_ITEM(item)->icon;
}

GString*
profiles_columnview_item_get_profile(ProfilesColumnviewItem* item)
{
    return PROFILES_COLUMNVIEW_ITEM(item)->profile;
}

uint
profiles_columnview_item_get_windows(ProfilesColumnviewItem* item)
{
    return PROFILES_COLUMNVIEW_ITEM(item)->windows;
}

uint
profiles_columnview_item_get_tabs(ProfilesColumnviewItem* item)
{
    return PROFILES_COLUMNVIEW_ITEM(item)->tabs;
}

bool
profiles_columnview_item_get_default(ProfilesColumnviewItem* item)
{
    return PROFILES_COLUMNVIEW_ITEM(item)->is_default;
}

bool
profiles_columnview_item_get_synced(ProfilesColumnviewItem* item)
{
    return PROFILES_COLUMNVIEW_ITEM(item)->is_synced;
}

bool
profiles_columnview_item_get_is_internal(ProfilesColumnviewItem* item)
{
    return PROFILES_COLUMNVIEW_ITEM(item)->is_internal;
}

double
profiles_columnview_item_get_seq_num(ProfilesColumnviewItem* item)
{
    return PROFILES_COLUMNVIEW_ITEM(item)->seq_num;
}

orion::ProfileId
profiles_columnview_item_get_id(ProfilesColumnviewItem* item)
{
    return PROFILES_COLUMNVIEW_ITEM(item)->id;
}



void
profiles_columnview_item_set_icon(ProfilesColumnviewItem* item, GString* icon)
{
    PROFILES_COLUMNVIEW_ITEM(item)->icon = icon;
}

void
profiles_columnview_item_set_profile(ProfilesColumnviewItem* item, GString* profile)
{
    PROFILES_COLUMNVIEW_ITEM(item)->profile = profile;
}

void
profiles_columnview_item_set_windows(ProfilesColumnviewItem* item, uint windows)
{
    PROFILES_COLUMNVIEW_ITEM(item)->windows = windows;
}

void
profiles_columnview_item_set_tabs(ProfilesColumnviewItem* item, uint tabs)
{
    PROFILES_COLUMNVIEW_ITEM(item)->tabs = tabs;
}

void
profiles_columnview_item_set_default(ProfilesColumnviewItem* item, bool is_default)
{
    PROFILES_COLUMNVIEW_ITEM(item)->is_default = is_default;
}

void
profiles_columnview_item_set_synced(ProfilesColumnviewItem* item, bool is_synced)
{
    PROFILES_COLUMNVIEW_ITEM(item)->is_synced = is_synced;
}

int
profiles_columnview_item_seq_num_compare(gconstpointer a, gconstpointer b, gpointer user_data)
{
    double seq_num_a = profiles_columnview_item_get_seq_num(static_cast<ProfilesColumnviewItem*>(const_cast<gpointer>(a)));
    double seq_num_b = profiles_columnview_item_get_seq_num(static_cast<ProfilesColumnviewItem*>(const_cast<gpointer>(b)));
    if(seq_num_a>seq_num_b)
    {
        return 1;
    }
    else if (seq_num_a<seq_num_b)
    {
        return -1;
    }
    else
    {
        return 0;
    }
}



ProfileArray* profile_array_new()
{
    ProfileArray* res = g_new(ProfileArray, 1);
    if(res!=NULL)
    {
        res->array = g_array_new(false, false, sizeof(orion::ProfileId));
    }
    return res;
}


ProfileArray* profile_array_copy(ProfileArray* self)
{
    ProfileArray* res = g_new(ProfileArray, 1);
    if(res!=NULL)
    {
        res->array = g_array_copy(self->array);
    }
    return res;
}

void profile_array_free(ProfileArray* self)
{
    g_array_free(self->array, true);
    g_free(self);
}


G_DEFINE_BOXED_TYPE(ProfileArray, profile_array, profile_array_copy, profile_array_free);

