#include <src/sync_api.h>
#include <src/internal/workers.h>
#include <src/internal/thread_holder.h>
#include <../common/src/defer.h>
#include <../common/src/thread_util.h>
#ifdef _WIN32
#define NOMINMAX
#include <winsock2.h>
#include <io.h>
static inline int fd_close(int fd) { return closesocket(fd); }
#else
#include <sys/socket.h>
#include <unistd.h>
static inline int fd_close(int fd) { return close(fd); }
#endif

#ifdef _WIN32
int socketpair(int af, int type, int prot, int fd[2]);
#endif

namespace orion::sync {
    //reflect-cpp and inheriotance with constructors:
    //  https://stackoverflow.com/questions/79630214/reflect-cpp-doesnt-work-with-anything-but-basic-structures

//we probably shouldn't delegate it to run loop disk/network thread since this can potentialy be a long running task
//and so we would block, what should just be a short responsive threads, it for everything else for potentially longer time.

static SyncId generate_sync_id() {
    static SyncId sync_id = Failure; //start from 1 since 0 is used to indicate failure to generate a sync id
    return ++sync_id == std::numeric_limits<SyncId>::max() ? 1/*see Failure*/ : sync_id;
}

struct SyncChannel {
    std::shared_ptr<ThreadHolder> m_client;
    std::shared_ptr<ThreadHolder> m_server;
    std::shared_ptr<std::thread> m_cancelling_thread;

    ~SyncChannel() {
        if (m_cancelling_thread) {
            m_cancelling_thread->detach();
        }
    }
};

std::mutex sync_pipes_m;
static auto sync_pipes = std::map<SyncId, SyncChannel>();

static void
sync_cleanup(int fd[2], SyncId sync_id)
{
    //the handles reffered to by fd[] might have been already closed.
    for (int i = 0; i < 2; i++) {
        fd_close(fd[i]);
    }
    std::lock_guard lk(sync_pipes_m);
    sync_pipes.erase(sync_id);
};

//this is the "main" sync thread that waits for the client and server workers to finish
static void
sync_channel_thread(SyncId sync_id, FinishedErrorCb finished_cb, int fd[2])
{
    orion::set_current_thread_name(R"(sync_channel)");
    SyncChannel syncChannel;
    defer([sync_id, fd]() {
        sync_cleanup(fd, sync_id);
    });
    {
        std::lock_guard lk(sync_pipes_m);
        //if this channel isn't found here the the code if self modifying :)
        syncChannel = sync_pipes.at(sync_id);
    }
    syncChannel.m_client->run();
    syncChannel.m_server->run();
    syncChannel.m_client->wait();
    syncChannel.m_server->wait();
    if (!syncChannel.m_client->lastError() && !syncChannel.m_server->lastError()) {
        finished_cb(sync_id, Error::ok());
    }
};

static std::thread*
sync_launch(SyncId sync_id, FinishedErrorCb finished_cb, int sv[2])
{
    if (sync_id != Failure) {
        //let it destroy itself upon the therad method finish plus let the caller have an option to join the main synch thread
        return new std::thread(sync_channel_thread, sync_id, finished_cb, sv);
    } else {
        sync_cleanup(sv, sync_id);
    }
    return nullptr; //failed to start
}

static int
set_blocking_timeout(int fd, int timeout_sec)
{
#ifdef _WIN32
    DWORD timeout_ms = timeout_sec * 1000;
    return setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO,
        reinterpret_cast<const char*>(&timeout_ms), sizeof(timeout_ms));
#else
    struct timeval t;
    t.tv_sec = timeout_sec;
    t.tv_usec = 0;
    return setsockopt(
        fd,     // Socket descriptor
        SOL_SOCKET, // To manipulate options at the sockets API level
        SO_RCVTIMEO,// Specify the receiving or sending timeouts
        reinterpret_cast<const char*>(&t), // option values
        sizeof(t)
    );
#endif
}

SyncApi::ChannelHandle
SyncApi::sync_setup(const std::string& collection, const Uri&& uri, Supplier supplier, const Consumer consumer, FinishedErrorCb finished_cb
    , AccessTokenCb access_token_cb)
{
    //1. sanity check
    int sv[2];
    if ((supplier == nullptr && consumer == nullptr) || finished_cb == nullptr) {
        if (finished_cb != nullptr) {
            finished_cb(Failure, MAKE_ERROR(ErrorCode::argument, "one of supplier or consumer must be set and finished_cb cannot be null"));
        }
        return ChannelHandle(Failure);
    }
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == -1) {
        finished_cb(Failure, MAKE_ERROR(ErrorCode::open, "unable to open sync pipe"));
        return ChannelHandle(Failure);
    }
    //set blocking timeout
    if (set_blocking_timeout(sv[0], 10) == -1) {//client side
        finished_cb(Failure, MAKE_ERROR(ErrorCode::io, "unable to set blocking timeout for client side"));
        return ChannelHandle(Failure);
    }
    if (set_blocking_timeout(sv[1], 30) == -1) {//server side
        finished_cb(Failure, MAKE_ERROR(ErrorCode::io, "unable to set blocking timeout for server side"));
        return ChannelHandle(Failure);
    }

    //2. start the pipes    
    bool isExport = supplier != nullptr;

    Worker *c, *s;
    SyncChannel syncChannel {
        .m_client = std::shared_ptr<ThreadRunnerHolder>(new ThreadRunnerHolder(
            (c = new ClientWorker(supplier, consumer, finished_cb, sv[0])))),
        .m_server = std::shared_ptr<ThreadRunnerHolder>(new ThreadRunnerHolder(
            (s = new ServerWorker(std::forward<const Uri>(uri), collection, isExport, finished_cb, sv[1])))),
        .m_cancelling_thread = nullptr
    };
    
    SyncId sync_id;
    {
        std::lock_guard lk(sync_pipes_m);
        sync_id = generate_sync_id();
        auto const result = sync_pipes.insert(std::make_pair(sync_id, std::move(syncChannel)));
        if (!result.second) {
           sync_id = Failure;
        }
    }
    if (sync_id == Failure) {
        //keys conflict
        finished_cb(Failure, MAKE_ERROR(ErrorCode::invalid_state, "Synch id %lu has already been registered in as a running sync task", sync_id));
        return ChannelHandle(Failure);
    };
    c->link_sync_id(sync_id);
    s->link_sync_id(sync_id);
    return SyncApi::ChannelHandle(sync_id, std::shared_ptr<std::thread>(sync_launch(sync_id, finished_cb, sv)));
};

bool
SyncApi::cancel(SyncId sync_id)
{
    std::lock_guard lk(sync_pipes_m);
    if (sync_pipes.contains(sync_id)) {
        
        SyncChannel& syncChannel = sync_pipes.at(sync_id);

        //this is here in case the cancel is called from either one of the bellow threads
        if (syncChannel.m_cancelling_thread == nullptr) {//otherwise this channel is lredy being cancelled
            syncChannel.m_cancelling_thread = std::shared_ptr<std::thread>(new std::thread([syncChannel]() {
                orion::set_current_thread_name(R"(sync_cancel)");
                syncChannel.m_client->cancel();
                syncChannel.m_server->cancel();
            }));
            //don't block here since it can be called from the very thread we'r trying to cancel.
            //ct.join();
            return true;
        }
    }
    return false;
}

bool SyncApi::is_syncing(SyncId sync_id) const
{
    std::lock_guard lk(sync_pipes_m);
    return sync_pipes.contains(sync_id);
}

SyncApi::SyncApi()
{
}

SyncApi::~SyncApi()
{
}

bool
SyncApi::ChannelHandle::cancel() const
{
    return SyncApi::instance()->cancel(m_sync_id);
}

bool
SyncApi::ChannelHandle::wait() const
{
    if (m_main_thread && m_main_thread->joinable()) {
        m_main_thread->join();
        return true;
    }
    return false;
}

bool SyncApi::ChannelHandle::is_syncing() const
{
    return SyncApi::instance()->is_syncing(m_sync_id);
}

} // namespace orion::sync
