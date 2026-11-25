#include <src/sync_types.h>
#include <src/internal/workers.h>
#include <src/internal/error_payload.h>
#include <zip.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef int ssize_t;
static inline ssize_t fd_read(int fd, void* buf, unsigned int count) {
    return ::recv(fd, static_cast<char*>(buf), count, 0);
}
static inline ssize_t fd_write(int fd, const void* buf, unsigned int count) {
    return ::send(fd, static_cast<const char*>(buf), count, 0);
}
#else
#include <sys/socket.h>
#include <signal.h>
#include <unistd.h>
static inline ssize_t fd_read(int fd, void* buf, size_t count) { return ::read(fd, buf, count); }
static inline ssize_t fd_write(int fd, const void* buf, size_t count) { return ::write(fd, buf, count); }
#endif
#include <memory.h>

namespace orion::sync {

//this is a global solution supposedly working on any OS supporting POSIX.  We could also set a signal handle but athat's a bit akward for handling error in code.
//signal(SIGPIPE, SIG_IGN);
//there'd be another ,per call and per socket, options but these are not guarantied to be supported on all OSes.
/*
flag on send() call the MSG_NOSIGNAL option, which will turn the SIGPIPE behavior off on a per call basis.
but not all operating systems support the MSG_NOSIGNAL flag.

the SO_NOSIGPIPE socket flag that can be set with setsockopt()
on some operating systems (e.g. those derived from BSD). This will prevent SIGPIPE from being caused by writes just to the sockets it is set on.
static void
ignore_sigpipe()
{
    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = SIG_IGN;
    act.sa_flags = SA_INTERRUPT;//SA_RESTART;
    int r = sigaction(SIGPIPE, &act, NULL);
    if (r)
        signal(SIGPIPE, SIG_IGN);
}
*/

static Error
write_payload(int fd, const Payload& out)
{
    const std::string message = out.serialize();
    if (out.error()) {
        return out.error();
    }
    uint32_t len = message.size();
#ifndef _WIN32
    signal(SIGPIPE, SIG_IGN);
#endif
    if (fd_write(fd, &len, sizeof(len)) < 0) {
        return MAKE_ERROR(ErrorCode::io, "Synch  write payload length failed");
    }

    // Write message data
    if (fd_write(fd, message.c_str(), len) < 0) {
        return MAKE_ERROR(ErrorCode::io, "Synch  write payload data failed");
    }
    SUCCEED();
}

static Error
read_payload(int fd, std::string& buffer)
{
    uint32_t len = 0;
    ssize_t bytes_read = fd_read(fd, &len, sizeof(len));
    if (bytes_read < 0) {
        RAISE(ErrorCode::io, "Synch  read payload length failed");
    }
    
    if (bytes_read == 0 || len == 0) {
        buffer.clear();
        SUCCEED();
    }
    //check for an unrealistic message len?
    if (len > 128 * 1024) { // Example unrealistic size check
        RAISE(ErrorCode::io, "Synch  read payload length failed, unrealistic, possibly negative, size: %u", len);
    }

    // Read payload data
    buffer.resize(len);
    ssize_t data_read = fd_read(fd, &buffer[0], len);
    
    if (data_read < 0) {
        RAISE(ErrorCode::io, "Synch read payload data failed");
    } else if (data_read != static_cast<ssize_t>(len)) {
        RAISE(ErrorCode::io, "Synch read payload data failed, Expected %lu chars, got %lu chars.", static_cast<unsigned long>(len), static_cast<unsigned long>(data_read));
    }
    SUCCEED();
}

void
ClientWorker::run_import()
{
    if ((m_running & consumer_run) != 0) {
        Error e = read_payload(m_fd, m_message);
        if (verify(e, consumer_run) && m_message.length() > 0) {
            const std::shared_ptr<Payload> out = Payload::instance(m_message);
            if (out == nullptr) {
                verify(MAKE_ERROR(ErrorCode::not_found, "unknown object type on client input: %s"
                                , m_message.c_str())
                        , consumer_run);
            } else if (verify(out->error(), consumer_run)) {
                verify(m_consumer(out), consumer_run);
            }
        } else {
            m_running &= ~consumer_run;
        }
    }
}

void
ClientWorker::run_export()
{
    if ((m_running & supplier_run) != 0) {
        std::shared_ptr<Payload> in = m_supplier();//should this return shared_ptr?
        if (in && verify(in->error(), supplier_run) && !in->empty()) {
            Error e = write_payload(m_fd, *in);
            verify(e, supplier_run);
        } else {
            m_running &= ~supplier_run;
        }
    }
}

bool
ClientWorker::run()
{
    run_import();
    run_export();
    return m_running & (consumer_run|supplier_run);
}

std::shared_ptr<Payload>
ServerWorker::read_client()
{
    Error e = read_payload(m_fd, m_message);
    if (!verify(e, m_running) || m_message.empty()) {
        m_running &= ~m_running;
    } else if (m_message.empty()) {
        if (!running_exchange()) {
            m_running &= ~m_running;
        }
    } else {
        std::shared_ptr<Payload> in = Payload::instance(m_message);
        if (in == nullptr) {
            verify((e = MAKE_ERROR(ErrorCode::not_found, "unknown object type on server input: %s"
                            , m_message.c_str()))
                    , supplier_run);
        } else if (verify((e = in->error()), m_running)) {
            return in->to_json_payload();
        }
    }
    return std::shared_ptr<Payload>(new ErrorPayload(e));
}

void
ServerWorker::run_export()
{
    if ((m_running & supplier_run) != 0 && !running_exchange()) {
        //doing export
        std::shared_ptr<Payload> jsonPayload = read_client();
        if (verify(jsonPayload->error(), supplier_run) && !jsonPayload->empty()) {
            //send to SyncExternalFacade
            Error e = m_syncService->write(*jsonPayload);
            verify(e, supplier_run);
        } else if (!running_exchange()) {
            m_running &= ~supplier_run;
        }
    }
}

void
ServerWorker::write_client(const std::shared_ptr<Payload>& server_pyload)
{
    if (verify(server_pyload->error(), m_running)) {
        if (server_pyload->empty() && !running_exchange()) {
            m_running &= ~m_running;
        } else {
            std::shared_ptr<Payload> csv = server_pyload->to_csv_payload();
            if (verify(csv->error(), m_running)) { 
                verify(write_payload(m_fd, *csv), m_running);
            }
        }
    }
}

void
ServerWorker::run_import()
{
    if ((m_running & consumer_run) != 0 && !running_exchange()) {
        //doing import
        std::shared_ptr<Payload> in = m_syncService->read(m_collection);
        write_client(in);
    }
}

void
ServerWorker::run_exchange()
{
    if (running_exchange()) {
        //running full sync
        std::shared_ptr<Payload> jsonClient = read_client();
        if (verify(jsonClient->error(), m_running)) {
            std::shared_ptr<Payload> jsonServer = m_syncService->exchange(m_collection, *jsonClient);
            write_client(jsonServer);
            if (jsonClient->empty() && jsonServer->empty()) {
                m_running &= ~consumer_run|supplier_run;
            }
        }
    }
}

bool
ServerWorker::run()
{
    run_import();
    run_export();
    run_exchange();
    return m_running & (consumer_run|supplier_run);
}

} //namespace orion::sync