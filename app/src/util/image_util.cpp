#include "image_util.h"
#include <common/src/log.h>
#include <common/src/defer.h>

namespace orion::gtk {

// The caller takes ownership of the data and is responsible for freeing it.
GdkTexture*
create_transparent_grayscale_favicon(GdkTexture* src, double alpha)
{
    int width = gdk_texture_get_width(src);
    int height = gdk_texture_get_height(src);

    gsize stride = width * 4;
    guchar *pixels = static_cast<guchar*>(g_malloc(stride * height));

    gdk_texture_download(src, pixels, stride);

    for (int y = 0; y < height; y++) {
        guchar *row = pixels + y * stride;
        for (int x = 0; x < width; x++) {
            guchar *px = row + x * 4;

            // Format is RGBA (8-bit per channel)
            guchar r = px[0];
            guchar g = px[1];
            guchar b = px[2];
            guchar a = px[3];

            // Compute luminance (Rec. 709 luma formula)
            guchar gray = (guchar)(0.2126*r + 0.7152*g + 0.0722*b);

            px[0] = gray;
            px[1] = gray;
            px[2] = gray;

            // Apply alpha multiplier
            px[3] = (guchar)(a * alpha);
        }
    }

    GBytes *bytes = g_bytes_new_take(pixels, stride * height);
    GdkTexture *tex = gdk_memory_texture_new(width, height, GDK_MEMORY_DEFAULT, bytes, stride);
    g_bytes_unref(bytes);

    return tex;
}

GdkTexture*
convert_g_themed_icon_to_texture(GThemedIcon* icon, int size)
{
    // Check if the GIcon is a GThemedIcon
    if (!G_TYPE_CHECK_INSTANCE_TYPE(icon, G_TYPE_THEMED_ICON)) {
        log_warning("convert_g_themed_icon_to_texture: icon is not GThemedIcon");
        return NULL;
    }

    // Get the default icon theme
    GtkIconTheme* icon_theme = gtk_icon_theme_get_for_display(gdk_display_get_default());
    if (!icon_theme) {
        log_warning("convert_g_themed_icon_to_texture: failed to get default icon theme");
        return NULL;
    }

    // Look up the icon as a GtkIconPaintable
    GtkIconPaintable* paintable = gtk_icon_theme_lookup_by_gicon(
        icon_theme,
        G_ICON(icon),     // The GThemedIcon
        size,             // Desired icon size
        1,                // Scale factor (1 for standard DPI)
        GTK_TEXT_DIR_LTR, // Text direction
        GTK_ICON_LOOKUP_FORCE_REGULAR // TODO: is this flag appropriate?
    );

    if (!paintable) {
        log_warning("convert_g_themed_icon_to_texture: failed to look up icon paintable");
        return NULL;
    }

    defer([paintable]{ g_object_unref(paintable); });

    // Get the GFile for the icon
    GFile* file = gtk_icon_paintable_get_file(paintable);
    if (!file) {
        log_warning("convert_g_themed_icon_to_texture: no file for icon paintable");
        return NULL;
    }
    defer([file]{ g_object_unref(file); });

    // Reference the file (since paintable owns it, and we'll unref paintable soon)
    g_object_ref(file);

    // Load the file as a GdkTexture
    GError* error = NULL;
    GdkTexture* texture = gdk_texture_new_from_file(file, &error);
    if (!texture) {
        log_warning("convert_g_themed_icon_to_texture: failed to load texture from file: %s", error->message);
        g_clear_error(&error);
        return NULL;
    }

    return texture;
}

GdkTexture*
convert_icon_to_texture(GIcon* icon, int size)
{
    if (G_IS_THEMED_ICON(icon)) {
        return convert_g_themed_icon_to_texture(G_THEMED_ICON(icon), size);
    }
    else
    {
        if(icon)
        {
            return GDK_TEXTURE(g_object_ref(icon));
        }
        else
        {
            return NULL;
        }
    }
}

} // namespace orion::gtk