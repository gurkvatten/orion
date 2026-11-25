#pragma once

#include <functional>
#include <memory>

#include <common/src/error.h>
#include <core/dto/profile_dto.h>

namespace orion { class CoreApi; }

namespace orion {

class SyncService {
public:
    class Handle {
    public:
        virtual ~Handle() = default;
        virtual void cancel() = 0;
    };

    using OperationHandle = std::shared_ptr<Handle>;
    typedef std::function<void(int done, int total, const std::string& description)> ProgressHandler;
    typedef std::function<void(Error result)> ExportToCompletionHandler;
    typedef std::function<void(Error result)> ImportFromCompletionHandler;
    virtual OperationHandle export_to(orion::ProfileId profile_id, const std::string& directory, ProgressHandler progress_handler, ExportToCompletionHandler completion_handler) = 0;
    virtual OperationHandle import_from(orion::ProfileId profile_id, const std::string& source, ProgressHandler progress_handler, ImportFromCompletionHandler completion_handler) = 0;

private:
    static std::shared_ptr<SyncService> create(orion::CoreApi* api);
    friend class orion::CoreApi;
};

}
