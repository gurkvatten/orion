#include <../common/src/error.h>
#include <../common/src/type_info.h>
#include <functional>
#include <type_traits>
#include <utility>

#pragma once

namespace orion::sync {

    /* rs-BSDO object header (https://mozilla-services.github.io/syncstorage-rs/syncstorage/api-1.5.html#basic-storage-object)

    The header is supplied by the synch API client any field, mandatory fields, that are not supplied will be calculated as follows:
        - id            -> class name + '_' + payload crc32 as string
        - modified      -> current UTC timestamp
        - sortindex     -> 100

*/
struct SyncPayloadBSDO {
    std::string id;             //"-F_Szdjg3GzX",
    int64_t modified;           //1388635807.41,
    int sortindex;              //140,
    std::string payload;        // the actual data, e.g. "{ \"this is\": \"an example\" }", format is depandent amd enforced by the Payload class
                                // when the object is finally stored/retrived from server/file/etc that it'll be, most likely,
                                // json | gzip | base64 on export and base64 -d | gzip -d | json on import
    int ttl;                    // 1    //number of secs to keep this record, not obvios if it's optional or not and what happens if not present
};

class Payload {
public:
    static std::shared_ptr<Payload> instance(const std::string& serialized);
    static std::shared_ptr<Payload> instance(const std::string& type_name, const SyncPayloadBSDO& bsdo);
    static std::shared_ptr<Payload> server_instance(const SyncPayloadBSDO& bsdo, const std::string& collection);

    virtual const std::string serialize() const;
    virtual void deserialize(const std::string& data) = 0;

    virtual const std::string payload() const = 0;
    const SyncPayloadBSDO& bsdo_object() const {return m_bsdo;}
    virtual const std::string collection() const;
    
    inline const Error& error() const { return m_error; }
    inline bool valid() const { return !m_error; }
    inline bool empty() const { return m_bsdo.id.empty(); }

    const std::shared_ptr<Payload> to_json_payload() const;
    const std::shared_ptr<Payload> to_csv_payload() const;

    virtual ~Payload() = default;

protected:
    Payload(const SyncPayloadBSDO& bsdo) :
        m_bsdo(std::move(bsdo))
        ,m_error(Error::ok())
    {};
    Payload(const Error& error) :
        m_error(error)
    {};
    Payload() = default;

protected:
    SyncPayloadBSDO m_bsdo;
    //last error after any payload operation
    Error m_error;
};

//whatever this object contains goes into one sync BSDO object.
class CSVPayload : public Payload {
public:
    CSVPayload() = default;
    CSVPayload(const SyncPayloadBSDO& bsdo) : Payload(bsdo) {deserialize(bsdo.payload);}
    virtual ~CSVPayload() = default;

    inline void add_row(std::string csv_row) {
        m_csv_rows.push_back(csv_row);
        m_bsdo.payload = payload();
    }
    virtual void deserialize(const std::string& data);
    virtual const std::string payload() const;

private:
    std::string m_csv_header;
    std::vector<std::string> m_csv_rows;
};

//payload classes to identify individual collections.
//this way the sync can create distinquished identifications for the different collections sunc as:
// bookmarks, history, etc ...
#define DEFINE_COLLECTION(name) \
class name##Payload : public CSVPayload {       \
public:                                         \
    explicit name##Payload(const SyncPayloadBSDO& bsdo) :\
        CSVPayload(bsdo) {};                    \
    template<typename... Args, typename = std::enable_if_t<(sizeof...(Args) > 0)>> \
    explicit name##Payload(Args&&... args) : \
        CSVPayload(SyncPayloadBSDO{std::forward<Args>(args)...}) {} \
    name##Payload() = default;                  \
    ~name##Payload() = default;                 \
};

// Keep collection names in one place and reuse this list for registration in sync_payload.cpp.
#define FOR_EACH_SYNC_COLLECTION(X) \
    X(Window) \
    X(Tab) \
    X(Setting) \
    X(History) \
    X(Password) \
    X(Bookmark) \
    X(BookmarkFolder) \
    X(ReadingList)

FOR_EACH_SYNC_COLLECTION(DEFINE_COLLECTION)

struct CollectionInfo : public TypeInfo{
    static std::string collection_from(std::string name);
    static std::string ns_less_type_from_collection(const std::string& collection);
};

template<typename P>
struct Collection {
    static std::string
    name()
    {
        return CollectionInfo::collection_from(TypeInfo::type_name(typeid(P).name()));
    };
};

template<>
struct Collection<void> {
    static std::string
    name()
    {
        return std::string();
    };
};

typedef long SyncId;
constexpr SyncId Failure = 0;
using Supplier = std::function<std::shared_ptr<Payload>(void)>; //either blocks or returs T or std:nullpter as end, no std::future<T> required as it runs i dedicated thread.
using Consumer = std::function<Error(std::shared_ptr<Payload>)>;
//FinishedErrorCb can be called with Error::ok in which case it indicated that the sync finished ok and the return value is discarded.
//if it's called with an error then it should either cancel or return true, to indicate we want to stop the failing sync direction, 
//but never both as then the cancel info might get lost since the sync might end before the cancel can take an effect.
//if this method just returns false, but doesn't cancel, then the sync will continue as if nothing happened.
using FinishedErrorCb = std::function<bool(SyncId, Error)>;
struct AccessToken {
    std::string token;
    int64_t expires_at; //UTC timestamp in seconds
};
//AccessTokenCb is called when the sync needs an access token to continue, it can be called multiple times if the token is expired or invalid.
//the callback should return a valid access token or an empty string if it can't provide one, in which case the sync will fail with an error.
//in case of an error FinishedErrorCb will be called. Error message contains the http status code.
using AccessTokenCb = std::function<AccessToken(SyncId, Error)>;

}
