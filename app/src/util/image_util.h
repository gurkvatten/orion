#pragma once

#include <gtk/gtk.h>

namespace orion::gtk {

const double SUSPENDED_FAVICON_ALPHA = 0.7;

GdkTexture* create_transparent_grayscale_favicon(GdkTexture* src, double alpha);
GdkTexture* convert_g_themed_icon_to_texture(GThemedIcon* icon, int size);
GdkTexture* convert_icon_to_texture(GIcon* icon, int size);

} // namespace orion::gtk