#pragma once
#include <src/sync_types.h>
#include <../common/src/uri.h>
#include <../common/src/zlib_util.h>
#include <src/internal/thread_holder.h>
#include <memory>

namespace orion::sync {

constexpr int8_t supplier_run = 1;
constexpr int8_t consumer_run = 2;

class Worker : public ThreadRunner {
public:
    void link_sync_id(SyncId sync_id) {this->m_sync_id = sync_id;};

protected:
    Worker(FinishedErrorCb finished_cb, int fd) :
        m_finished_cb(finished_cb), m_fd(fd), m_running(0) {}
    virtual ~Worker() {cleanup();};

    virtual bool start();
    virtual void rollback(const Error& reason) {
        m_finished_cb(m_sync_id, reason);
    }
    virtual void cleanup();

protected:
    bool verify(const Error& error, int8_t directionFlags);
    bool running_exchange() {return m_running == (consumer_run|supplier_run);}

protected:
    FinishedErrorCb m_finished_cb;
    int m_fd;
    std::string m_message;
    int8_t m_running;
    SyncId m_sync_id;
};

class ClientWorker : public Worker {
public:
    ClientWorker(Supplier supplier, Consumer consumer, FinishedErrorCb finished_cb, int fd) :
        Worker(finished_cb, fd)
        ,m_supplier(supplier), m_consumer(consumer) {
        if (m_supplier != nullptr) {
            m_running |= supplier_run;
        }
        if (m_consumer != nullptr) {
            m_running |= consumer_run;
        }
    };
    ~ClientWorker() = default;

    virtual bool run();
    virtual void rollback(const Error& reason) {/*noop*/};

private:
    void run_import();
    void run_export();

private:
    Supplier m_supplier;
    Consumer m_consumer;
};

struct SyncExternalService_Interface {
    virtual Error read(const std::string& collection, std::string& buffer) = 0; 
    virtual Error write(const std::string& collection, const std::string& buffer) = 0;
    virtual Error exchange(const std::string& collection, std::string& buffer) = 0;
    virtual Error rollback() = 0;
    virtual void cleanup() = 0;
}; //phoney internal base just to make the unique ptr happy

//facade over the real service implementation given by the supplied uri
class SyncExternalFacade {
public:
    static std::unique_ptr<SyncExternalFacade> instance(const Uri&& uri);

    std::shared_ptr<Payload> read(const std::string& collection);
    Error write(const Payload& payload); //the collection is given by the payload
    std::shared_ptr<Payload> exchange(const std::string& collection, const Payload& payload);

    Error rollback();
    void cleanup();

    ~SyncExternalFacade() {
        cleanup();
    }
private:
    SyncExternalFacade(const Uri&& uri);
    
private:
    const Uri m_uri;
    std::unique_ptr<SyncExternalService_Interface> m_internal;
    orion::GZip m_gzip;
    std::string m_buffer;
};

class ServerWorker : public Worker {
public:
    ServerWorker(const Uri&& uri, const std::string& collection, bool isExport, FinishedErrorCb finished_cb, int fd) :
        Worker(finished_cb, fd)
        ,m_collection(collection)
        ,m_syncService(SyncExternalFacade::instance(std::forward<const Uri>(uri))) {
            if (!collection.empty()) {
                m_running |= consumer_run;
            }
            if (isExport) {
                m_running |= supplier_run;
            }            
        };
    ~ServerWorker() {
        cleanup();
    };

    virtual bool run();
    virtual void cleanup();
    virtual void rollback(const Error& reason);

private:
    void run_import();
    void run_export();
    void run_exchange();

    std::shared_ptr<Payload> read_client();
    void write_client(const std::shared_ptr<Payload>& server_pyload);

private:
    std::string m_collection;
    std::unique_ptr<SyncExternalFacade> m_syncService;
};

}