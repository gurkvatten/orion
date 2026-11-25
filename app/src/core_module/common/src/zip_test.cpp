#define ENABLE_VERBOSE_LOGS 0

#include <src/zip_test.h>
#include <src/log.h>
#include <zip.h>
#include <src/error.h>
#include <src/zipper.h>
#include <src/file_system.h>
#include <filesystem>

namespace orion {

void
test_zip(const char* app_id)
{
    log_verbose("test_zip: starting");

    auto zip_path = get_data_directory_path(app_id, { "test", "zipper", "ublock_origin-1.62.0.zip" });
    VERIFY_LOG(ensure_parent_directories(zip_path));
    std::error_code ec;
    bool exists = std::filesystem::exists(zip_path, ec);
    if (ec) {
        log_warning("test_zip: error checking existence of zip file: %s", ec.message().c_str());
        return;
    }
    
    if (exists) {
        auto zipper = Zipper::create();
        Zipper::Node root{};
        VERIFY_LOG(zipper->extract_to_memory(zip_path, &root));
        CHECK_LOG(root.children.size() == 30, "expected 30 entries in test zip file");
    }
}

} // namespace orion