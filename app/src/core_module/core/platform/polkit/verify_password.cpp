#include "verify_password.h"

#include <polkit/polkit.h>
#include <gio/gio.h>

namespace orion::polkit {

static void
check_authorization_callback(GObject* source_object, GAsyncResult* res, gpointer user_data)
{
    PolkitAuthority* authority = POLKIT_AUTHORITY(source_object);
    GError *error = NULL;

    PolkitAuthorizationResult *result =
        polkit_authority_check_authorization_finish(authority, res, &error);

    if (error) {
        g_printerr("Authorization check failed: %s\n", error->message);
        g_error_free(error);
        return;
    }

    if (polkit_authorization_result_get_is_authorized(result)) {
        g_print("Authorized!\n");
    } else if (polkit_authorization_result_get_is_challenge(result)) {
        g_print("Authentication required but canceled or failed.\n");
    } else {
        g_print("Not authorized.\n");
    }

    g_object_unref(result);
}

bool
verify_password()
{
    PolkitAuthority *authority = polkit_authority_get_sync(NULL, NULL);

    // This is your Polkit action name:
    const char *action_id = "org.example.myaction";

    pid_t pid = getpid();
    uid_t uid = getuid();

    // Subject: currently logged-in user
    PolkitSubject *subject = polkit_unix_process_new_for_owner(
        pid,
        0, // start_time (do not care)
        uid
    );

    polkit_authority_check_authorization(
        authority,
        subject,
        action_id,
        NULL,                                 // Details dictionary
        POLKIT_CHECK_AUTHORIZATION_FLAGS_ALLOW_USER_INTERACTION,
        NULL,                                 // GCancellable
        check_authorization_callback,
        NULL                                  // User data
    );

    // Run main loop so async call works
    // GMainLoop *loop = g_main_loop_new(NULL, FALSE);
    // g_main_loop_run(loop);

    return 0;
}

} // namespace orion::polkit