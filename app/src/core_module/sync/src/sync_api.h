#pragma once
#include <src/sync_types.h>
#include <../common/src/uri.h>
#include <string>
#include <memory>
#include <thread>
//#include <network> unfortunately not yet available in c++20. Not even #include <experimental/network> seems to be avaiable, need to move to C++ 23 :)

namespace orion::sync {

class SyncApi : public std::enable_shared_from_this<SyncApi> {

public:
    class ChannelHandle {
    //SyncChannel instance coul be ,in theary, a part of this class and we could drop the  SyncId => SyncChannel map
    //however it would expose the internal instane outside to user space making it up to the user to mainain the internal structur lifetime
    //=> not good.
        friend SyncApi;
        public:
            ChannelHandle() = default;

            bool cancel() const;
            bool wait() const;
            bool is_syncing() const;
            bool valid() const { return m_sync_id != Failure; }
            SyncId id() const { return m_sync_id; }
            ChannelHandle& operator = (const ChannelHandle&) = default;
            ChannelHandle& operator = (ChannelHandle&&) = delete;

        private:
            ChannelHandle(ChannelHandle&&) = default;
            ChannelHandle(ChannelHandle&) = default;

            ChannelHandle(SyncId sync_id, std::shared_ptr<std::thread> main_thread) : m_sync_id(sync_id), m_main_thread(main_thread) {}
            ChannelHandle(SyncId sync_id) : m_sync_id(sync_id) {}
        
        private:
            SyncId m_sync_id = Failure;
            std::shared_ptr<std::thread> m_main_thread;
    };

public:
    static std::shared_ptr<SyncApi>
    instance()
    {
        static std::shared_ptr<SyncApi> instance = std::shared_ptr<SyncApi>(new SyncApi());
        return instance;
    };

    //ideally this method should return immediately after it has opened the pipe so that it doesn't block the current thread.
    //supplying only supplier means export, only consumer means import, both means sync (2 way)
    template<typename P>
    const ChannelHandle
    sync(Collection<P> collection, const Uri&& uri, Supplier supplier, Consumer consumer, FinishedErrorCb finished_cb, AccessTokenCb access_token_cb = nullptr)
    {
        return sync_setup(collection.name(), std::forward<const Uri>(uri), supplier, consumer, finished_cb, access_token_cb);
    }
    bool cancel(SyncId sync_id); //false if the SyncId doesn't refere to any valid sync in progress, it blocks utill the sync has been cancelled
    bool is_syncing(SyncId sync_id) const; //false if the SyncId doesn't refere to any valid sync in progress

    ~SyncApi();

private:
    ChannelHandle sync_setup(const std::string& collection, const Uri&& uri, Supplier supplier, const Consumer consumer, FinishedErrorCb finished_cb
        , AccessTokenCb access_token_cb);
    SyncApi();
};

} //namespace orion::sync
