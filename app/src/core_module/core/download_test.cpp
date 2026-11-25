#define ENABLE_DOWNLOAD_TEST !NDEBUG && 0
#define ENABLE_VERBOSE_LOGS 0

#include <core/download_test.h>
#include <core/core_api.h>
#include <common/src/error.h>
#include <common/src/defer.h>
#include <core/downloader.h>
#include <common/src/file_system.h>
#include <filesystem>

namespace orion {

Error
test_downloader(CoreApi* api)
{
    auto dl = api->create_downloader();
    if(!dl) {
        SUCCEED();
    }
    dl->set_uri("https://example-files.online-convert.com/document/txt/example.txt");
    //dl->set_uri("https://addons.mozilla.org/firefox/downloads/file/4412673/ublock_origin-1.62.0.xpi");
    std::filesystem::path path = get_data_directory_path(api->app_id().c_str(), { "test", "downloader", "example.txt" });
    VERIFY(ensure_parent_directories(path));
    dl->set_target_path(path.string());
    dl->set_finished_cb([dl](Error err) {
        VERIFY_LOG(err);
        log_verbose("test_downloader: download finished: %s", dl->target_path().c_str());
    });
    dl->set_progress_cb([dl](const Downloader::Progress& progress) {
        log_verbose("test_downloader: download progress: chunk_received:%zu total_received:%zu content_length:%zu estimated_progress:%f",
            progress.chunk_received, progress.total_received, progress.content_length, progress.estimated_progress);    
        //dl->cancel(); // cancellation test
    });
    dl->set_overwrite_existing(true);
    dl->start_download();
    SUCCEED();
}

void
test_download(CoreApi* api)
{
#if ENABLE_DOWNLOAD_TEST
    VERIFY_LOG(test_downloader(api));
#endif
}

} // namespace orion
