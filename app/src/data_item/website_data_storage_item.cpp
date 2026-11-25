#include <string>

#include "website_data_storage_item.h"

#include <common/src/defer.h>

#include "app.h"

namespace {
    struct CppWrapper {
        
    };
}

struct _WebsiteDataStorageItem {
    GObject parent;

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    WebKitWebsiteData* data;
#endif

    CppWrapper wrapper;
};

enum {
    PROP_0,
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    PROP_DATA,
    PROP_TYPES,
#endif
    PROP_NAME,
    PROP_TYPES_STRING,
    N_PROPS,
};

static GParamSpec* obj_props[N_PROPS] = {0, };

G_DEFINE_TYPE(WebsiteDataStorageItem, website_data_storage_item, G_TYPE_OBJECT)

static void
website_data_storage_item_init(WebsiteDataStorageItem* self)
{
    std::construct_at(&self->wrapper);
}

std::string
website_data_storage_item_get_name(WebsiteDataStorageItem* self)
{
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    if(self->data)
    {
        const char* name = webkit_website_data_get_name(self->data);
        return name ? name : "";
    }
    else
    {
        return "";
    }
#else
    return "";
#endif
}

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
WebKitWebsiteData*
website_data_storage_item_get_data(WebsiteDataStorageItem* self)
{
    return self->data;
}

static void
website_data_storage_item_set_data(WebsiteDataStorageItem* self, WebKitWebsiteData* data)
{
    if(self->data != data)
    {
        g_clear_pointer(&self->data, webkit_website_data_unref);
        self->data = webkit_website_data_ref(data);
        g_object_notify_by_pspec(G_OBJECT(self), obj_props[PROP_DATA]);
        g_object_notify_by_pspec(G_OBJECT(self), obj_props[PROP_TYPES]);
        g_object_notify_by_pspec(G_OBJECT(self), obj_props[PROP_NAME]);
        g_object_notify_by_pspec(G_OBJECT(self), obj_props[PROP_TYPES_STRING]);
    }
}

WebKitWebsiteDataTypes
website_data_storage_item_get_types(WebsiteDataStorageItem* self)
{
    if(self->data)
    {
        return webkit_website_data_get_types(self->data);
    }
    else
    {
        return WebKitWebsiteDataTypes(0);
    }
}
#endif

std::string
website_data_storage_item_get_types_string(WebsiteDataStorageItem* self)
{
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    if(!self->data) return "";

    size_t bit_count = sizeof(WebKitWebsiteDataTypes) * 8;
    WebKitWebsiteDataTypes types = webkit_website_data_get_types(self->data);

    std::string res;

    for(size_t i = 0; i < bit_count; i++)
    {
        WebKitWebsiteDataTypes mask = WebKitWebsiteDataTypes(WebKitWebsiteDataTypes(1u) << i);

        #define append(str) do { if(!res.empty()) res.append(", "); res.append(str); } while(0)

        switch(WebKitWebsiteDataTypes(mask & types))
        {
            case WEBKIT_WEBSITE_DATA_MEMORY_CACHE:
                append(_("Memory Cache"));
                break;
            case WEBKIT_WEBSITE_DATA_DISK_CACHE:
                append(_("Disk Cache"));
                break;
            case WEBKIT_WEBSITE_DATA_OFFLINE_APPLICATION_CACHE:
                append(_("Offline App Cache"));
                break;
            case WEBKIT_WEBSITE_DATA_SESSION_STORAGE:
                append(_("Session Storage"));
                break;
            case WEBKIT_WEBSITE_DATA_LOCAL_STORAGE:
                append(_("Local Storage"));
                break;
            case WEBKIT_WEBSITE_DATA_INDEXEDDB_DATABASES:
                append(_("IndexedDB"));
                break;
            case WEBKIT_WEBSITE_DATA_COOKIES:
                append(_("Cookies"));
                break;
            case WEBKIT_WEBSITE_DATA_DEVICE_ID_HASH_SALT:
                append(_("Device ID Hash Salt"));
                break;
            case WEBKIT_WEBSITE_DATA_HSTS_CACHE:
                append(_("HSTS"));
                break;
            case WEBKIT_WEBSITE_DATA_ITP:
                append(_("ITP"));
                break;
            case WEBKIT_WEBSITE_DATA_SERVICE_WORKER_REGISTRATIONS:
                append(_("Service Workers"));
                break;
            case WEBKIT_WEBSITE_DATA_DOM_CACHE:
                append(_("DOM"));
                break;
            case WEBKIT_WEBSITE_DATA_ALL:
                //will never happen
                break;
        }

        #undef append
    }

    return res;
#else
    return "";
#endif
}

gboolean
website_data_storage_item_filter_func(gpointer item, gpointer user_data)
{
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    WebsiteDataStorageItem* self = APP_WEBSITE_DATA_STORAGE_ITEM(item);

    const char* filter_string = static_cast<const char*>(user_data);

    if(!filter_string) return TRUE;

    if(!self->data) return FALSE;

    {
        char* name = g_utf8_casefold(webkit_website_data_get_name(self->data), -1);
        defer([name]{ g_free(name); });
        if(strstr(name, filter_string)) return TRUE;
    }
    {
        char* types_string = g_utf8_casefold(website_data_storage_item_get_types_string(self).c_str(), -1);
        defer([types_string]{ g_free(types_string); });
        if(strstr(types_string, filter_string)) return TRUE;
    }

#endif
    return FALSE;
}

static void
website_data_storage_item_set_property(GObject* object, guint prop_id, const GValue* value, GParamSpec* pspec)
{
    WebsiteDataStorageItem* self = APP_WEBSITE_DATA_STORAGE_ITEM(object);

    switch(prop_id)
    {
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
        case PROP_DATA:
            website_data_storage_item_set_data(self, static_cast<WebKitWebsiteData*>(g_value_get_boxed(value)));
            break;
#endif
    }
}

static void
website_data_storage_item_get_property(GObject* object, guint prop_id, GValue* value, GParamSpec* pspec)
{
    WebsiteDataStorageItem* self = APP_WEBSITE_DATA_STORAGE_ITEM(object);

    switch(prop_id)
    {
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
        case PROP_TYPES:
            g_value_set_flags(value, website_data_storage_item_get_types(self));
            break;
        case PROP_DATA:
            g_value_set_boxed(value, website_data_storage_item_get_data(self));
            break;
#endif
        case PROP_NAME:
            g_value_set_string(value, website_data_storage_item_get_name(self).c_str());
            break;
        case PROP_TYPES_STRING:
            g_value_set_string(value, website_data_storage_item_get_types_string(self).c_str());
            break;
    }
}


static void
website_data_storage_item_finalize(GObject* object)
{
    WebsiteDataStorageItem* self = APP_WEBSITE_DATA_STORAGE_ITEM(object);
    std::destroy_at(&self->wrapper);

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    g_clear_pointer(&self->data, webkit_website_data_unref);
#endif

    G_OBJECT_CLASS(website_data_storage_item_parent_class)->finalize(object);
}

static void
website_data_storage_item_class_init(WebsiteDataStorageItemClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = website_data_storage_item_finalize;
    object_class->set_property = website_data_storage_item_set_property;
    object_class->get_property = website_data_storage_item_get_property;

    
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    obj_props[PROP_DATA] = g_param_spec_boxed("data", NULL, NULL, WEBKIT_TYPE_WEBSITE_DATA, 
        GParamFlags(G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_CONSTRUCT_ONLY));
    obj_props[PROP_TYPES] = g_param_spec_flags("types", NULL, NULL, WEBKIT_TYPE_WEBSITE_DATA_TYPES, 0,
        GParamFlags(G_PARAM_STATIC_STRINGS | G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY));
#endif
    obj_props[PROP_NAME] = g_param_spec_string("name", NULL, NULL, NULL,
        GParamFlags(G_PARAM_STATIC_STRINGS | G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY));
    obj_props[PROP_TYPES_STRING] = g_param_spec_string("types-string", NULL, NULL, NULL,
        GParamFlags(G_PARAM_STATIC_STRINGS | G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY));
    
    g_object_class_install_properties(object_class, N_PROPS, obj_props);
}