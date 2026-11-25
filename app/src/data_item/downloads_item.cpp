#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include <common/src/defer.h>

#include "data_item/downloads_item.h"

namespace {
    struct CppWrapper {
        orion::DownloadId id;
        std::string name;
        std::string icon_string;
        std::filesystem::path file_path;
        orion::DownloadOperationState operation_state;
        std::string uri;
        orion::UnixTimeUtc64 timestamp = orion::UnixTimeUtc64(0);
        int64_t total_size = 0;
        int64_t current_size = 0;
        int64_t download_speed = 0;
        int64_t prev_current_size = 0;
        //in microseconds
        int64_t prev_current_size_time = 0;
    };
};

struct _DownloadsItem
{
	GObject parent;

    GIcon* icon;

    CppWrapper cpp_data;
};

G_DEFINE_TYPE(DownloadsItem, downloads_item, G_TYPE_OBJECT)

enum {
    PROP_0,
    PROP_ID,
    PROP_NAME,
    PROP_FILE_ICON,
    PROP_DOWNLOADS_PATH,
    PROP_OPERATION_STATE,
    PROP_ORIGIN_URI,
    PROP_TIME_STAMP,
    PROP_TOTAL_SIZE,
    PROP_CURRENT_SIZE,
    PROP_DOWNLOAD_SPEED,
    N_PROPS,
};

static GParamSpec* obj_properties[N_PROPS];

G_DEFINE_ENUM_TYPE(DownloadOperationState, download_operation_state,
    G_DEFINE_ENUM_VALUE(gint(orion::DownloadOperationState::downloading), "downloading"),
    G_DEFINE_ENUM_VALUE(gint(orion::DownloadOperationState::finished), "finished"),
    G_DEFINE_ENUM_VALUE(gint(orion::DownloadOperationState::paused), "paused"),
    G_DEFINE_ENUM_VALUE(gint(orion::DownloadOperationState::failed), "failed"),
    G_DEFINE_ENUM_VALUE(gint(orion::DownloadOperationState::cancelled), "cancelled"),
    G_DEFINE_ENUM_VALUE(gint(orion::DownloadOperationState::cancelling), "cancelling")
)

DownloadsItem*
downloads_item_new_from_dto(const orion::DownloadDto& dto)
{
    GIcon* icon = g_icon_new_for_string(dto.icon_string.c_str(), NULL);
    defer([icon]{ g_object_unref(icon); });
    return APP_DOWNLOADS_ITEM(g_object_new(downloads_item_get_type(),
        "id", dto.id.value,
        "file-path", dto.path.c_str(),
        "file-name", dto.name.c_str(),
        "file-icon", icon,
        "operation-state", int(dto.operation_state),
        "timestamp", dto.timestamp.value,
        "total-size", dto.total_size,
        "current-size", dto.current_size,
        "origin-uri", dto.origin_uri.c_str(),
        NULL 
    ));
}

gboolean
downloads_item_compare(gconstpointer item_a, gconstpointer item_b, gpointer user_data)
{
    auto downloads_item_a = static_cast<const DownloadsItem*>(item_a);
    auto downloads_item_b = static_cast<const DownloadsItem*>(item_b);
    return downloads_item_a->cpp_data.timestamp.value < downloads_item_b->cpp_data.timestamp.value;
}

orion::DownloadId
downloads_item_get_id(DownloadsItem* item)
{
    return APP_DOWNLOADS_ITEM(item)->cpp_data.id;
}

std::filesystem::path
downloads_item_get_downloads_path(DownloadsItem* item)
{
    return APP_DOWNLOADS_ITEM(item)->cpp_data.file_path;
}

std::string
downloads_item_get_file_name(DownloadsItem* item)
{
    return APP_DOWNLOADS_ITEM(item)->cpp_data.name;
}

std::string
downloads_item_get_origin_uri(DownloadsItem* item)
{
    return APP_DOWNLOADS_ITEM(item)->cpp_data.uri;
}

orion::DownloadOperationState
downloads_item_get_operation_state(DownloadsItem* item)
{
    return APP_DOWNLOADS_ITEM(item)->cpp_data.operation_state;
}

orion::UnixTimeUtc64
downloads_item_get_timestamp(DownloadsItem* item)
{
    return APP_DOWNLOADS_ITEM(item)->cpp_data.timestamp;
}

int64_t
downloads_item_get_total_size(DownloadsItem* item)
{
    return APP_DOWNLOADS_ITEM(item)->cpp_data.total_size;
}

int64_t
downloads_item_get_current_size(DownloadsItem* item)
{
    return APP_DOWNLOADS_ITEM(item)->cpp_data.current_size;
}

int64_t
downloads_item_get_download_speed(DownloadsItem* item)
{
    return APP_DOWNLOADS_ITEM(item)->cpp_data.download_speed;
}

GIcon*
downloads_item_get_file_icon(DownloadsItem* item)
{
    return APP_DOWNLOADS_ITEM(item)->icon;
}

static void
downloads_item_set_downloads_path(DownloadsItem* item, const gchar* path)
{
    std::string path_cpp = path?path:"";
    if(APP_DOWNLOADS_ITEM(item)->cpp_data.file_path != path_cpp)
    {
        item->cpp_data.file_path = path_cpp;
        g_object_notify_by_pspec(G_OBJECT(item), obj_properties[PROP_DOWNLOADS_PATH]);
    }
}

static void
downloads_item_set_name(DownloadsItem* item, const gchar* name)
{
    std::string name_cpp = name?name:"";
    if(APP_DOWNLOADS_ITEM(item)->cpp_data.name != name_cpp)
    {
        item->cpp_data.name = name_cpp;
        g_object_notify_by_pspec(G_OBJECT(item), obj_properties[PROP_NAME]);
    }
}

static void
downloads_item_set_operation_state(DownloadsItem* item, orion::DownloadOperationState state)
{
    if(APP_DOWNLOADS_ITEM(item)->cpp_data.operation_state != state)
    {
        item->cpp_data.operation_state = state;
        g_object_notify_by_pspec(G_OBJECT(item), obj_properties[PROP_OPERATION_STATE]);
    }
}

static void
downloads_item_set_origin_uri(DownloadsItem* item, const gchar* uri)
{
    if(uri)
    {
        if(APP_DOWNLOADS_ITEM(item)->cpp_data.uri != uri)
        {
            item->cpp_data.uri = uri;
            g_object_notify_by_pspec(G_OBJECT(item), obj_properties[PROP_ORIGIN_URI]);
        }
    }
}

static void
downloads_item_set_id(DownloadsItem* item, orion::DownloadId id)
{
    if(item->cpp_data.id != id)
    {
        item->cpp_data.id = id;
        g_object_notify_by_pspec(G_OBJECT(item), obj_properties[PROP_ID]);
    }
}

static void
downloads_item_set_timestamp(DownloadsItem* item, orion::UnixTimeUtc64 timestamp)
{
    if(item->cpp_data.timestamp != timestamp)
    {
        item->cpp_data.timestamp = timestamp;
        g_object_notify_by_pspec(G_OBJECT(item), obj_properties[PROP_TIME_STAMP]);
    }
}

static void
downloads_item_set_total_size(DownloadsItem* item, int64_t total_size)
{
    if(item->cpp_data.total_size != total_size)
    {
        item->cpp_data.total_size = total_size;
        g_object_notify_by_pspec(G_OBJECT(item), obj_properties[PROP_TOTAL_SIZE]);
    }
}

static void
downloads_item_set_current_size(DownloadsItem* item, int64_t current_size)
{
    if(item->cpp_data.current_size != current_size)
    {
        item->cpp_data.current_size = current_size;

        g_object_notify_by_pspec(G_OBJECT(item), obj_properties[PROP_CURRENT_SIZE]);
        //need to run 2 timers from when this is set to notify about current size and download speed
    }

    int64_t current_time = g_get_monotonic_time(); //in useconds
    if(current_time != item->cpp_data.prev_current_size_time)
    {
        item->cpp_data.download_speed = 1000000*((current_size-(item->cpp_data.prev_current_size)))/(current_time-(item->cpp_data.prev_current_size_time));
        //keeping these current time fields in case we decide to decouple download speed updates from current size updates
        item->cpp_data.prev_current_size = current_size;
        item->cpp_data.prev_current_size_time = current_time;
        g_object_notify_by_pspec(G_OBJECT(item), obj_properties[PROP_DOWNLOAD_SPEED]);
    }
}

static void
downloads_item_set_file_icon(DownloadsItem* item, GIcon* icon)
{
    if(item->icon != icon)
    {
        g_clear_object(&item->icon);
        item->icon = g_object_ref(icon);
        g_object_notify_by_pspec(G_OBJECT(item), obj_properties[PROP_FILE_ICON]);
    }
}



void
downloads_item_update_from_dto(DownloadsItem* item, const orion::DownloadDto& dto)
{
    GIcon* icon = g_icon_new_for_string(dto.icon_string.c_str(), NULL);
    defer([icon]{ g_object_unref(icon); });
    downloads_item_set_id(item, dto.id);
    downloads_item_set_downloads_path(item, dto.path.c_str());
    downloads_item_set_name(item, dto.name.c_str());
    downloads_item_set_file_icon(item, icon);
    downloads_item_set_operation_state(item, dto.operation_state);
    downloads_item_set_timestamp(item, dto.timestamp);
    downloads_item_set_current_size(item, dto.current_size);
    downloads_item_set_total_size(item, dto.total_size);
    downloads_item_set_origin_uri(item, dto.origin_uri.c_str());
}


static void
downloads_item_set_property(GObject* object, guint property_id, const GValue* val, GParamSpec* pspec)
{
    DownloadsItem* self = APP_DOWNLOADS_ITEM(object);

    switch(property_id)
    {
        case PROP_ID:
            downloads_item_set_id(self, orion::DownloadId(g_value_get_int64(val)));
            break;
        case PROP_DOWNLOADS_PATH:
            downloads_item_set_downloads_path(self, g_value_get_string(val));
            break;
        case PROP_NAME:
            downloads_item_set_name(self, g_value_get_string(val));
            break;
        case PROP_OPERATION_STATE:
            downloads_item_set_operation_state(self, orion::DownloadOperationState(g_value_get_enum(val)));
            break;
        case PROP_ORIGIN_URI:
            downloads_item_set_origin_uri(self, g_value_get_string(val));
            break;
        case PROP_TIME_STAMP:
            downloads_item_set_timestamp(self, orion::UnixTimeUtc64(g_value_get_int64(val)));
            break;
        case PROP_TOTAL_SIZE:
            downloads_item_set_total_size(self, g_value_get_int64(val));
            break;
        case PROP_CURRENT_SIZE:
            downloads_item_set_current_size(self, g_value_get_int64(val));
            break;
        case PROP_FILE_ICON:
            downloads_item_set_file_icon(self, G_ICON(g_value_get_object(val)));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
downloads_item_get_property(GObject* object, guint property_id, GValue* val, GParamSpec* pspec)
{
    DownloadsItem* self = APP_DOWNLOADS_ITEM(object);

    switch(property_id)
    {
        case PROP_ID:
            g_value_set_int64(val, downloads_item_get_id(self).value);
            break;
        case PROP_FILE_ICON:
            g_value_set_object(val, downloads_item_get_file_icon(self));
            break;
        case PROP_DOWNLOADS_PATH:
            g_value_set_string(val, downloads_item_get_downloads_path(self).c_str());
            break;
        case PROP_NAME:
            g_value_set_string(val, downloads_item_get_file_name(self).c_str());
            break;
        case PROP_ORIGIN_URI:
            g_value_set_string(val, downloads_item_get_origin_uri(self).c_str());
            break;
        case PROP_OPERATION_STATE:
            g_value_set_enum(val, gint(downloads_item_get_operation_state(self)));
            break;
        case PROP_TIME_STAMP:
            g_value_set_int64(val, downloads_item_get_timestamp(self).value);
            break;
        case PROP_TOTAL_SIZE:
            g_value_set_int64(val, downloads_item_get_total_size(self));
            break;
        case PROP_CURRENT_SIZE:
            g_value_set_int64(val, downloads_item_get_current_size(self));
            break;
        case PROP_DOWNLOAD_SPEED:
            g_value_set_int64(val, downloads_item_get_download_speed(self));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
downloads_item_dispose(GObject* object)
{
    //DownloadsItem* item = APP_DOWNLOADS_ITEM(object);
    G_OBJECT_CLASS(downloads_item_parent_class)->finalize(object);
}

static void
downloads_item_finalize(GObject* object)
{
    DownloadsItem* item = APP_DOWNLOADS_ITEM(object);
    std::destroy_at(&item->cpp_data);
    g_clear_object(&item->icon);
    G_OBJECT_CLASS(downloads_item_parent_class)->finalize(object);
}

static void
downloads_item_init(DownloadsItem* item)
{
    std::construct_at(&item->cpp_data);
}

static void
downloads_item_constructed(GObject* object)
{
    //DownloadsItem* item = APP_DOWNLOADS_ITEM(object);
    G_OBJECT_CLASS(downloads_item_parent_class)->constructed(object);
}

static void
downloads_item_class_init(DownloadsItemClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = downloads_item_dispose;
    object_class->finalize = downloads_item_finalize;
    object_class->get_property = downloads_item_get_property;
    object_class->set_property = downloads_item_set_property;
    object_class->constructed = downloads_item_constructed;

    obj_properties[PROP_ID] = g_param_spec_int64("id", NULL, NULL, G_MININT64, G_MAXINT64, 0,
        GParamFlags(G_PARAM_EXPLICIT_NOTIFY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    obj_properties[PROP_DOWNLOADS_PATH] = g_param_spec_string("file-path", NULL, NULL, "",
        GParamFlags(G_PARAM_EXPLICIT_NOTIFY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    obj_properties[PROP_FILE_ICON] = g_param_spec_object("file-icon", NULL, NULL, G_TYPE_ICON,
        GParamFlags(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    obj_properties[PROP_NAME] = g_param_spec_string("file-name", NULL, NULL, "",
        GParamFlags(G_PARAM_EXPLICIT_NOTIFY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    obj_properties[PROP_OPERATION_STATE] = g_param_spec_enum("operation-state", NULL, NULL, 
        download_operation_state_get_type(), gint(orion::DownloadOperationState::downloading),
        GParamFlags(G_PARAM_EXPLICIT_NOTIFY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    obj_properties[PROP_ORIGIN_URI] = g_param_spec_string("origin-uri", NULL, NULL, "",
        GParamFlags(G_PARAM_EXPLICIT_NOTIFY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    obj_properties[PROP_TIME_STAMP] = g_param_spec_int64("timestamp", NULL, NULL, G_MININT64, G_MAXINT64, 0,
        GParamFlags(G_PARAM_EXPLICIT_NOTIFY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    obj_properties[PROP_TOTAL_SIZE] = g_param_spec_int64("total-size", NULL, NULL, G_MININT64, G_MAXINT64, 0,
        GParamFlags(G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));
    obj_properties[PROP_CURRENT_SIZE] = g_param_spec_int64("current-size", NULL, NULL, G_MININT64, G_MAXINT64, 0,
        GParamFlags(G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));
    obj_properties[PROP_DOWNLOAD_SPEED] = g_param_spec_int64("download-speed", NULL, NULL, G_MININT64, G_MAXINT64, 0,
        GParamFlags(G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

    g_object_class_install_properties(object_class, N_PROPS, obj_properties);
}