#define ENABLE_VERBOSE_LOGS 0

#include <iostream>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <signal.h>
#include <atomic>
#include <string_view>

#include "app.h"
#include "app_downloader.h"
#include "app_password_storage.h"
#include "webextensions/webview_factory_host.h"
#include "util/webkit_util.h"

#include <core/core_api.h>

#include <common/src/log.h>
#include <common/src/error.h>
#include <common/src/file_system.h>
#include <common/src/defer.h>
#include <common/src/uri.h>

struct IterationData {
    GMainContext* context;
    GApplication* app;
};

static bool
iteration_cb(void* user_data)
{
    //log_verbatim("iteration_cb()");
    auto data = static_cast<IterationData*>(user_data);

    int n = 0;

    // The iteration step must not block, this would break core run loop functionality
    // eg., timer watchers would not work correctly
    while (g_main_context_pending(data->context)) {
        g_main_context_iteration(data->context, false);
        n++;
        if (n > 20) {
            // Safety measure to avoid some latency issues where this loop could steal execution time
            break;
        }
    }

    return orion_app_is_running(ORION_APP(data->app));
}

static orion::Error
init(int argc, char** argv, char** envp, int* exit_status)
{
    bool debug_mode = false;
    for (int i = 1; i < argc; ++i) {
        if (std::string_view(argv[i]) == "--debug") {
            debug_mode = true;
            break;
        }
    }

#ifndef NDEBUG
    // Avoids stderr libGL messages
    // NOTE: this is only here because we have a bad permission setup on our testing machine,
    // which causes access to gpu to fail
    setenv("GSK_RENDERER", "opengl", 0); // also maybe try "cairo" instead
    // setenv("WEBKIT_DISABLE_DMABUF_RENDERER", "1", 0);
    // setenv("WEBKIT_DISABLE_COMPOSITING_MODE", "1", 0);
    //setenv("GDK_BACKEND", "x11", 0);
    setenv("LIBGL_ALWAYS_SOFTWARE", "1", 0);
    setenv("GALLIUM_DRIVER", "llvmpipe", 0);
    setenv("LIBGL_DEBUG", "quiet", 0);
    setenv("MESA_DEBUG", "0", 0);
#endif

	orion::log_info("orion-linux is starting");
    orion::log_info("App version: %s (WebKit %s)", APPLICATION_VERSION, orion::webkit::get_webkit_version().c_str());
    orion::log_info("App is beta: %s", APPLICATION_IS_BETA ? "true" : "false");
    if (debug_mode) {
        orion::log_info("Debug mode: %s", debug_mode ? "true" : "false");
    }
    
	orion::log_debug("CommandLine:");
	for (int i = 0; i < argc; ++i) {
		orion::log_debug("\t%s", argv[i]);
	}
	orion::log_debug("Environment:");
	for (char** env = envp; *env != 0; ++env) {
		orion::log_debug("\t%s", *env);
	}

#ifndef NDEBUG
    // visually separate environment variables from the rest of the output
    printf("------------------------------------------------------------------------------------------------------------------------\n");
#endif

    // do not abort the program if SIGPIPE (broken pipe) signal is received
    signal(SIGPIPE, SIG_IGN);

    //flatpak specific migration to move existing dbs in wrong location
#if defined(FLATPAK_BUILD) && FLATPAK_BUILD
    std::filesystem::path datadir = orion::get_data_directory_path(APPLICATION_COMMAND, {});
    std::filesystem::path old_datadir = std::filesystem::path(std::getenv("HOME")) / ".var" / "app" / "com.kagi.Orion" / "data";

    //checks whether new db exists, if not, checks for old db,
    //if old db exists first moves the wal and shm, then moves old db
    //renames are atomic and our condition is a new db existing and 
    if(datadir != old_datadir)
    {
        std::filesystem::path old_db_path = old_datadir / "profiles.db";
        std::filesystem::path new_db_path = datadir / "profiles.db";

        std::error_code ec;

        if(!std::filesystem::is_regular_file(new_db_path, ec) && std::filesystem::is_regular_file(old_db_path, ec))
        {
            VERIFY(orion::ensure_parent_directories(new_db_path));
            orion::log_info("found profiles database in old path but not new, moving to new path...");

            std::filesystem::path old_shm_path = old_datadir / "profiles.db-shm";
            std::filesystem::path new_shm_path = datadir / "profiles.db-shm";
            if(std::filesystem::is_regular_file(old_shm_path, ec))
            {
                std::filesystem::rename(old_shm_path, new_shm_path, ec);
                if(ec)
                {
                    orion::log_error("failed to move %s to %s: %s", old_shm_path.c_str(), new_shm_path.c_str(), ec.message().c_str());
                    RAISE(orion::ErrorCode::io, "failed to move %s to %s: %s", old_shm_path.c_str(), new_shm_path.c_str(), ec.message().c_str());
                }
                else
                {
                    orion::log_info("moved %s to %s", old_shm_path.c_str(), (new_shm_path).c_str());
                }
            }
            std::filesystem::path old_wal_path = old_datadir / "profiles.db-wal";
            std::filesystem::path new_wal_path = datadir / "profiles.db-wal";
            if(std::filesystem::is_regular_file(old_wal_path, ec))
            {
                std::filesystem::rename(old_wal_path, new_wal_path, ec);
                if(ec)
                {
                    orion::log_error("failed to move %s to %s: %s", old_wal_path.c_str(), new_wal_path.c_str(), ec.message().c_str());
                    RAISE(orion::ErrorCode::io, "failed to move %s to %s: %s", old_wal_path.c_str(), new_wal_path.c_str(), ec.message().c_str());
                }
                else
                {
                    orion::log_info("moved %s to %s", old_wal_path.c_str(), new_wal_path.c_str());
                }
            }

            std::filesystem::rename(old_db_path, new_db_path, ec);

            if(ec)
            {
                orion::log_error("failed to move %s to %s: %s", old_db_path.c_str(), new_db_path.c_str(), ec.message().c_str());
                RAISE(orion::ErrorCode::io, "failed to move %s to %s: %s", old_db_path.c_str(), new_db_path.c_str(), ec.message().c_str());
            }
            else
            {
                orion::log_info("moved %s to %s", old_db_path.c_str(), new_db_path.c_str());
            }
        }
    }
#endif

    // Sets the ref count to 1
	GApplication* app = G_APPLICATION(orion_app_new());
    orion::log_verbose("main: app_ref_count=%u", G_OBJECT(app)->ref_count);

    // We have to replace GLib's g_application_run(app, argc, argv) call with
    // our custom setup because we want to use our own run loop that controls
    // the UI GLib loop. By default, GLib application does not need argc or
    // argv parameters so there is no need to pass them to GLib at the moment.

    // The issue is that g_application_run is the only method that unregisters
    // the application and which sets private is_registered flag to FALSE.
    // That is probably the only way to reset this flag after g_application_register
    // call sets it to TRUE. Also, g_object_new sets ref count to 1. Calling
    // g_application_register increments GObject ref count, setting it to 2.
    // Now, when trying to g_object_unref 2 times to destroy the application
    // (and calling its finalize method), a warning is emitted because
    // the internal is_registered flag is still TRUE.

    // Initialize the application (mimics g_application_run's startup).
    // This bumps the ref count by 1 only if the app object is a primary instance
    GError* err = NULL;
    gboolean success = g_application_register(app, NULL /* cancellable */, &err);
    if (!success) {
        orion::log_error("%s", err->message);
        g_clear_pointer(&err, g_error_free);
        RAISE(orion::ErrorCode::io, "failed to register application to d-bus");
    }

    if(g_application_get_is_remote(app))
    {
        GVariant* args = g_variant_ref_sink(g_variant_new_strv(argv, argc));
        defer([args]{ g_variant_unref(args); });

#if NDEBUG
        g_action_group_activate_action(G_ACTION_GROUP(app), "remote-activate", args);
#else
        RAISE(orion::ErrorCode::assert, "attempted debug launch but another instance of the app was already running,\n"
            "close that to ensure you're interacting with a new instance");
#endif
    }
    else
    {
        defer([app]{
            orion::log_verbose("main: app_ref_count=%u", G_OBJECT(app)->ref_count);
            g_object_unref(app);
            // Mostly vanity, the app object will still leak because we would need to call
            // another g_object_unref to destroy it here.
        });

        global_core_api = orion::CoreApi::create(APPLICATION_COMMAND); //intentionally passing APPLICATION_COMMAND for a more natural data path inside webkit
        auto core_api = global_core_api;
        core_api->set_debug_mode(debug_mode);
        core_api->set_password_storage(orion::app_password_storage());
        core_api->set_downloader_factory(orion::app_downloader_factory());

        VERIFY(core_api->init());
        
        orion_app_startup(ORION_APP(app));
        // Emits "activate" signal
        g_application_activate(app);

        if (exit_status) {
            *exit_status = EXIT_SUCCESS; // hard-coded at the moment
        }

        // Get the default GMainContext. There should be no need to destroy it, from documentation:
        // "The data is owned by the called function."
        GMainContext* context = g_main_context_default();

        IterationData data{};
        data.context = context;
        data.app = app;

        core_api->set_iteration(iteration_cb, &data, std::chrono::milliseconds{1});
        core_api->start_run_loop();

        orion_app_cleanup(ORION_APP(app));
    }

    

    SUCCEED();
}

static void
check_dispatch_arg_leaks()
{
    int count = orion::global_dispatch_arg_count.load(std::memory_order_relaxed);
    if (count != 0) {
        // This means that DispatchArg objects in orion RunLoop were not destroyed properly.
        orion::log_error("reamining DispatchArg objects count: %d (but 0 expected)", count);
    } else {
        orion::log_verbose("remaining DispatchArg objects count: %d", count);
    }
}

static void
check_core_api_leaks()
{
    int count = orion::global_core_api_count.load(std::memory_order_relaxed);
    if (count != 0) {
        // This means that CoreApi objects were not destroyed properly.
        orion::log_error("remaining CoreApi objects count: %d (but 0 expected)", count);
    } else {
        orion::log_verbose("remaining CoreApi objects count: %d", count);
    }
}

static void
check_run_loop_leaks()
{
    int count = orion::global_run_loop_count.load(std::memory_order_relaxed);
    if (count != 0) {
        // This means that RunLoop objects were not destroyed properly.
        orion::log_error("remaining RunLoop objects count: %d (but 0 expected)", count);
    } else {
        orion::log_verbose("remaining RunLoop objects count: %d", count);
    }
}

static void
check_webview_factory_leaks()
{
    int count = WebExtensions::global_webview_factory_count.load(std::memory_order_relaxed);
    if (count != 0) {
        // This means that WebExtensionWebViewFactoryImpl objects were not destroyed properly.
        orion::log_error("remaining WebExtensionWebViewFactoryImpl objects count: %d (but 0 expected)", count);
    } else {
        orion::log_verbose("remaining WebExtensionWebViewFactoryImpl objects count: %d", count);
    }
}

static void
check_object_leaks()
{
    check_dispatch_arg_leaks();
    check_core_api_leaks();
    check_run_loop_leaks();
    check_webview_factory_leaks();
}

int
main(int argc, char** argv, char** envp)
{
    int exit_status = 0;
    orion::Error err = init(argc, argv, envp, &exit_status);
    if (err) {
        orion::log_error("unhandled error (%s): %s", err.code_name().c_str(), err.to_string().c_str());
        return EXIT_FAILURE;
    }
    orion::log_verbose("orion exiting gracefully with exist status %d", exit_status);

    // This reset may still be needed to ensure that the global CoreApi is properly destroyed.
    // But it is probably unnecessary.
    global_core_api = nullptr;

    check_object_leaks();

	return exit_status;
}
