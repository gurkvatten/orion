#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define APP_TYPE_URL_IMAGE_WIDGET (url_image_get_type())
G_DECLARE_FINAL_TYPE (UrlImage, url_image, APP, URL_IMAGE, AdwBin)

void url_image_set_url(UrlImage* self, const char *url);
const char* url_image_get_url(UrlImage* self);

G_END_DECLS
