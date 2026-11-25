#pragma once
#include <gtk/gtk.h>

#include <core/dto/profile_dto.h>
#include <core/private_mode.h>

#include "app_profile_data_holder.h"
#include "webextensions/webview_factory_host.h"

#include "data_item/back_forward_list.h"

G_BEGIN_DECLS

//current consumers: MainWindow, OrionWebviewPreviewWindow
#define ORION_TYPE_WINDOW (orion_window_get_type())
G_DECLARE_INTERFACE(OrionWindow, orion_window, ORION, WINDOW, GtkWindow)

struct _OrionWindowInterface {
    GTypeInterface parent;
};

namespace orion {

// Mixed content test page:
// https://googlesamples.github.io/web-fundamentals/fundamentals/security/prevent-mixed-content/simple-example.html
// https://googlesamples.github.io/web-fundamentals/fundamentals/security/prevent-mixed-content/active-mixed-content.html
enum class TlsStatus : int64_t {
    none, //used for when no content is currently on screen
    secure, //connection is secured through tls
    mixed, //some content is from an insecure origin (insecure-content-detected)
    failure, //connection's tls logic produced an error
    insecure, //connection doesn't utilise tls
    local, //content origin is local machine
};

};

orion::WindowId orion_window_get_window_id(OrionWindow* self);
orion::ProfileId orion_window_get_profile_id(OrionWindow* self);
ProfileDataHolder* orion_window_get_profile_data_holder(OrionWindow* self);
orion::PrivateMode orion_window_get_private_mode(OrionWindow* self);
WebExtensions::WebExtensionWebViewFactory& orion_window_get_webview_factory(OrionWindow* self);
std::string orion_window_get_uri(OrionWindow* self);
orion::TlsStatus orion_window_get_tls_status(OrionWindow* self);

//these are somewhat inefficient, as they copy data (GValue doesn't allow copying out),
//you can manually use g_object_get_property to avoid this
orion::BackForwardList* orion_window_get_back_list(OrionWindow* self); //should start from newest (closest to current)
orion::BackForwardList* orion_window_get_forward_list(OrionWindow* self); //should start from oldest (closest to current)

G_END_DECLS
