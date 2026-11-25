#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include <common/src/uri.h>

#include "data_item/passwords_columnview_item.h"

struct _PasswordsColumnviewItem
{
    GObject parent;

    orion::PasswordItemId id;
    GString* url;
    GString* username;
    GString* password;
    GString* verification_key;
    GString* verification_code;
    GtkLabel* password_label;
    GtkWidget* verification_code_widget;
};

G_DEFINE_TYPE(PasswordsColumnviewItem, passwords_columnview_item, G_TYPE_OBJECT)

enum {
    PROP_0,
    PROP_ID,
    PROP_URL,
    PROP_USERNAME,
    PROP_PASSWORD,
    PROP_VERIFICATION_KEY,
    PROP_VERIFICATION_CODE,
    N_PROPS,
};

static GParamSpec* obj_properties[N_PROPS] = {};

orion::PasswordItemId
passwords_columnview_item_get_id(PasswordsColumnviewItem* item)
{
    return PASSWORDS_COLUMNVIEW_ITEM(item)->id;
}

GString*
passwords_columnview_item_get_url(PasswordsColumnviewItem* item)
{
    return PASSWORDS_COLUMNVIEW_ITEM(item)->url;
}

GString*
passwords_columnview_item_get_username(PasswordsColumnviewItem* item)
{
    return PASSWORDS_COLUMNVIEW_ITEM(item)->username;
}

GString*
passwords_columnview_item_get_password(PasswordsColumnviewItem* item)
{
    return PASSWORDS_COLUMNVIEW_ITEM(item)->password;
}

GString*
passwords_columnview_item_get_verification_key(PasswordsColumnviewItem* item)
{
    return PASSWORDS_COLUMNVIEW_ITEM(item)->verification_key;
}

GString*
passwords_columnview_item_get_verification_code(PasswordsColumnviewItem* item)
{
    return PASSWORDS_COLUMNVIEW_ITEM(item)->verification_code;
}

double
passwords_columnview_item_get_seq_num(PasswordsColumnviewItem* item)
{
    return static_cast<double>(PASSWORDS_COLUMNVIEW_ITEM(item)->id.value);
}

GtkLabel*
passwords_columnview_item_get_password_label(PasswordsColumnviewItem* item)
{
    return PASSWORDS_COLUMNVIEW_ITEM(item)->password_label;
}

GtkWidget*
passwords_columnview_item_get_verification_code_widget(PasswordsColumnviewItem* item)
{
    return PASSWORDS_COLUMNVIEW_ITEM(item)->verification_code_widget;
}

void
passwords_columnview_item_set_url(PasswordsColumnviewItem* item, const char* url)
{
    g_string_assign(item->url, url);
    g_object_notify(G_OBJECT(item), "url"); // explicit notify because G_PARAM_EXPLICIT_NOTIFY
}

void
passwords_columnview_item_set_username(PasswordsColumnviewItem* item, const char* username)
{
    g_string_assign(item->username, username);
    g_object_notify(G_OBJECT(item), "username"); // explicit notify because G_PARAM_EXPLICIT_NOTIFY
}

void
passwords_columnview_item_set_password(PasswordsColumnviewItem* item, const char* password)
{
    g_string_assign(item->password, password);
    g_object_notify(G_OBJECT(item), "password"); // explicit notify because G_PARAM_EXPLICIT_NOTIFY
}

void
passwords_columnview_item_set_verification_key(PasswordsColumnviewItem* item, const char* verification_key)
{
    g_string_assign(item->verification_key, verification_key);
    g_object_notify(G_OBJECT(item), "verification-key"); // explicit notify because G_PARAM_EXPLICIT_NOTIFY
}

void
passwords_columnview_item_set_verification_code(PasswordsColumnviewItem* item, const char* verification_code)
{
    g_string_assign(item->verification_code, verification_code);
    g_object_notify(G_OBJECT(item), "verification-code"); // explicit notify because G_PARAM_EXPLICIT_NOTIFY
}

void
passwords_columnview_item_set_password_label(PasswordsColumnviewItem* item, GtkLabel* label)
{
    PASSWORDS_COLUMNVIEW_ITEM(item)->password_label = label;
}

void
passwords_columnview_item_set_verification_code_widget(PasswordsColumnviewItem* item, GtkWidget* widget)
{
    PASSWORDS_COLUMNVIEW_ITEM(item)->verification_code_widget = widget;
}

PasswordsColumnviewItem*
passwords_columnview_item_new_from_dto(const orion::PasswordItemDto& dto)
{
	PasswordsColumnviewItem* item = static_cast<PasswordsColumnviewItem*>(
        g_object_new(PASSWORDS_COLUMNVIEW_ITEM_TYPE, NULL)
    );

    item->id = dto.id;

    item->url = g_string_new(dto.url.c_str());
    item->username = g_string_new(dto.username.c_str());
    item->password = g_string_new(dto.password.c_str());
    item->verification_key = g_string_new(dto.verification_key.c_str());
    item->verification_code = g_string_new(dto.verification_code.c_str());

    return item;
}

static void
passwords_columnview_item_set_property(GObject* object, guint property_id, const GValue* value, GParamSpec* pspec)
{
    PasswordsColumnviewItem* item = PASSWORDS_COLUMNVIEW_ITEM(object);

    switch (property_id) {
    case PROP_ID:
        item->id = orion::PasswordItemId(g_value_get_int64(value));
        break;
    case PROP_URL:
        passwords_columnview_item_set_url(item, g_value_get_string(value));
        break;
    case PROP_USERNAME:
        passwords_columnview_item_set_username(item, g_value_get_string(value));
        break;
    case PROP_PASSWORD:
        passwords_columnview_item_set_password(item, g_value_get_string(value));
        break;
    case PROP_VERIFICATION_KEY:
        passwords_columnview_item_set_verification_key(item, g_value_get_string(value));
        break;
    case PROP_VERIFICATION_CODE:
        passwords_columnview_item_set_verification_code(item, g_value_get_string(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

static void
passwords_columnview_item_get_property(GObject* object, guint property_id, GValue* value, GParamSpec* pspec)
{
    PasswordsColumnviewItem* item = PASSWORDS_COLUMNVIEW_ITEM(object);

    switch (property_id) {
    case PROP_ID:
        g_value_set_int64(value, int64_t(passwords_columnview_item_get_id(item).value));
        break;
    case PROP_URL:
        g_value_set_string(value, passwords_columnview_item_get_url(item)->str);
        break;
    case PROP_USERNAME:
        g_value_set_string(value, passwords_columnview_item_get_username(item)->str);
        break;
    case PROP_PASSWORD:
        g_value_set_string(value, passwords_columnview_item_get_password(item)->str);
        break;
    case PROP_VERIFICATION_KEY:
        g_value_set_string(value, passwords_columnview_item_get_verification_key(item)->str);
        break;
    case PROP_VERIFICATION_CODE:
        g_value_set_string(value, passwords_columnview_item_get_verification_code(item)->str);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

static void
passwords_columnview_item_finalize(GObject* object)
{
    PasswordsColumnviewItem* item = PASSWORDS_COLUMNVIEW_ITEM(object);
    
    std::destroy_at(&item->id);

    g_string_free(item->url, TRUE);
    g_string_free(item->username, TRUE);
    g_string_free(item->password, TRUE);
    g_string_free(item->verification_key, TRUE);
    g_string_free(item->verification_code, TRUE);

    G_OBJECT_CLASS(passwords_columnview_item_parent_class)->finalize(object);
}

void
passwords_columnview_item_init(PasswordsColumnviewItem* item)
{
    std::construct_at(&item->id);
}

static void
passwords_columnview_item_class_init(PasswordsColumnviewItemClass* klass)
{
    G_OBJECT_CLASS(klass)->finalize = passwords_columnview_item_finalize;
    G_OBJECT_CLASS(klass)->set_property = passwords_columnview_item_set_property;
    G_OBJECT_CLASS(klass)->get_property = passwords_columnview_item_get_property;

    obj_properties[PROP_ID] = g_param_spec_int64(
        "id", NULL, NULL, INT64_MIN, INT64_MAX, 0, GParamFlags(G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY));
    obj_properties[PROP_URL] = g_param_spec_string(
        "url", NULL, NULL, "", GParamFlags(G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY));
    obj_properties[PROP_USERNAME] = g_param_spec_string(
        "username", NULL, NULL, "", GParamFlags(G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY));
    obj_properties[PROP_PASSWORD] = g_param_spec_string(
        "password", NULL, NULL, "", GParamFlags(G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY));
    obj_properties[PROP_VERIFICATION_KEY] = g_param_spec_string(
        "verification-key", NULL, NULL, "", GParamFlags(G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY));
    obj_properties[PROP_VERIFICATION_CODE] = g_param_spec_string(
        "verification-code", NULL, NULL, "", GParamFlags(G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY));

    g_object_class_install_properties(G_OBJECT_CLASS(klass), N_PROPS, obj_properties);
}
