#include <glib.h>
#include <gio/gio.h>
#include <string>
#include <functional>

#include <common/src/coroutine.h>

namespace orion {

//returned string might include null characters
detached_task get_host_path(std::string_view path, std::function<void(std::string_view)> cb);

// The caller of the function takes ownership of the returned data and is responsible for freeing it using g_free
gchar* load_text_from_resource(const char* resource_path);

// Deletes the given directory (or file) recursively
gboolean delete_recursively(GFile* file, GError** error);

void make_directory_with_parents_async(GFile* file, GCancellable* cancellable, GAsyncReadyCallback cb, gpointer user_data);
gboolean make_directory_with_parents_finish(GFile* file, GAsyncResult* res, GError** err);

gboolean write_file_if_needed(GFile* file, GBytes* bytes, GCancellable* cancellable, GError** err);
void write_file_if_needed_async(GFile* file, GBytes* bytes, GCancellable* cancellable, GAsyncReadyCallback cb, gpointer user_data);
gboolean write_file_if_needed_finish(GFile* file, GAsyncResult* res, GError** err);

} // namespace orion