#pragma once

#include <core/dto/profile_dto.h>


namespace orion {

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT

#include <webkit/webkit.h>


void register_cookie_max_age_clamp(WebKitCookieManager* mgr, orion::ProfileId profile_id);

#endif

} //namespace orion