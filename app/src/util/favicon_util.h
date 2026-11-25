#pragma once

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
#include <webkit/webkit.h>
#endif

#include <common/src/coroutine.h>

#include "app.h"

#include "window/main_window/orion_window_iface.h"

namespace orion::gtk {

void load_gtk_image_favicon_from_uri(OrionWindow* win, GtkImage* image, const gchar* uri);
async_task<GIcon*> get_icon_for_uri(OrionWindow* win, const orion::Uri& uri);

} // namespace orion::gtk