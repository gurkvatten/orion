#pragma once

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
#include <webkit/webkit.h>
#endif

#include "app.h"

namespace orion::gtk {

//TODO: FIXME: modify this to instead use a caching system
//(currently lacking in speed, creates a webview per call, inefficient)

//function takes a reference to label
void load_gtk_label_from_uri_description(GtkLabel* label, const gchar* uri);

//caller is the owner of data and is responsible for freeing
//callbakck recieves NULL as description string on fail
template<typename T> void execute_with_uri_description(void(*cb)(const char*, T*), const gchar* uri, T* data);

} // namespace orion::gtk