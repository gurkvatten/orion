#include "url_image.h"
#include "util/ref_util.h"
#include "common/src/memory.h"
#include <libsoup/soup.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <map>

struct SoupAsyncArg {
    std::unique_ptr<orion::gtk::WeakRefWrapper> image_weak_ref;
    std::string url;
};

static
std::map<std::string, GdkTexture*> g_url_image_cache;

static
void load_image_from_url(GtkImage* image, const char* url)
{
    if(!url) {
        return;
    }
    auto it = g_url_image_cache.find(url);
    if(it != g_url_image_cache.end()) {
        if(it->second) {
            gtk_image_set_from_paintable(image, GDK_PAINTABLE(it->second));
        }
        return;
    }
    SoupSession* session = soup_session_new();
    SoupMessage* msg = soup_message_new("GET", url);
    if(!msg) {
        return;
    }

    auto arg = std::make_unique<SoupAsyncArg>();
    arg->image_weak_ref = std::make_unique<orion::gtk::WeakRefWrapper>(image);
    arg->url = url;

    soup_session_send_async(session, msg, 0, nullptr,
        [](GObject* source, GAsyncResult* res, gpointer user_data) {
            std::unique_ptr<SoupAsyncArg> arg((SoupAsyncArg*)user_data);
            GError* error = nullptr;

            UNIQUE_PTR(GInputStream, g_object_unref) stream(
                soup_session_send_finish(SOUP_SESSION(source), res, &error));

            if (!stream) {
                g_url_image_cache[arg->url] = nullptr;
                g_warning("Download failed: %s", error->message);
                g_error_free(error);
                return;
            }

            // Read stream into bytes
            GByteArray* buf = g_byte_array_new();
            guint8 tmp[4096];
            gssize n;

            while ((n = g_input_stream_read(stream.get(), tmp, sizeof(tmp), nullptr, &error)) > 0) {
                g_byte_array_append(buf, tmp, n);
            }
            GBytes* bytes = g_byte_array_free_to_bytes(buf);

            // Create texture
            UNIQUE_PTR(GdkTexture, g_object_unref) tex(gdk_texture_new_from_bytes(bytes, &error));
            g_bytes_unref(bytes);

            if (!tex) {
                g_url_image_cache[arg->url] = nullptr;
                g_warning("Failed to create texture: %s", error->message);
                g_error_free(error);
                return;
            }

            auto ref = arg->image_weak_ref->lock();
            if (!ref) {
                return;
            }
            GtkImage* image = GTK_IMAGE(ref.get());

            // Set paintable
            gtk_image_set_from_paintable(image, GDK_PAINTABLE(tex.get()));

            // Update cache
            g_url_image_cache[arg->url] = tex.release();
        },
        arg.release());
}

struct _UrlImage {
    AdwBin parent_instance;
    char  *url;
    GtkImage *image;
};

G_DEFINE_TYPE (UrlImage, url_image, ADW_TYPE_BIN)

enum {
    PROP_0,
    PROP_URL,
    PROP_PIXEL_SIZE,
    N_PROPS
};
static GParamSpec *props[N_PROPS];

void
url_image_set_url(UrlImage* self, const char *url)
{
    bool reload_image = true;
    if(!url) {
        reload_image = false;
    }
    if(url && self->url && std::string(url) == std::string(self->url)) {
        reload_image = false;
    }
    if(!url || !*url) {
        reload_image = false;
    }
    
    g_free (self->url);
    self->url = g_strdup (url);

    if (reload_image) {
        load_image_from_url (self->image, self->url);
    }
}

const char*
url_image_get_url(UrlImage* self)
{
    return self->url;
}

static void
url_image_set_property(GObject *obj, guint prop_id, const GValue *value, GParamSpec *pspec)
{
    UrlImage* self = APP_URL_IMAGE (obj);
    switch (prop_id) {
    case PROP_URL:
        url_image_set_url (self, g_value_get_string (value));
        break;
    case PROP_PIXEL_SIZE:
        gtk_image_set_pixel_size (self->image, g_value_get_int (value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
    }
}

static void
url_image_get_property(GObject *obj, guint prop_id, GValue *value, GParamSpec *pspec)
{
    UrlImage *self = APP_URL_IMAGE (obj);
    switch (prop_id) {
    case PROP_URL: g_value_set_string (value, self->url); break;
    case PROP_PIXEL_SIZE: g_value_set_int (value, gtk_image_get_pixel_size(self->image)); break;
    default: G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
    }
}

static void
url_image_dispose(GObject *obj)
{
    UrlImage *self = APP_URL_IMAGE (obj);
    g_clear_pointer (&self->url, g_free);
    G_OBJECT_CLASS (url_image_parent_class)->dispose (obj);
}

static void
url_image_class_init(UrlImageClass *klass)
{
    GObjectClass *gobj_class = G_OBJECT_CLASS (klass);
    gobj_class->set_property = url_image_set_property;
    gobj_class->get_property = url_image_get_property;
    gobj_class->dispose = url_image_dispose;

    props[PROP_URL] =
        g_param_spec_string ("url", "URL", "Image URL",
                             NULL,
                             (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    props[PROP_PIXEL_SIZE] =
        g_param_spec_int ("pixel-size", "Pixel Size", "Pixel Size of the image",
                          -1, G_MAXINT, -1,
                          (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    g_object_class_install_properties (gobj_class, N_PROPS, props);
}

static void
url_image_init(UrlImage* self)
{
    self->image = GTK_IMAGE (gtk_image_new ());
    adw_bin_set_child (ADW_BIN (self), GTK_WIDGET (self->image));
}
