#include <rfl/json.hpp>
#include <src/sync_types.h>
#include <src/internal/error_payload.h>
#include <../common/src/csv_util.h>
#include <../common/src/json_util.h>
#include <../common/src/conversion_util.h>
#include <array>
#include <regex>
#include <numeric>
#include <stdexcept>

namespace orion::sync {

constexpr static std::string_view payload_postfix("payload");
// Multi-word collection names (e.g. "readinglist", "bookmarkfolder")
// cannot be safely reconstructed by just uppercasing the first character.
// Keep explicit known payload type names and resolve the reverse mapping from them.
#define SYNC_COLLECTION_TYPE_NAME(name) std::string_view(#name "Payload"),
static constexpr auto known_collection_type_names = std::to_array<std::string_view>({
    FOR_EACH_SYNC_COLLECTION(SYNC_COLLECTION_TYPE_NAME)
});
#undef SYNC_COLLECTION_TYPE_NAME

std::string
CollectionInfo::collection_from(std::string name)
{
    name = TypeInfo::strip_ns(name);
    std::transform(name.begin(), name.end(), name.begin(), ::tolower);
    size_t pos = name.rfind(payload_postfix);
    if (pos != std::string::npos) {
        name.erase(pos, payload_postfix.length());
    }
    return name;
}

std::string
CollectionInfo::ns_less_type_from_collection(const std::string& collection)
{
    for (const auto type_name : known_collection_type_names) {
        if (collection_from(std::string(type_name)) == collection) {
            return std::string(type_name);
        }
    }

    // fallback for unknown collections
    if (collection.empty()) {
        return std::string();
    }
    std::string c;
    c += (collection[0] + ('A' - 'a'));
    c += collection.substr(1);
    c += (payload_postfix[0] + ('A' - 'a'));
    c += payload_postfix.substr(1);
    return c;
}

//not sure if it's even nec essary to define the exact structures. Maybe it's better for sync to treat the payload
//as an opaque object that's defined by BDSO only. The BDSO would be constructed by the sync itself 

//the bellow could've been one reqex but maube it's better to separated it.
static std::regex number(R"(^[+-]?(\d+(?:\.\d+)?|\.?\d+)$)"); //matches integers and decimals, with optional leading + or - sign
//, allows leading dot in the number for cases like .5 or -.5
static std::regex boolean(R"(^(true|false|yes|no)$)");

class JSONPayload : public Payload {
public:
    JSONPayload() = delete;
    //json payload collection isn't given by it's subclasses hence it's passed on here
    JSONPayload(const SyncPayloadBSDO& bsdo, const std::string& collection) :
        Payload(bsdo), m_collection(collection) {deserialize(bsdo.payload);}

    ~JSONPayload() = default;

    virtual const std::string payload() const { return m_json_data; }

    virtual const std::string collection() const { return m_collection; }
    
    virtual void
    deserialize(const std::string& data)
    {
        m_error = Error::ok();
        m_json_data.clear();
        if (data.empty()) {
            //valid case
            m_bsdo.payload = payload();
            return;
        }
        if (data.at(0) == '{') {
            m_json_data = data;
        }
        else if (data.at(0) == '[') {
            //assuming json data is directly given. Potential json schema validation will happen later.
            m_json_data = data;
        } else {//assuming csv lines.
            //expecting and array of objects
            m_columns_count = 0;
            m_error = split_csv_to_rows(data, [this](const std::string& row) {
                if (!row.empty()) {
                    construct_json(row);
                }
            });
            if (!m_error && m_json_data.size() <= 2) { //we should have at least an array with empty object if we have any data
                m_error = MAKE_ERROR(ErrorCode::invalid_format, "CSV data produced an invalid JSON");
            }
            if (m_error) {
                m_json_data.clear();
            } else {
                //all ok -> close the array
                m_json_data.back() = ']'; //replace the last comma with closing of 2D array
            }                     
        }
        m_bsdo.payload = payload();
    }

private:

    void
    construct_json(const std::string& csv_row)
    {
        //construct json object based on header and row values, we can use a simple map for that
        //it's a naive json from csv whwreby we only ever deduce numbers and booleans but all other values are strings
        //no nested object as this is csv -> json whereby json is represented as 2D array.

        int len = m_json_data.length();
        m_json_data += '['; //open new array row
        if (len == 0) {
            m_json_data += '['; //starting the 2D array
        }
        m_error = parse_csv(csv_row, ',', [this, len](const std::vector<std::string>& row) {
            if (len == 0 || row.size() == m_columns_count) {
                for (size_t i = 0; i < row.size(); ++i) {
                    m_json_data += (std::regex_match(row[i], number) || std::regex_match(row[i], boolean)
                            ? row[i] + "," 
                            : (row[i] == "null" ? "null," : escape_json_field(row[i]) + ","));
                    if (len == 0) {
                        m_columns_count++;
                    }
                }
            }
        });
        //check if we've got an object data
        if (m_json_data.at(m_json_data.length() - 1) == '[') {
            //no -> shrink back to before adding the object since we haven't added any data
            m_json_data.erase(len, m_json_data.length() - len); 
        } else {
            //remove the last comma and close the object
            m_json_data.back() = ']'; //replace the last comma with closing array for header
            m_json_data += ","; //add comma => expecting more rows to come.
        }
        if (m_columns_count == 0) {
            m_error = MAKE_ERROR(ErrorCode::invalid_format, "CSV header row is empty");
        } else if (csv_row.size() != m_columns_count) {
            m_error = MAKE_ERROR(ErrorCode::invalid_format, "CSV row has different number of fields than header");
        }
    }

private:
    std::string m_collection;
    std::string m_json_data;
    size_t m_columns_count = 0;
};

const std::string
CSVPayload::payload() const
{
    return m_csv_header + std::accumulate(m_csv_rows.begin(), m_csv_rows.end(), std::string(""), [](const std::string& a, const std::string& b) {
        return a + "\n" + b;
    });
}

void
CSVPayload::deserialize(const std::string& data)
{
    m_error = Error::ok();
    m_csv_rows.clear();
    m_csv_header.clear();
    if (data.empty()) {
        //valid case
        m_bsdo.payload = payload();
        return;
    }
    if (data.at(0) == '{') {
        return;
    }
    else if (data.at(0) == '[') {
        //json string to csv.
        m_error = json_array_to_csv(data, ',', [this](const std::string& row) {
            if (m_csv_header.empty()) {
                m_csv_header = row;
            } else {
                m_csv_rows.push_back(row);
            }
        });
    } else {
        m_error = split_csv_to_rows(data, [this](const std::string& row) {
            if (!row.empty()) {
                if (m_csv_header.empty()) {
                    m_csv_header = row;
                } else {
                    m_csv_rows.push_back(row);
                }
            }
        });
    }
    if (!m_error && (m_csv_header.empty() || m_csv_rows.empty())) {
        m_error = MAKE_ERROR(ErrorCode::invalid_format, "CSV data must contain at least a header and one row");
    }
    if (m_error) {
        m_csv_rows.clear();
        m_csv_header.clear();
    }
    m_bsdo.payload = payload();
}

/**
typedef std::map<std::string_view, create_instance_fn> map_type;

static const map_type the_map = {
    { std::string_view(typeid(CSVPayload).name()), &create_instance<CSVPayload> },
    { std::string_view(typeid(JSONPayload).name()), &create_instance<JSONPayload> },
    //add more payload types here as needed
};
static const map_type&
get_map()
{
    return the_map;
}
**/

template <typename Key, typename Value, std::size_t size>
struct Map {
    std::array<std::pair<Key, Value>, size> data;

    [[nodiscard]] constexpr Value
    at(const Key &key) const
    {
        const auto itr = std::find_if(begin(data), end(data), [&key](const auto &v) { return v.first == key; });
        if (itr != end(data)) {
            return itr->second;
        }
        return nullptr;
    }
};

template<typename T>
static Payload* create_instance(const SyncPayloadBSDO& bsdo) { return new T(bsdo); }
using create_instance_fn = Payload*(*)(const SyncPayloadBSDO&);
#define REGISTER_COLLECTION(name) std::pair<std::string_view, create_instance_fn>{#name "Payload", &create_instance<name##Payload>},

static constexpr auto c_register = std::to_array<std::pair<std::string_view, create_instance_fn>>({
    REGISTER_COLLECTION(CSV)
    FOR_EACH_SYNC_COLLECTION(REGISTER_COLLECTION)
});

#undef REGISTER_COLLECTION

static constexpr auto collection_types_map = Map<std::string_view, create_instance_fn, c_register.size()>{{c_register}};

std::shared_ptr<Payload>
Payload::instance(const std::string& type_name, const SyncPayloadBSDO& bsdo)
{
    create_instance_fn creator = collection_types_map.at(type_name);
    if (creator) {
        //deserialize happens in constroctor
        return std::shared_ptr<Payload>(creator(bsdo));
    }
    return nullptr; //unknown type
}

std::shared_ptr<Payload>
Payload::instance(const std::string& serialized)
{
    //the first line of the data is the type name, we can use it to create the right payload type
    size_t pos = serialized.find('\n');
    if (pos == std::string::npos) {
        return nullptr; //invalid data
    }
    std::string type_name = serialized.substr(0, pos);
    std::string payload_bsdo = serialized.substr(pos + 1);
    SyncPayloadBSDO bsdo = rfl::json::read<SyncPayloadBSDO>(payload_bsdo).value();
    return instance(type_name, bsdo);
}

std::shared_ptr<Payload>
Payload::server_instance(const SyncPayloadBSDO& bsdo, const std::string& collection)
{
    return std::shared_ptr<Payload>(new JSONPayload(bsdo, collection));
}

const std::string
Payload::serialize() const
{
    return m_bsdo.id.empty() ? "" :
        std::string(TypeInfo::strip_ns(TypeInfo::type_name(typeid(*this).name()))) + '\n' + rfl::json::write(m_bsdo);
}

const std::string
Payload::collection() const
{
    std::string c(typeid(*this).name());
    return CollectionInfo::collection_from(TypeInfo::type_name(c));
}

const std::shared_ptr<Payload>
Payload::to_json_payload() const
{
    std::shared_ptr<JSONPayload> jsonPayload = std::shared_ptr<JSONPayload>(new JSONPayload(m_bsdo, collection()));
    return jsonPayload;
}

const std::shared_ptr<Payload>
Payload::to_csv_payload() const
{
    if (!empty() && !error()) {
        //make sure we give it the right class
        std::string serialized = serialize();
        std::string csv_type = CollectionInfo::ns_less_type_from_collection(collection());
        size_t pos = serialized.find('\n');
        if (pos == std::string::npos) {
            throw std::logic_error("Serialized string is invalid, missing type definition, string: " + serialized); //invalid data
        }
        //replace type info
        serialized = csv_type + serialized.substr(pos);
        std::shared_ptr<Payload> payload = Payload::instance(serialized);
        if (payload == nullptr) {
            throw std::logic_error("Collection " + collection() + " doesn't have assigned any known type. The collection type id: " + csv_type); //invalid data
        }
        return payload;
    }
    return std::shared_ptr<Payload>(new ErrorPayload(error()));
}

} // namespace orion::sync
