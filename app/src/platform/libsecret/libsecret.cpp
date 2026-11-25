#define ENABLE_VERBOSE_LOGS 0

#include <libsecret/secret.h>

#include "platform/libsecret/libsecret.h"
#include <common/src/log.h>
#include <common/src/defer.h>
#include <common/src/error.h>
#include <common/src/dispatch.h>
#include <common/src/string_util.h>

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
#   include <jsc/jsc.h>
#   include "util/jsc_util.h"
#endif

#define ENABLE_LIBSECRET_TEST !NDEBUG && 1
#define APP_SCHEMA_VERSION 1 // not needed at the moment, can be useful in the future

// Note: this returns pointer to static string, do not try to free it.
// This is useful just internally from Orion app, schema name is never used as predicate in lookup.
// We use it to distinguish test and production secret items.
static const char*
static_app_schema_name(const char* app_id)
{
    static std::string schema_name = std::string(app_id) + ".PasswordItems";
    return schema_name.c_str();
}

// Note: this returns pointer to static string, do not try to free it.
static const char*
static_app_test_schema_name(const char* app_id)
{
    static std::string schema_name = std::string(app_id) + ".PasswordItems.Test";
    return schema_name.c_str();
}

namespace orion::libsecret {

static const SecretSchema*
get_schema(const char* app_id)
{
    static const SecretSchema schema = {
        static_app_schema_name(app_id),
        SECRET_SCHEMA_NONE,
        {
            { "app_id", SECRET_SCHEMA_ATTRIBUTE_STRING },
            { "schema_version", SECRET_SCHEMA_ATTRIBUTE_INTEGER },
            { "is_test", SECRET_SCHEMA_ATTRIBUTE_BOOLEAN },
            { NULL, SECRET_SCHEMA_ATTRIBUTE_STRING },
        },
        0, NULL, NULL, NULL, NULL, NULL, NULL, NULL
    };
    return &schema;
}

#if ENABLE_LIBSECRET_TEST && defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
static const SecretSchema*
get_test_schema(const char* app_id)
{
    static const SecretSchema schema = {
        static_app_test_schema_name(app_id), // different name of test schema
        SECRET_SCHEMA_NONE,
        {
            { "app_id", SECRET_SCHEMA_ATTRIBUTE_STRING },
            { "schema_version", SECRET_SCHEMA_ATTRIBUTE_INTEGER },
            { "is_test", SECRET_SCHEMA_ATTRIBUTE_BOOLEAN },
            { NULL, SECRET_SCHEMA_ATTRIBUTE_STRING },
        },
        0, NULL, NULL, NULL, NULL, NULL, NULL, NULL
    };
    return &schema;
}
#endif // ENABLE_LIBSECRET_TEST && defined(ENABLE_WEBKIT) && ENABLE_WEBKIT

static Error
is_test_schema(const char* app_id, const SecretSchema* schema, gboolean* out_is_test)
{
    RAISE_CHECK(out_is_test);

    if (strcmp(schema->name, static_app_schema_name(app_id)) == 0) {
        *out_is_test = FALSE;
    } else if (strcmp(schema->name, static_app_test_schema_name(app_id)) == 0) {
        *out_is_test = TRUE;
    } else {
        RAISE(ErrorCode::argument, "unknown secret schema name: %s", schema->name);
    }

    SUCCEED();
}

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
static Error
store_secret_string_impl(const char* app_id, const SecretSchema* schema, const char* str)
{
    GError* error = nullptr;
    defer([&error]{
        if (error) {
            g_clear_error(&error);
        }
    });

    gboolean is_test;
    VERIFY(is_test_schema(app_id, schema, &is_test));

#if NDEBUG
    log_info("store_secret_string_impl: app_id=%s schema_version=%d is_test=%d",
        APPLICATION_ID,
        APP_SCHEMA_VERSION,
        is_test
    );
#endif

    // Just a display label, not part of lookup key. This is displayed for example
    // in Seahorse password manager.
    std::string label = "Orion Passwords";
    if (is_test) {
        label += " (Test)";
    }

    if (!secret_password_store_sync(
        schema,
        SECRET_COLLECTION_DEFAULT,
        label.c_str(), // label (not part of unique key)
        str,
        nullptr, // cancellable
        &error,
        "app_id", app_id,
        "schema_version", APP_SCHEMA_VERSION,
        "is_test", is_test,
        NULL // end of attributes
    )) {
        RAISE(ErrorCode::io, "failed to store password field: %s", (error ? error->message : "unknown error"));
    }

    SUCCEED();
}
#endif // defined(ENABLE_WEBKIT) && ENABLE_WEBKIT

static Error
get_password_items_impl(const char* app_id, const SecretSchema* schema, std::vector<PasswordItemDto>* out_items)
{
    RAISE_CHECK(out_items);

    GError* error = nullptr;
    defer([&error]{
        if (error) {
            g_clear_error(&error);
        }
    });

    gboolean is_test;
    VERIFY(is_test_schema(app_id, schema, &is_test));

#if NDEBUG
    log_info("get_password_items_impl: app_id=%s schema_version=%d is_test=%d",
        app_id,
        APP_SCHEMA_VERSION,
        is_test
    );
#endif

    gchar* str = secret_password_lookup_sync(
        schema,
        nullptr, // cancellable
        &error,
        "app_id", app_id,
        "schema_version", APP_SCHEMA_VERSION,
        "is_test", is_test,
        NULL
    );

    if (error) {
        RAISE(ErrorCode::io, "failed to lookup password items: %s", error->message);
    }

    defer([str] { secret_password_free(str); });

    log_verbose("get_password_items_impl: dump:\n%s", str);

    if (!str || strlen(str) == 0) {
        // no stored items yet
        SUCCEED();
    }

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    // Parse JSON string
    JSCContext* ctx = jsc_context_new();
    defer([ctx]{ g_object_unref(ctx); });

    JSCValue* json_str = jsc_value_new_string(ctx, str);
    JSCValue* fn = jsc_context_evaluate(ctx, "JSON.parse", -1);
    JSCValue* array = jsc_value_function_call(fn, JSC_TYPE_VALUE, json_str, G_TYPE_NONE);

    if (!jsc_value_is_array(array)) {
        RAISE(ErrorCode::io, "stored password items is not a valid JSON array");
    }

    gint32 length = jsc_value_object_get_property(array, "length")
        ? jsc_value_to_int32(jsc_value_object_get_property(array, "length"))
        : 0;

    for (gint32 i = 0; i < length; ++i) {
        JSCValue* obj = jsc_value_object_get_property_at_index(array, i);
        if (!jsc_value_is_object(obj)) {
            continue;
        }

        PasswordItemId id{};
        VERIFY(orion::str::try_parse_int64(
            orion::jsc::value_to_string(jsc_value_object_get_property(obj, "id")), &id.value));

        ProfileId profile_id{};
        VERIFY(orion::str::try_parse_int64(
            orion::jsc::value_to_string(jsc_value_object_get_property(obj, "profile_id")), &profile_id.value));

        PasswordItemDto item{
            .id = id,
            .profile_id = profile_id,
            .url = orion::jsc::value_to_string(jsc_value_object_get_property(obj, "url")),
            .username = orion::jsc::value_to_string(jsc_value_object_get_property(obj, "username")),
            .password = orion::jsc::value_to_string(jsc_value_object_get_property(obj, "password")),
            .verification_key = orion::jsc::value_to_string(jsc_value_object_get_property(obj, "verification_key")),
            .verification_code = orion::jsc::value_to_string(jsc_value_object_get_property(obj, "verification_code")),
        };

        out_items->push_back(item);
    }
#endif // defined(ENABLE_WEBKIT) && ENABLE_WEBKIT

    SUCCEED();
}

static Error
upsert_password_items_impl(const char* app_id, const SecretSchema* schema, const std::vector<PasswordItemDto>& items)
{
    if (is_main_thread()) {
        // because of jsc_context_new call below - it would break things
        RAISE(ErrorCode::threading, "upsert_password_items_impl must not be called on main thread");
    }

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    JSCContext* ctx = jsc_context_new();
    defer([ctx]{ g_object_unref(ctx); });

    JSCValue* array = jsc_value_new_array(ctx, G_TYPE_NONE);
    
    for (size_t i = 0; i < items.size(); ++i) {
        const PasswordItemDto& item = items[i];

        JSCValue* obj = jsc_value_new_object(ctx, NULL, NULL);

        std::string id = std::to_string(item.id.value); // avoid JSON numbers to keep precision
        std::string profile_id = std::to_string(item.profile_id.value);

        jsc_value_object_set_property(obj, "id", jsc_value_new_string(ctx, id.c_str()));
        jsc_value_object_set_property(obj, "profile_id", jsc_value_new_string(ctx, profile_id.c_str()));
        jsc_value_object_set_property(obj, "url", jsc_value_new_string(ctx, item.url.c_str()));
        jsc_value_object_set_property(obj, "username", jsc_value_new_string(ctx, item.username.c_str()));
        jsc_value_object_set_property(obj, "password", jsc_value_new_string(ctx, item.password.c_str()));
        jsc_value_object_set_property(obj, "verification_key", jsc_value_new_string(ctx, item.verification_key.c_str()));
        jsc_value_object_set_property(obj, "verification_code", jsc_value_new_string(ctx, item.verification_code.c_str()));

        jsc_value_object_set_property_at_index(array, i, obj);
    }
    JSCValue* fn = jsc_context_evaluate(ctx, "JSON.stringify", -1);
    JSCValue* result = jsc_value_function_call(fn, JSC_TYPE_VALUE, array, G_TYPE_NONE);

    std::string str = orion::jsc::value_to_string(result);

    VERIFY(store_secret_string_impl(app_id, schema, str.c_str()));
#endif // defined(ENABLE_WEBKIT) && ENABLE_WEBKIT

    SUCCEED();
}

#if ENABLE_LIBSECRET_TEST && defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
static Error
test_libsecret_internal(const char* app_id)
{
    {
        std::vector<PasswordItemDto> items = {
            PasswordItemDto{
                .id = PasswordItemId(1),
                .profile_id = ProfileId(1),
                .url = "https://example.com",
                .username = "user1",
                .password = "pass1",
                .verification_key = "verif_key_1",
                .verification_code = "verif_code_1",
            },
            PasswordItemDto{
                .id = PasswordItemId(2),
                .profile_id = ProfileId(1),
                .url = "https://atlas.org",
                .username = "user2",
                .password = "pass2",
                .verification_key = "verif_key_2",
                .verification_code = "verif_code_2",
            },
        };

        VERIFY(upsert_password_items_impl(app_id, get_test_schema(app_id), items));
    }
    {
        std::vector<PasswordItemDto> out_items;
        get_password_items_impl(app_id, get_test_schema(app_id), &out_items);

        RAISE_CHECK(out_items.size() == 2);

        RAISE_CHECK(out_items[0].id == PasswordItemId(1));
        RAISE_CHECK(out_items[0].profile_id == ProfileId(1));
        RAISE_CHECK(out_items[0].url == "https://example.com");
        RAISE_CHECK(out_items[0].username == "user1");
        RAISE_CHECK(out_items[0].password == "pass1");
        RAISE_CHECK(out_items[0].verification_key == "verif_key_1");
        RAISE_CHECK(out_items[0].verification_code == "verif_code_1");

        RAISE_CHECK(out_items[1].id == PasswordItemId(2));
        RAISE_CHECK(out_items[1].profile_id == ProfileId(1));
        RAISE_CHECK(out_items[1].url == "https://atlas.org");
        RAISE_CHECK(out_items[1].username == "user2");
        RAISE_CHECK(out_items[1].password == "pass2");
        RAISE_CHECK(out_items[1].verification_key == "verif_key_2");
        RAISE_CHECK(out_items[1].verification_code == "verif_code_2");
    }

    SUCCEED();
}
#endif // ENABLE_LIBSECRET_TEST && defined(ENABLE_WEBKIT) && ENABLE_WEBKIT

Error
upsert_password_items(const char* app_id, const std::vector<PasswordItemDto>& items)
{
    return upsert_password_items_impl(app_id, get_schema(app_id), items);
}

Error
get_password_items(const char* app_id, std::vector<PasswordItemDto>* out_items)
{
    return get_password_items_impl(app_id, get_schema(app_id), out_items);
}

void
log_password_items(const char* app_id)
{
    std::string appid = app_id;
    dispatch(on_disk_thread(), [appid]{
        std::vector<PasswordItemDto> items;
        VERIFY_LOG(get_password_items(appid.c_str(), &items));
        for (const auto& item : items) {
            log_verbose(
                "PasswordItemDto{id=%lld, profile_id=%lld, url='%s', username='%s', password='%s'"
                ", verification_key='%s', verification_code='%s'}",
                item.id.value,
                item.profile_id.value,
                item.url.c_str(),
                item.username.c_str(),
                item.password.c_str(),
                item.verification_key.c_str(),
                item.verification_code.c_str()
            );
        }
    });
}

void
test_libsecret(const char* app_id)
{
    std::string appid = app_id;
    // libsecret operations work only with webkit at the moment
#if ENABLE_LIBSECRET_TEST && defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    dispatch(on_disk_thread(), [appid]{
        Error err = test_libsecret_internal(appid.c_str());
        if (err) {
            log_error("libsecret test failed: %s", err.to_string().c_str());
        } else {
            log_verbose("libsecret test succeeded");
        }
    });
#endif // ENABLE_LIBSECRET_TEST && defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
}

} // namespace orion::libsecret
