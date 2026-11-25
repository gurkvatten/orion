#include <src/sync_api.h>
#include <src/internal/workers.h>
#include <../common/src/error.h>
#include <../common/src/conversion_util.h>
#include <../common/src/csv_util.h>
#include <../common/src/time_util.h>
#include <thread>
#include <condition_variable>
#include  <cassert>
#include <format>
#include <chrono>

namespace orion::sync::tests {

static std::mutex m;
static std::condition_variable cv;

void
payload_test()
{
    SyncPayloadBSDO bsdo = {"-F_Szdjg3GzX", 1388635807, 140, "\"field 1\",\"field2\"\n11,value 21", 1};
    BookmarkPayload csv(bsdo);
    assert(!csv.error() && csv.payload() == bsdo.payload);

    csv.add_row("added value 12,-2.2");
    std::string csv_payload = csv.payload();
    std::string csv_serialized = csv.serialize();
    std::string csv_collection = csv.collection();

    std::shared_ptr<Payload> deserializedCSV = Payload::instance(csv_serialized);
    assert(deserializedCSV != nullptr && !deserializedCSV->error() && deserializedCSV->payload() == csv_payload);

    std::shared_ptr<Payload> json = csv.to_json_payload();
    assert(json != nullptr && !json->error());

    std::string json_payload = json->payload();
    std::string json_serialized = json->serialize();
    std::string json_collection = json->collection();
    assert(json_collection == csv_collection);

    std::shared_ptr<Payload> deserializedJSON = Payload::instance(json_serialized);
    assert(deserializedJSON == nullptr);//JSONPayload can only ever be used as an object to be transfered to the external facade, 
                                        //hence it doesn't support deserialization back into a JSONPayload object, so this should return nullptr
    
    std::shared_ptr<Payload> csv_from_json = json->to_csv_payload();
    assert(csv_from_json != nullptr && !csv_from_json->error() && csv_from_json->collection() == csv_collection);                                    
}

void
gzip_test()
{
    GZip gzip;
    SyncPayloadBSDO bsdo = {"-F_Szdjg3GzX", 1388635807, 140, "\"field 1\",\"field2\"\nvalue 11,value 21", 1};
    BookmarkPayload csv(bsdo);
    assert(!csv.error() && csv.payload() == bsdo.payload);

    //make it so that gzip actually has an effect
    for (int i = 1; i <= 10; ++i) {
        csv.add_row(std::format(R"(added value 1{},added value 2{})", i, i));
    }
    std::string csv_payload = csv.payload();

    Error e;
    std::string gzipped = gzip.compress(csv_payload, e);
    assert(!e);
    std::string ungzipped = gzip.decompress(gzipped, e);
    assert(!e && ungzipped == csv_payload);
}

void
gzip_base64_test()
{
    GZip gzip;
    SyncPayloadBSDO bsdo = {"-F_Szdjg3GzX", 1388635807, 140, "\"field 1\",\"field2\"\nvalue 11,value 21", 1};
    BookmarkPayload csv(bsdo);
    assert(!csv.error() && csv.payload() == bsdo.payload);

    //make it so that gzip actually has an effect
    for (int i = 1; i <= 10; ++i) {
        csv.add_row(std::format(R"(added value 1{},added value 2{})", i, i));
    }
    std::string csv_payload = csv.payload();

    Error e;
    std::string gzipped = gzip.compress(csv_payload, e);
    assert(!e);

    std::string base64_encoded = orion::base64_encode(gzipped);
    std::string base64_decoded = orion::base64_decode(base64_encoded);
    assert(gzipped == base64_decoded);

    std::string ungzipped = gzip.decompress(base64_decoded, e);
    assert(!e && ungzipped == csv_payload);
}

void
external_facade_test()
{
    //this is more of an integration test to make sure the external facing API works as expected.
    std::unique_ptr<SyncExternalFacade> facade = SyncExternalFacade::instance(std::move(Uri::parse_strict("file:///home/mikin/mobiTelco/sync/")));

    SyncPayloadBSDO bsdo = {"-F_Szdjg3GzX", 1388635807, 140, "\"field 1\",\"field2\"\n11,value 21", 1};
    BookmarkPayload csv(bsdo);
    assert(!csv.error() && csv.payload() == bsdo.payload);

    std::shared_ptr<Payload> json = csv.to_json_payload();
    assert(!json->error());

    Error e = facade->write(*json);
    assert(!e);
    facade->cleanup(); //make sure the data is flushed to file

    std::shared_ptr<Payload> read_back = facade->read(csv.collection());
    assert(!read_back->error() && read_back->payload() == json->payload());
    facade->cleanup(); //make sure the data is flushed to file
}

//this test needs an existing sync staorage => e.g. run external_facade_test before it
void
external_facade_rollback_test()
{
    std::unique_ptr<SyncExternalFacade> facade = SyncExternalFacade::instance(std::move(Uri::parse_strict("file:///home/mikin/mobiTelco/sync/")));
    std::shared_ptr<Payload> read_existing = facade->read(Collection<BookmarkPayload>().name());
    assert(!read_existing->error());
    facade->cleanup();

    SyncPayloadBSDO bsdo = {"-F_Szdjg3GYz", 1388635807, 140, "\"field 1\",\"field2\"\nnew value 1_1,new value 2_1", 1};
    BookmarkPayload csv(bsdo);
    assert(!csv.error() && csv.payload() == bsdo.payload);

    std::shared_ptr<Payload> json = csv.to_json_payload();
    assert(!json->error());

    Error e = facade->write(*json);
    assert(!e);
    e = facade->rollback();
    assert(!e);
    facade->cleanup();

    std::shared_ptr<Payload> read_back = facade->read(csv.collection());
    assert(!read_back->error() && read_back->payload() == read_existing->payload());
    facade->cleanup();
}

static constexpr size_t max_records = 100;
static size_t o_count = 0;

void
export_test()
{
    std::shared_ptr<SyncApi> api = SyncApi::instance();

    SyncApi::ChannelHandle channel_handle = api->sync(Collection<void>(), Uri::parse_strict("file:///home/mikin/mobiTelco/sync")
            , []() -> std::shared_ptr<BookmarkPayload> {
                BookmarkPayload* csv;
                if (++o_count <= max_records) {
                    csv = new BookmarkPayload(SyncPayloadBSDO("-F_Szdjg3" + std::to_string(o_count), orion::tm::today_local(), 140, "\"field 1\",\"field 2\"\nvalue 1_0,value 2_0", 1));
                    csv->add_row(std::format(R"(added value 1_{},added value 2_{})", o_count, o_count));
                } else {
                    //stop the export
                    csv = new BookmarkPayload(SyncPayloadBSDO());
                }
                return std::shared_ptr<BookmarkPayload>(csv);
            }, nullptr
            , [syncApi = api->shared_from_this()](SyncId sync_id, Error e) -> bool {
            	if (e && sync_id != Failure) {
                	syncApi->cancel(sync_id);
                }
                return true; //stop the failing direction
            });
    channel_handle.wait();
    o_count = 0; //reset for next test
}

void
import_test()
{
    std::vector<std::string> csv;
    csv.reserve(max_records * 2 + 1); //each object has 2 csv rows (header + 1, see the export_test above)
    struct csvHolder {
        std::vector<std::string>& csv; //it can be referenced since we never leave this method unless the sync is finished. see channel_handle.wait(); below
    };
    csvHolder csv_holder(csv);

    std::shared_ptr<SyncApi> api = SyncApi::instance();

    SyncApi::ChannelHandle channel_handle = api->sync(Collection<BookmarkPayload>(), Uri::parse_strict("file:///home/mikin/mobiTelco/sync")
            , nullptr
            , [csv_holder](std::shared_ptr<Payload> in) -> Error {
                Error error = split_csv_to_rows(in->payload(), [csv_holder, in](const std::string& row, size_t row_nr) -> Error {
                    if (!row.empty()) {
                        if (!csv_holder.csv.empty() && row_nr == 0 && row != csv_holder.csv.at(0)) {
                            RAISE(ErrorCode::invalid_format, "The incoming object header of the collection %s must match the first object header", in->collection());
                        } else if (row_nr == 0) {//every row_nr 0 is header of new object => either ignore it of staore the header as the first row in the csv.
                            if (csv_holder.csv.empty()) {
                                csv_holder.csv.push_back(row);
                            }
                        } else {
                            csv_holder.csv.push_back(row);
                            o_count++;
                        }
                    }
                    SUCCEED();
                });
                return error;
            }
            , [syncApi = api->shared_from_this()](SyncId sync_id, Error e) -> bool {
            	if (e && sync_id != Failure) {
                	syncApi->cancel(sync_id);
                }
                return true; //stop the failing direction
            });
    channel_handle.wait();
    assert(csv.size() == max_records * 2 + 1); //+1 for header
    o_count = 0; //reset for next test
}

void
cancel_test()
{
    using namespace std::chrono_literals;

    std::shared_ptr<SyncApi> api = SyncApi::instance();

    SyncApi::ChannelHandle channel_handle = api->sync(Collection<void>(), Uri::parse_strict("file:///home/mikin/mobiTelco/sync")
            , []() -> std::shared_ptr<BookmarkPayload> {
                //error payload
                std::this_thread::sleep_for(1000ms);
                return std::shared_ptr<BookmarkPayload>(new BookmarkPayload(SyncPayloadBSDO("-F_Szdjg3_0", orion::tm::today_local(), 140, "\"field 1\",\"field 2",1 )));
            }, nullptr
            , [syncApi = api->shared_from_this()](SyncId sync_id, Error e) -> bool {
                //we should either cancel or return true to indicate we want to stop the failing direction 
                //but never both since then the cancel info might get lost since the synch ends before the cancel can take effect.
                //if this method just returns false, but doesn't cancel, then the sync will continue as if nothing happened.
            	if (e && sync_id != Failure) {
                	syncApi->cancel(sync_id);
                }
                return false;
            });
    channel_handle.wait();
}

}//orion::sync::test