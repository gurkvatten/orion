#include <src/net_http_client.h>
#include <../common/src/conversion_util.h>
#include <../common/src/crypto.h>
#include <string>
#include <iomanip>
#include <cctype>
#include <chrono>


namespace orion::net {
struct HawkCredential {
    std::string id;
    std::string key;
    std::string algorithm = "sha256"; // sha1 or sha256
    std::string ext;
    std::string payload_hash;
    std::string timestamp;
    std::string nonce;
    std::string mac;
};

static std::string
bytes_to_hex(const uint8_t* data, size_t len)
{
    std::ostringstream ss;
    ss << std::hex << std::setfill('0');
    for (size_t i = 0; i < len; ++i) {
        ss << std::setw(2) << int(data[i]);
    }
    return ss.str();
}

static std::string
compute_payload_hash(
    const std::string& payload,
    const std::string& algorithm)
{
    if (algorithm == "sha256") {
        uint8_t hash[32];
        orion::crypto::sha256(reinterpret_cast<const uint8_t*>(payload.data()), payload.size(), hash);
        return base64_encode(std::string(reinterpret_cast<const char*>(hash), 32));
    } else if (algorithm == "sha1") {
        uint8_t hash[20];
        orion::crypto::sha1(reinterpret_cast<const uint8_t*>(payload.data()), payload.size(), hash);
        return base64_encode(std::string(reinterpret_cast<const char*>(hash), 20));
    }
    return "";
}

static std::string
parse_hawk_field(
    const std::string& header,
    const std::string& field_name)
{
    const std::string search = field_name + "=\"";
    size_t pos = header.find(search);
    if (pos == std::string::npos) {
        return "";
    }
    pos += search.size();
    size_t end = header.find('"', pos);
    if (end == std::string::npos) {
        return "";
    }
    return header.substr(pos, end - pos);
}

static HawkCredential
parse_hawk_authorization_header(const std::string& header)
{
    HawkCredential cred;
    if (header.find("Hawk") == std::string::npos) {
        return cred;
    }

    cred.id = parse_hawk_field(header, "id");
    cred.key = parse_hawk_field(header, "key");
    cred.algorithm = parse_hawk_field(header, "algorithm");
    if (cred.algorithm.empty()) {
        cred.algorithm = "sha256";
    }
    cred.ext = parse_hawk_field(header, "ext");
    cred.payload_hash = parse_hawk_field(header, "hash");
    cred.timestamp = parse_hawk_field(header, "ts");
    cred.nonce = parse_hawk_field(header, "nonce");
    cred.mac = parse_hawk_field(header, "mac");

    return cred;
}

static std::string
make_hawk_signature(
    const HawkCredential& cred,
    const std::string& method,
    const std::string& uri,
    const std::string& host,
    int port)
{
    std::ostringstream auth_string;
    auth_string << "hawk.1.header\n";
    auth_string << cred.timestamp << "\n";
    auth_string << cred.nonce << "\n";
    auth_string << method << "\n";
    auth_string << uri << "\n";
    auth_string << host << "\n";
    auth_string << port << "\n";
    if (!cred.payload_hash.empty()) {
        auth_string << cred.payload_hash << "\n";
    }
    if (!cred.ext.empty()) {
        auth_string << cred.ext << "\n";
    }

    const std::string normalized_string = auth_string.str();
    const auto key_bytes = reinterpret_cast<const uint8_t*>(cred.key.data());

    if (cred.algorithm == "sha256") {
        uint8_t mac[32];
        orion::crypto::hmac_sha256(key_bytes, cred.key.size(),
            reinterpret_cast<const uint8_t*>(normalized_string.data()), normalized_string.size(), mac);
        return base64_encode(std::string(reinterpret_cast<const char*>(mac), 32));
    } else if (cred.algorithm == "sha1") {
        uint8_t mac[20];
        orion::crypto::hmac_sha1(key_bytes, cred.key.size(),
            reinterpret_cast<const uint8_t*>(normalized_string.data()), normalized_string.size(), mac);
        return base64_encode(std::string(reinterpret_cast<const char*>(mac), 20));
    }

    return "";
}

static bool
validate_hawk_authorization(
    const HawkCredential& cred,
    const std::string& method,
    const std::string& uri,
    const std::string& host,
    int port)
{
    if (cred.id.empty() || cred.key.empty() || cred.mac.empty()) {
        return false;
    }

    const std::string computed_mac = make_hawk_signature(cred, method, uri, host, port);
    return computed_mac == cred.mac;
}

void validate_incoming_hawk_request(
    const std::string& auth_header,
    const std::string& method,
    const std::string& uri,
    const std::string& payload,
    const std::string& host,
    int port)
{
    using namespace orion::net;

    // Parse the incoming HAWK authorization header
    HawkCredential cred = parse_hawk_authorization_header(auth_header);

    if (cred.id.empty()) {
        // Invalid HAWK token
        return;
    }

    // If request has a body, compute its hash
    if (!payload.empty()) {
        cred.payload_hash = compute_payload_hash(payload, cred.algorithm);
    }

    // Validate the signature
    if (validate_hawk_authorization(cred, method, uri, host, port)) {
        // Valid HAWK request - process it
    } else {
        // Invalid signature - reject request
    }
}

void create_hawk_signed_request(
    const std::string& client_id,
    const std::string& client_key,
    const std::string& method,
    const std::string& uri,
    const std::string& host,
    int port,
    const std::string& payload = {})
{
    using namespace orion::net;

    HawkCredential cred;
    cred.id = client_id;
    cred.key = client_key;
    cred.algorithm = "sha256";
    cred.timestamp = std::to_string(std::chrono::system_clock::now().time_since_epoch().count() / 1000);
    cred.nonce = "nonce123"; // In practice, generate random nonce

    // Compute payload hash if request has body
    if (!payload.empty()) {
        cred.payload_hash = compute_payload_hash(payload, cred.algorithm);
    }

    // Generate the MAC
    cred.mac = make_hawk_signature(cred, method, uri, host, port);

    // Build Authorization header
    std::ostringstream auth;
    auth << "Hawk id=\"" << cred.id << "\", ts=\"" << cred.timestamp
         << "\", nonce=\"" << cred.nonce << "\", mac=\"" << cred.mac << "\"";
    if (!cred.payload_hash.empty()) {
        auth << ", hash=\"" << cred.payload_hash << "\"";
    }

    // Use auth.str() as Authorization header value
    std::string auth_header = auth.str();
}
}
