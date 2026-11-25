#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "add_bookmark_location_dropdown_item.h"

enum FolderType {
    normal = 0,
    bookmarks = 1,
    bookmarks_bar = 2,
    favourites = 3,
    reading_list = 4
};

struct _Folder
{
    GObject parent;

    gchar* icon_name;
    gchar* name;
    AddBookmarkFolder type;
    orion::BookmarkFolderId id;
};

enum {
    PROP_0,
    PROP_ICON_NAME,
    PROP_NAME,
    PROP_TYPE,
    PROP_ID,
    N_NEW_PROPS
};

static GParamSpec* obj_properties[N_NEW_PROPS] = { NULL, };

G_DEFINE_TYPE(Folder, folder, G_TYPE_OBJECT);

G_DEFINE_ENUM_TYPE(AddBookmarkFolder, add_bookmark_folder,
    G_DEFINE_ENUM_VALUE(ADD_BOOKMARK_FOLDER_NORMAL, "normal"),
    G_DEFINE_ENUM_VALUE(ADD_BOOKMARK_FOLDER_BOOKMARKS, "bookmarks"),
    G_DEFINE_ENUM_VALUE(ADD_BOOKMARK_FOLDER_READING_LIST, "reading_list"))

static void
folder_finalize(GObject* object)
{
    Folder* folder = FOLDER_TYPE(object);
    if(folder->icon_name) g_free(folder->icon_name);
    if(folder->name) g_free(folder->name);
    G_OBJECT_CLASS(folder_parent_class)->finalize(object);
}

static void
folder_set_property(GObject* object, guint prop_id, const GValue* value, GParamSpec* pspec)
{
    Folder* self = FOLDER_TYPE(object);
    switch (prop_id) {
        case PROP_ICON_NAME:
            if(self->icon_name) g_free(self->icon_name);
            self->icon_name = g_strdup(g_value_get_string(value));
            break;
        case PROP_ID:
            if(self->type != ADD_BOOKMARK_FOLDER_NORMAL) g_return_if_reached();
            self->id = g_value_get_int64(value);
            break;
        case PROP_NAME:
            if(self->name) g_free(self->name);
            self->name = g_strdup(g_value_get_string(value));
            break;
        case PROP_TYPE:
            self->type = (AddBookmarkFolder)g_value_get_enum(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
folder_get_property(GObject* object, guint prop_id, GValue* value, GParamSpec* pspec)
{
    Folder* self = FOLDER_TYPE(object);
    switch (prop_id) {
        case PROP_ICON_NAME:
            g_value_set_string(value, self->icon_name);
            break;
        case PROP_ID:
            if(self->type == ADD_BOOKMARK_FOLDER_READING_LIST) g_return_if_reached();
            g_value_set_int64(value, self->id.value);
            break;
        case PROP_NAME:
            g_value_set_string(value, self->name);
            break;
        case PROP_TYPE:
            g_value_set_enum(value, self->type);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

Folder*
folder_new_from_bookmark_folder_dto(orion::BookmarkFolderDto dto)
{
    const char* icon_name;
    AddBookmarkFolder type = ADD_BOOKMARK_FOLDER_NORMAL;
    switch(dto.type)
    {
        case orion::BookmarkFolderType::normal:
            icon_name = "folder-visiting-symbolic";
            break;
        case orion::BookmarkFolderType::favorites:
            icon_name = "star-outline-thick-symbolic";
            break;
        case orion::BookmarkFolderType::bookmarks_bar:
            icon_name = "library-symbolic";
            break;
        default:
            icon_name = "folder-visiting-symbolic";
            break;
    }
    Folder* folder = (Folder*)g_object_new(folder_get_type(), "icon-name", icon_name,
        "name", dto.name.c_str(),"type", type, "id", dto.id.value, NULL);
    return folder;
}

Folder*
folder_new_bookmarks()
{
    const char* icon_name = "bookmark-filled-symbolic";
    AddBookmarkFolder type = ADD_BOOKMARK_FOLDER_BOOKMARKS;
    Folder* folder = (Folder*)g_object_new(folder_get_type(), "icon-name", icon_name,
        "name", _("Bookmarks"),"type", type, NULL);
    return folder;
}

Folder*
folder_new_reading_list()
{
    const char* icon_name = "open-book-symbolic";
    AddBookmarkFolder type = ADD_BOOKMARK_FOLDER_READING_LIST;
    Folder* folder = (Folder*)g_object_new(folder_get_type(), "icon-name", icon_name,
        "name", _("Reading List"),"type", type, NULL);
    return folder;
}

static void
folder_init(Folder* folder)
{
}

static void
folder_class_init(FolderClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);
    object_class->finalize = folder_finalize;


    obj_properties[PROP_ICON_NAME] = g_param_spec_string(
        "icon-name",
        "Icon name fir this folder", // nick
        "An extra argument passed at construction", // blurb
        "FAILED_TO_LOAD",                    
        GParamFlags(/*G_PARAM_CONSTRUCT_ONLY | */G_PARAM_READWRITE)
    );
    obj_properties[PROP_NAME] = g_param_spec_string(
        "name",
        "Name of this folder", // nick
        "An extra argument passed at construction", // blurb
        "FAILED_TO_LOAD",                    
        GParamFlags(/*G_PARAM_CONSTRUCT_ONLY | */G_PARAM_READWRITE)
    );
    obj_properties[PROP_TYPE] = g_param_spec_enum(
        "type",
        "Type of this folder used for obfuscating non homogeneous folders", // nick
        "An extra argument passed at construction", // blurb
        ADD_BOOKMARK_FOLDER_TYPE,
        ADD_BOOKMARK_FOLDER_NORMAL,                    
        GParamFlags(G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE)
    );
    obj_properties[PROP_ID] = g_param_spec_int64(
        "id",
        "Id of this bookmark folder", // nick
        "An extra argument passed at construction", // blurb
        G_MININT64, // minimum value
        G_MAXINT64, // maximum value
        0,  // default value
        GParamFlags(/*G_PARAM_CONSTRUCT_ONLY | */G_PARAM_READWRITE)
    );

    object_class->set_property = folder_set_property;
    object_class->get_property = folder_get_property;
    g_object_class_install_properties(object_class, N_NEW_PROPS, obj_properties);
}
