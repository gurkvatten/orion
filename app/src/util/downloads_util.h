#pragma once

#include <filesystem>

#include <core/dto/profile_dto.h>

#include "ref_util.h"

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
#include <webkit/webkit.h>
#endif


namespace orion {

//bool for whether is successful, path to download to if successful, pointer to the object that is holding this file open
typedef std::function<void(bool, std::filesystem::path)> DownloadDestinationCallback;

//asynchronously calls this callback with a full path, inluding filename
//will always call callback even on failure
void get_download_destination(orion::ProfileId id, std::string recommended_filename, DownloadDestinationCallback&& cb);


#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT

//attempts to hijack standard http downloads to allow download pausing/continuation
void on_download_started(WebKitNetworkSession* self, WebKitDownload* download, orion::ProfileId* profile_id);

#endif

} // namespace orion