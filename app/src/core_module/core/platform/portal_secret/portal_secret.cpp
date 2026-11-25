#include <gio/gio.h>
#include <stdio.h>
#include <string.h>

#include <core/platform/portal_secret/portal_secret.h>

namespace orion::portal_secret {

/* Callback for the Response signal from RetrieveSecrets */
static void
handle_retrieve_response(
    GDBusConnection *connection, const gchar *sender_name, const gchar *object_path, const gchar *interface_name,
    const gchar *signal_name, GVariant *parameters, gpointer user_data)
{
    guint32 response_code;
    GVariant *response_data;

    /* Response signal format: (u a{sv}) */
    g_variant_get(parameters, "(u@a{sv})", &response_code, &response_data);

    if (response_code == 0) {
        GVariant *secrets_dict = NULL;
        if (g_variant_lookup(response_data, "secrets", "a{sv}", &secrets_dict)) {
            GVariantIter iter;
            const gchar *key;
            GVariant *value;

            g_variant_iter_init(&iter, secrets_dict);
            while (g_variant_iter_next(&iter, "{&sv}", &key, &value)) {
                gsize len;
                const guint8 *bytes = (const guint8 *)g_variant_get_fixed_array(value, &len, sizeof(guint8));
                printf("Secret '%s' = '", key);
                for (gsize i = 0; i < len; i++)
                    putchar(bytes[i]);
                printf("'\n");
                g_variant_unref(value);
            }
        } else {
            printf("No secrets returned.\n");
        }
    } else {
        printf("Failed to retrieve secret. Response code: %u\n", response_code);
    }

    g_variant_unref(response_data);
}

/* Callback for the StoreSecret response */
static void
handle_secret_response(
    GDBusConnection *connection, const gchar *sender_name, const gchar *object_path, const gchar *interface_name,
    const gchar *signal_name, GVariant *parameters, gpointer user_data)
{
    guint32 response_code;
    GVariant *response_data;

    /* Response signal: (u a{sv}) */
    g_variant_get(parameters, "(u@a{sv})", &response_code, &response_data);

    if (response_code == 0) {
        const gchar *handle_token = NULL;
        if (g_variant_lookup(response_data, "handle_token", "s", &handle_token)) {
            printf("Secret stored successfully! Handle token: %s\n", handle_token);
        } else {
            printf("Secret stored successfully, but no handle token returned.\n");
        }
    } else {
        printf("Failed to store secret. Response code: %u\n", response_code);
    }

    g_variant_unref(response_data);
}

static int
retrieve()
{
    GError *error = NULL;

    /* Connect to session bus */
    GDBusConnection *connection =
        g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);
    if (!connection) {
        fprintf(stderr, "Failed to connect to session bus: %s\n", error->message);
        g_error_free(error);
        return 1;
    }

    /* Subscribe to Response signals from the portal */
    guint sub_id = g_dbus_connection_signal_subscribe(
        connection,
        "org.freedesktop.portal.Desktop",      // sender
        "org.freedesktop.portal.Request",      // interface
        "Response",                            // signal
        NULL,                                  // object path (all)
        NULL,                                  // arg0 (any)
        G_DBUS_SIGNAL_FLAGS_NONE,
        handle_retrieve_response,
        NULL,                                  // user_data filled later
        NULL
    );

    /* Build the (a{sv}) dictionary with handle_tokens */
    GVariantBuilder tuple_builder;
    g_variant_builder_init(&tuple_builder, G_VARIANT_TYPE_TUPLE);

    GVariantBuilder dict_builder;
    g_variant_builder_init(&dict_builder, G_VARIANT_TYPE_VARDICT);

    /* Replace with your handle token(s) from StoreSecret */
    const gchar *tokens[] = { "<your-handle-token-here>", NULL };
    g_variant_builder_add(&dict_builder, "{sv}", "handle_tokens",
                      g_variant_new_strv(tokens, -1));

    GVariant *dict = g_variant_builder_end(&dict_builder);
    g_variant_builder_add_value(&tuple_builder, dict);

    GVariant *params = g_variant_builder_end(&tuple_builder);

    /* Call RetrieveSecrets asynchronously */
    g_dbus_connection_call(
        connection,
        "org.freedesktop.portal.Desktop",
        "/org/freedesktop/portal/desktop",
        "org.freedesktop.portal.Secret",
        "RetrieveSecrets",
        params,
        NULL,
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        NULL,
        NULL,
        NULL
    );

    /* Re-subscribe signal with loop as user_data so callback can quit */
    g_dbus_connection_signal_subscribe(
        connection,
        "org.freedesktop.portal.Desktop",
        "org.freedesktop.portal.Request",
        "Response",
        NULL,
        NULL,
        G_DBUS_SIGNAL_FLAGS_NONE,
        handle_retrieve_response,
        NULL,
        NULL
    );

    /* Cleanup */
    // g_dbus_connection_signal_unsubscribe(connection, sub_id);
    // g_object_unref(connection);
    (void)sub_id;

    return 0;
}

int
test_portal_secret()
{
    GError *error = NULL;

    /* Connect to session bus */
    GDBusConnection *connection = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);
    if (!connection) {
        fprintf(stderr, "Failed to connect to session bus: %s\n", error->message);
        g_error_free(error);
        return 1;
    }

    /* ----------------------
     * Step 1: Open a secret session
     * ---------------------- */
    GDBusProxy *secret_proxy = g_dbus_proxy_new_sync(
        connection,
        G_DBUS_PROXY_FLAGS_NONE,
        NULL,
        "org.freedesktop.portal.Desktop",
        "/org/freedesktop/portal/desktop",
        "org.freedesktop.portal.Secret",
        NULL,
        &error
    );

    if (!secret_proxy) {
        fprintf(stderr, "Failed to create Secret proxy: %s\n", error->message);
        g_error_free(error);
        g_object_unref(connection);
        return 1;
    }

    /* Prepare options dictionary for OpenSession (a{sv}) */
    GVariantBuilder tuple_builder;
    g_variant_builder_init(&tuple_builder, G_VARIANT_TYPE_TUPLE);

    GVariantBuilder dict_builder;
    g_variant_builder_init(&dict_builder, G_VARIANT_TYPE_VARDICT);

    g_variant_builder_add(&dict_builder, "{sv}", "app_id",
                          g_variant_new_string("com.example.MyApp"));

    GVariant *options = g_variant_builder_end(&dict_builder);
    g_variant_builder_add_value(&tuple_builder, options);

    GVariant *params = g_variant_builder_end(&tuple_builder);

    /* Call OpenSession synchronously to get the session object path */
    GVariant *reply = g_dbus_proxy_call_sync(
        secret_proxy,
        "OpenSession",
        params,
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        NULL,
        &error
    );

    if (!reply) {
        fprintf(stderr, "OpenSession failed: %s\n", error->message);
        g_error_free(error);
        g_object_unref(secret_proxy);
        g_object_unref(connection);
        return 1;
    }

    /* OpenSession returns (o s) -> session path and session token */
    const gchar *session_path;
    const gchar *session_token;
    g_variant_get(reply, "(&o&s)", &session_path, &session_token);
    g_variant_unref(reply);

    printf("Session path: %s, token: %s\n", session_path, session_token);

    /* ----------------------
     * Step 2: Call CreateSecret on the session
     * ---------------------- */
    GDBusProxy *session_proxy = g_dbus_proxy_new_sync(
        connection,
        G_DBUS_PROXY_FLAGS_NONE,
        NULL,
        "org.freedesktop.portal.Desktop",
        session_path,
        "org.freedesktop.portal.Secret.Session",
        NULL,
        &error
    );

    if (!session_proxy) {
        fprintf(stderr, "Failed to create Session proxy: %s\n", error->message);
        g_error_free(error);
        g_object_unref(secret_proxy);
        g_object_unref(connection);
        return 1;
    }

    /* Build (a{sv}) dictionary for CreateSecret */
    g_variant_builder_init(&tuple_builder, G_VARIANT_TYPE_TUPLE);
    g_variant_builder_init(&dict_builder, G_VARIANT_TYPE_VARDICT);

    g_variant_builder_add(&dict_builder, "{sv}", "label",
                          g_variant_new_string("My Secret"));

    const guint8 secret_data[] = { 's','e','c','r','e','t' };
    GVariant *secret_variant = g_variant_new_fixed_array(
        G_VARIANT_TYPE_BYTE, secret_data,
        sizeof(secret_data), sizeof(guint8));
    g_variant_builder_add(&dict_builder, "{sv}", "secret", secret_variant);

    GVariant *secret_options = g_variant_builder_end(&dict_builder);
    g_variant_builder_add_value(&tuple_builder, secret_options);

    GVariant *secret_params = g_variant_builder_end(&tuple_builder);

    /* Call CreateSecret asynchronously */
    g_dbus_proxy_call(
        session_proxy,
        "CreateSecret",
        secret_params,
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        NULL,
        NULL,   /* no immediate callback */
        NULL
    );

    /* ----------------------
     * Step 3: Listen for Response signals on the session object
     * ---------------------- */

    g_dbus_connection_signal_subscribe(
        connection,
        "org.freedesktop.portal.Desktop",
        "org.freedesktop.portal.Request",
        "Response",
        session_path,   /* very important: listen on the session path */
        NULL,
        G_DBUS_SIGNAL_FLAGS_NONE,
        handle_secret_response,
        NULL,
        NULL
    );

    /* Cleanup */
    g_object_unref(session_proxy);
    g_object_unref(secret_proxy);
    g_object_unref(connection);

    /* Cleanup */
    // g_dbus_connection_signal_unsubscribe(connection, sub_id);
    // g_object_unref(connection);
    //(void)sub_id;

    //retrieve();
    (void)retrieve;

    // TODO:
    return 0;
}

} // namespace orion::portal_secret