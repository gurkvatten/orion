#include <src/internal/workers.h>
#include <src/internal/error_payload.h>
#include <src/internal/file_client.h>
#include <../common/src/conversion_util.h> 
#include <../common/src/thread_util.h> 
#include <../common/src/type_info.h> 
#include <rfl/json.hpp>
#include <stdexcept>
#ifdef _WIN32
#include <winsock2.h>
#include <shlwapi.h>
#pragma comment(lib, "shlwapi.lib")
static inline int fd_close(int fd) { return closesocket(fd); }
#else
#include <unistd.h>
static inline int fd_close(int fd) { return close(fd); }
#endif

namespace orion::sync {
bool
Worker::verify(const Error& error, int8_t directionFlags)
{
    if (error) {
        if (m_finished_cb(m_sync_id, m_lastError = error)) {
            m_running &= ~directionFlags;
        } else {
            //the callback decided to continue -> clear the recent error, make it explicit here rather than set it only on the callabck returning true
            m_lastError = Error::ok();
        }
        return false;
    }
    return true;
}

bool
Worker::start()
{
    orion::set_current_thread_name(TypeInfo::strip_ns(TypeInfo::type_name(typeid(*this).name())).c_str());
    return true;
}

void
Worker::cleanup()
{
    if (m_fd != -1) {
        fd_close(m_fd);
        m_fd = -1;
    }
}

void
ServerWorker::rollback(const Error& reason)
{
    Error e = m_syncService->rollback();
    if (e) {
        m_finished_cb(m_sync_id, e);
    }
}

void
ServerWorker::cleanup()
{
    Worker::cleanup();
    m_syncService->cleanup();
}


template<class C>
struct SyncExternalService : SyncExternalService_Interface {
    SyncExternalService(C* client) :
        m_client(client) {
    }
    std::unique_ptr<C> m_client;

    Error read(const std::string& collection, std::string& buffer) {
        return m_client->read(collection, buffer);
    };

    Error write(const std::string& collection, const std::string& buffer) {
        return m_client->write(collection, buffer);
    };

    Error exchange(const std::string& collection, std::string& buffer) {
        return m_client->exchange(collection, buffer);
    };

    Error rollback() {
        return m_client->rollback();
    };

    void cleanup() {
        m_client->cleanup();
    };
};

std::unique_ptr<SyncExternalFacade>
SyncExternalFacade::instance(const Uri&& uri)
{
    return std::unique_ptr<SyncExternalFacade>(new SyncExternalFacade(std::forward<const Uri>(uri)));
}

SyncExternalFacade::SyncExternalFacade(const Uri&& uri) :
    m_uri(uri)
{
    if (uri.scheme() == "file") {
#ifdef _WIN32
        int wuri_len = MultiByteToWideChar(CP_UTF8, 0, uri.uri().c_str(), -1, nullptr, 0);
        std::wstring wuri(wuri_len - 1, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, uri.uri().c_str(), -1, wuri.data(), wuri_len);

        wchar_t winPath[MAX_PATH];
        DWORD pathLen = MAX_PATH;
        HRESULT hr = PathCreateFromUrlW(wuri.c_str(), winPath, &pathLen, 0);

        std::filesystem::path fsPath = SUCCEEDED(hr) ? std::filesystem::path(winPath) : std::filesystem::path(uri.path());
        m_internal = std::unique_ptr<SyncExternalService<FileClient>>
            (new SyncExternalService<FileClient>(new FileClient(fsPath)));
#else
        m_internal = std::unique_ptr<SyncExternalService<FileClient>>
            (new SyncExternalService<FileClient>(new FileClient(uri.path())));
#endif
    } else {
        throw std::invalid_argument("SyncExternalFacade currently supports file scheme only.");
    }
}

static std::shared_ptr<Payload>
base64_gzip_pauyload(const std::string& collection, GZip& gzip, const std::string& buffer, Error& e)
{
    if (e) {
        return std::shared_ptr<Payload>(new ErrorPayload(e));
    }
    SyncPayloadBSDO bsdo = rfl::json::read<SyncPayloadBSDO>(buffer).value();
    bsdo.payload = gzip.decompress(orion::base64_decode(bsdo.payload), e);
    if (e) {
        return std::shared_ptr<Payload>(new ErrorPayload(e));
    }
    return Payload::server_instance(bsdo, collection);
}

std::shared_ptr<Payload>
SyncExternalFacade::read(const std::string& collection)
{
    //the expect payload type here is json, but let's just read next bsdo in the file and initialise
    m_buffer.clear();
    Error e = m_internal->read(collection, m_buffer);
    if (m_buffer.empty()) {
        return std::shared_ptr<Payload>(new ErrorPayload(e));
    }
    return base64_gzip_pauyload(collection, m_gzip, m_buffer, e);
}

Error
SyncExternalFacade::write(const Payload& payload)
{
    Error e;
    SyncPayloadBSDO bsdo = payload.bsdo_object();
    bsdo.payload = orion::base64_encode(m_gzip.compress(bsdo.payload, e));
    return e ? e : m_internal->write(payload.collection(), rfl::json::write(bsdo));
}

std::shared_ptr<Payload>
SyncExternalFacade::exchange(const std::string& collection, const Payload& payload)
{
    //we do need to test the exchange support first by calling the client exchange method with an empty collection
    m_buffer.clear();
    Error e = m_internal->exchange(collection, m_buffer); //m_buffer is in out here
    if (e) {
        return std::shared_ptr<Payload>(new ErrorPayload(e));
    }
    SyncPayloadBSDO bsdo = payload.bsdo_object();
    bsdo.payload = orion::base64_encode(m_gzip.compress(bsdo.payload, e));
    if (e) {
        return std::shared_ptr<Payload>(new ErrorPayload(e));
    }
    m_buffer = rfl::json::write(bsdo);
    e = m_internal->exchange(collection, m_buffer);
    return base64_gzip_pauyload(collection, m_gzip, m_buffer, e);
}

Error
SyncExternalFacade::rollback()
{
    return m_internal->rollback();
}

void
SyncExternalFacade::cleanup()
{
    m_internal->cleanup();
}

} //namespace orion::sync