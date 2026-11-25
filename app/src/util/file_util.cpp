#include "file_util.h"

#include <glib.h>
#include <gtk/gtk.h>
#include <adwaita.h>

#include <iostream>
#include <coroutine>

#include <common/src/log.h>
#include <common/src/defer.h>
#include <common/src/string_util.h>

#include "util/coro_util.h"

#include "app.h"

namespace orion {




static async_task<std::string>
get_document_portal_mount_point()
{
    static orion::AsyncInitObject<std::string> documents_portal_mount_point;
    bool needs_init = co_await documents_portal_mount_point.wait_or_init();

    if(needs_init)
    {
        auto result = co_await orion::gtk::await<g_dbus_connection_call, g_dbus_connection_call_finish>(
            g_application_get_dbus_connection(g_application_get_default()),
            "org.freedesktop.portal.Documents",
            "/org/freedesktop/portal/documents",
            "org.freedesktop.portal.Documents",
            "GetMountPoint",
            nullptr,
            G_VARIANT_TYPE("(ay)"),
            G_DBUS_CALL_FLAGS_NONE,
            -1,
            nullptr
        );

        std::string mount_point;

        if(!orion::gtk::log_result_error(result))
        {
            GVariant* return_val = result.value();
            defer([return_val]{ g_variant_unref(return_val); });

            GVariant* byte_array = g_variant_get_child_value(return_val, 0);
            defer([byte_array]{ g_variant_unref(byte_array); });
            gsize len = 0;
            const guchar* bytes = (const guchar*)g_variant_get_fixed_array(byte_array, &len, sizeof(guchar));

            if(bytes && len > 0) {
                size_t actual_len = (bytes[len - 1] == '\0') ? len - 1 : len;
                mount_point = std::string(reinterpret_cast<const char*>(bytes), actual_len);
            }

            if(!mount_point.empty() && mount_point.back() != '/')
            {
                mount_point += '/';
            }
        }

        documents_portal_mount_point.set_value_and_resume(mount_point);
    }

    co_return documents_portal_mount_point.get();
}

async_task<std::filesystem::path>
get_host_path_coro(std::filesystem::path path)
{
    std::filesystem::path document_portal_mount_point = co_await get_document_portal_mount_point();

    if(document_portal_mount_point.empty()) co_return path;

    //logic for whether path is a strict child of document_portal_mount_point
    //checks whether mismatch between paths is at the end of mount point and path still continues after this
    auto [mount_point_it, path_it] = std::mismatch(
        document_portal_mount_point.begin(), document_portal_mount_point.end(),
        path.begin(), path.end()
    );



    //a path with trailing slashes can produce multiple empty nodes, we treat those as path ends as well, most commonly for a single trailing slash

    bool mount_point_nodes_after_are_empty = true; //will also be kept true if we've already hit the end
    for(auto it = mount_point_it; it != document_portal_mount_point.end(); ++it)
    {
        if(!it->empty())
        {
            mount_point_nodes_after_are_empty = false;
            break;
        }
    }

    bool more_than_2_path_nodes_after_are_not_empty = false; //only attempt to resolve to host path if path includes a node for id and a second one for exported file
    int counter = 0;
    for(auto it = path_it; it != path.end(); ++it)
    {
        if(!it->empty())
        {
            counter++;
            if(counter >= 2)
            {
                more_than_2_path_nodes_after_are_not_empty = true;
                break;
            }
        }
    }

    if (mount_point_nodes_after_are_empty && more_than_2_path_nodes_after_are_not_empty)
    {
        std::string doc_id = *path_it;

        const gchar* doc_ids[] = {
            doc_id.c_str(),
            NULL
        };

        GVariant* params = g_variant_ref_sink(g_variant_new("(^as)", doc_ids));
        defer([params]{ g_variant_unref(params); });

        auto result = co_await orion::gtk::await<g_dbus_connection_call, g_dbus_connection_call_finish>(
            g_application_get_dbus_connection(g_application_get_default()),
            "org.freedesktop.portal.Documents",
            "/org/freedesktop/portal/documents",
            "org.freedesktop.portal.Documents",
            "GetHostPaths",
            params,
            G_VARIANT_TYPE("(a{say})"),
            G_DBUS_CALL_FLAGS_NONE,
            -1,
            nullptr
        );

        if(!orion::gtk::log_result_error(result))
        {
            GVariant* return_value = result.value();
            defer([return_value]{ g_variant_unref(return_value); });

            GVariantIter* iter;
            gchar* returned_doc_id;
            GVariant* host_path_bytes;

            g_variant_get(return_value, "(a{say})", &iter);
            defer([iter]{ g_variant_iter_free(iter); });

            while(g_variant_iter_loop(iter, "{s@ay}", &returned_doc_id, &host_path_bytes))
            {
                if(0 == g_strcmp0(returned_doc_id, doc_id.c_str()))
                {
                    gsize len;
                    const guchar* bytes = static_cast<const guchar*>(g_variant_get_fixed_array(host_path_bytes, &len, sizeof(guchar)));

                    size_t actual_len = orion::str::strnlen0(reinterpret_cast<const char*>(bytes), len);

                    if(actual_len)
                    {
                        //safety measure because bytes may or may not contain a nullterm, meaning 
                        std::filesystem::path result = std::string_view(reinterpret_cast<const char*>(bytes), actual_len); 
                        
                        //path_it needs to point to the id node in (document_portal_mount_point)/(doc_id)
                        
                        //result will always (TODO: fact check this always, root folder edgecase for example) include the exported document itself,
                        //which is the immediate child of the (doc_id) folder we wish to strip that out
                        result = result.lexically_normal();
                        if(result.has_relative_path())
                        {
                            if(!result.has_filename()) //meaning we have a trailing slash, has_relative_path also means result is not root folder
                            {
                                result = result.parent_path();
                            }

                            result = result.parent_path();
                        }


                        for(++path_it; path_it != path.end(); ++path_it)
                        {
                            result /= *path_it;
                        }

                        co_return result;
                    }
                    else
                    {
                        co_return path;
                    }
                }
            }
            
            orion::log_error("org.freedesktop.portal.Documents::GetHostPaths failed to return for given document id %s", doc_id.c_str());
            co_return path;
        }
        else
        {
            co_return path;
        }
    }
    else
    {
        co_return path;
    }
}

detached_task
get_host_path(std::string_view path, std::function<void(std::string_view)> cb)
{
#if defined(FLATPAK_BUILD) && FLATPAK_BUILD
    std::filesystem::path host_path = co_await get_host_path_coro(path);
    cb(host_path.string());
#else
    co_return cb(path);
#endif
}

gchar*
load_text_from_resource(const char* resource_path)
{
    GError* local_error = NULL;
    GBytes* bytes = g_resources_lookup_data(resource_path, G_RESOURCE_LOOKUP_FLAGS_NONE, &local_error);
    if (!bytes) {
        orion::log_warning("failed to load gresource: %s", local_error->message);
        g_clear_error(&local_error);
        return NULL;
    }
    defer([bytes]{ g_bytes_unref(bytes); });

    gsize size;
    const gchar* data = static_cast<const gchar*>(g_bytes_get_data(bytes, &size));

    gchar* text = g_strndup(data, size);
    return text;
}

gboolean
delete_recursively(GFile* file, GError** error)
{
    GFileType type = g_file_query_file_type(file, G_FILE_QUERY_INFO_NONE, NULL);

    if (type == G_FILE_TYPE_DIRECTORY) {
        GFileEnumerator* enumerator = g_file_enumerate_children(file,
            G_FILE_ATTRIBUTE_STANDARD_NAME ","
            G_FILE_ATTRIBUTE_STANDARD_TYPE,
            G_FILE_QUERY_INFO_NONE,
            NULL,
            error);
        if (!enumerator) return FALSE;

        GFileInfo* info = NULL;
        while ((info = g_file_enumerator_next_file(enumerator, NULL, error))) {
            GFile* child = g_file_get_child(file, g_file_info_get_name(info));

            if (!delete_recursively(child, error)) {
                g_object_unref(child);
                g_object_unref(info);
                g_object_unref(enumerator);
                return FALSE;
            }

            g_object_unref(child);
            g_object_unref(info);
        }

        g_object_unref(enumerator);
    }

    return g_file_delete(file, NULL, error);
}

static void
make_directory_with_parents_thread_func(GTask* task, gpointer source_object, gpointer task_data, GCancellable* cancellable)
{
    GFile* file = G_FILE(source_object);
    GError* err = NULL;

    if(g_file_make_directory_with_parents(file, cancellable, &err))
    {
        g_task_return_boolean(task, TRUE);
    }
    else
    {
        if(g_error_matches(err, G_IO_ERROR, G_IO_ERROR_EXISTS))
        {
            g_error_free(err);
            g_task_return_boolean(task, TRUE);
        }
        else
        {
            g_task_return_error(task, err);
        }
    }
}

void
make_directory_with_parents_async(GFile* file, GCancellable* cancellable, GAsyncReadyCallback cb, gpointer user_data)
{
    GTask* task = g_task_new(file, cancellable, cb, user_data);
    g_task_set_source_tag(task, (gpointer)make_directory_with_parents_async);

    g_task_run_in_thread(task, make_directory_with_parents_thread_func);
    g_object_unref(task);
}

gboolean
make_directory_with_parents_finish(GFile* file, GAsyncResult* res, GError** err)
{
    g_return_val_if_fail(g_task_is_valid (res, file), FALSE);
    return g_task_propagate_boolean(G_TASK(res), err);
}



gboolean
write_file_if_needed(GFile* file, GBytes* bytes, GCancellable* cancellable, GError** err)
{
    GError* local_error = NULL;
    gsize len = 0;
    const char* data = static_cast<const char*>(g_bytes_get_data(bytes, &len));

    GFileInfo* info = g_file_query_info(file, G_FILE_ATTRIBUTE_STANDARD_SIZE "," G_FILE_ATTRIBUTE_ETAG_VALUE, G_FILE_QUERY_INFO_NONE, cancellable, &local_error);
    if(g_error_matches(local_error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
    {
        g_clear_error(&local_error);

        return g_file_replace_contents(file, data, len, NULL, FALSE, G_FILE_CREATE_REPLACE_DESTINATION, NULL, cancellable, err);
    }
    if(local_error)
    {
        g_propagate_error(err, local_error);
        return FALSE;
    }
    defer([info]{ g_object_unref(info); });

    goffset size = g_file_info_get_size(info);
    if(size < 0)
    {
        if(err) *err = g_error_new(G_IO_ERROR, G_IO_ERROR_NOT_REGULAR_FILE, "file reported negative size");
        return FALSE;
    }
    if((gsize)size != g_bytes_get_size(bytes))
    {
        return g_file_replace_contents(file, data, len, g_file_info_get_etag(info), FALSE, G_FILE_CREATE_REPLACE_DESTINATION, NULL, cancellable, err);
    }

    //orion::log_debug("font file exists with correct size");
    return TRUE;
}

static void
write_file_if_needed_thread_func(GTask* task, gpointer source_object, gpointer task_data, GCancellable* cancellable)
{
    GFile* file = G_FILE(source_object);
    GError* err = NULL;

    if(write_file_if_needed(file, static_cast<GBytes*>(task_data), cancellable, &err))
    {
        g_task_return_boolean(task, TRUE);
    }
    else
    {
        g_task_return_error(task, err);
    }
}

void write_file_if_needed_async(GFile* file, GBytes* bytes, GCancellable* cancellable, GAsyncReadyCallback cb, gpointer user_data)
{
    GTask* task = g_task_new(file, cancellable, cb, user_data);
    g_task_set_source_tag(task, (gpointer)write_file_if_needed_async);
    g_task_set_task_data(task, g_bytes_ref(bytes), [](gpointer data){ g_bytes_unref(static_cast<GBytes*>(data)); });

    g_task_run_in_thread(task, write_file_if_needed_thread_func);
    g_object_unref(task);
}

gboolean
write_file_if_needed_finish(GFile* file, GAsyncResult* res, GError** err)
{
    g_return_val_if_fail(g_task_is_valid (res, file), FALSE);
    return g_task_propagate_boolean(G_TASK(res), err);
}

} // namespace orion