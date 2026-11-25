#include <gio/gio.h>

#include <common/src/log.h>
#include <common/src/defer.h>
#include <core/test/core_api_test.h>

namespace orion {

void
test_file_download_using_gio()
{
    GFile* file = g_file_new_for_uri("https://addons.mozilla.org/firefox/downloads/file/4412673/ublock_origin-1.62.0.xpi");
    defer([file]{
        g_object_unref(file);
    });
    g_file_read_async(file, G_PRIORITY_DEFAULT, nullptr, [](GObject* source_object, GAsyncResult* res, gpointer user_data) {
        GError* error = NULL;
        GFileInputStream* stream = g_file_read_finish(G_FILE(source_object), res, &error);
        if (!stream) {
            log_error("failed to open stream: %s", error->message);
            g_clear_error(&error);
            return;
        }
        defer ([stream]{
            g_object_unref(stream);
        });

        // Read from the stream synchronously here for simplicity
        char buffer[4096];
        gssize bytes_read;
        int64_t total_bytes_read = 0;

        while ((bytes_read = g_input_stream_read(G_INPUT_STREAM (stream),
                                                buffer,
                                                sizeof (buffer),
                                                NULL,
                                                &error)) > 0) {
            total_bytes_read += bytes_read;
        }

        if (bytes_read < 0) {
            log_error("read error: %s", error->message);
            g_clear_error(&error);
        }
        log_debug("finished reading file stream, total bytes read: %lld", total_bytes_read);
    }, nullptr);
}

} // namespace orion