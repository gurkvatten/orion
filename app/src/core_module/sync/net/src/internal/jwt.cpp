
#include <src/net_http_client.h>
#include <../common/src/uri.h>
#include <../common/src/conversion_util.h>
#include <../common/src/crypto.h>
#include <../common/src/string_util.h>
#include <../common/src/error.h>
#include <string>
#include <sstream>
#include <iomanip>
#include <cctype>
#include <vector>
#include <chrono>

#include <openssl/evp.h>
#include <openssl/pem.h>

namespace orion::net {

struct JwtToken {
    std::string header_json;      // Raw JSON header
    std::string payload_json;     // Raw JSON payload
    std::string signature_b64;    // Base64url-encoded signature
    std::string algorithm;        // "HS256", "HS512", "RS256", etc.
    std::string key_id;           // Optional "kid" field
};

struct JwtClaims {
    std::string iss;              // Issuer
    std::string sub;              // Subject
    std::string aud;              // Audience
    int64_t exp = 0;              // Expiration time (seconds since epoch)
    int64_t nbf = 0;              // Not before (seconds since epoch)
    int64_t iat = 0;              // Issued at (seconds since epoch)
    std::string jti;              // JWT ID
};

static JwtToken parse_jwt_token(const std::string& token)
{
    JwtToken jwt;
    
    // Split token into three parts by '.'
    std::vector<std::string> parts;
    std::string current;
    for (char c : token) {
        if (c == '.') {
            parts.push_back(current);
            current.clear();
        } else {
            current.push_back(c);
        }
    }
    if (!current.empty()) {
        parts.push_back(current);
    }
    
    if (parts.size() != 3) {
        return jwt; // Invalid token format
    }
    
    // Decode header, payload, and signature
    jwt.header_json = base64url_decode(parts[0]);
    jwt.payload_json = base64url_decode(parts[1]);
    jwt.signature_b64 = parts[2];
    
    // Extract algorithm from header JSON (simplistic parsing)
    size_t alg_pos = jwt.header_json.find("\"alg\"");
    if (alg_pos != std::string::npos) {
        size_t colon_pos = jwt.header_json.find(':', alg_pos);
        size_t quote1 = jwt.header_json.find('"', colon_pos);
        size_t quote2 = jwt.header_json.find('"', quote1 + 1);
        if (quote1 != std::string::npos && quote2 != std::string::npos) {
            jwt.algorithm = jwt.header_json.substr(quote1 + 1, quote2 - quote1 - 1);
        }
    }
    
    // Extract key ID from header JSON if present
    size_t kid_pos = jwt.header_json.find("\"kid\"");
    if (kid_pos != std::string::npos) {
        size_t colon_pos = jwt.header_json.find(':', kid_pos);
        size_t quote1 = jwt.header_json.find('"', colon_pos);
        size_t quote2 = jwt.header_json.find('"', quote1 + 1);
        if (quote1 != std::string::npos && quote2 != std::string::npos) {
            jwt.key_id = jwt.header_json.substr(quote1 + 1, quote2 - quote1 - 1);
        }
    }
    
    return jwt;
}

static std::string extract_json_field(const std::string& json, const std::string& field_name)
{
    const std::string search_key = "\"" + field_name + "\"";
    size_t pos = json.find(search_key);
    if (pos == std::string::npos) {
        return "";
    }
    
    pos = json.find(':', pos);
    if (pos == std::string::npos) {
        return "";
    }
    ++pos;
    
    // Skip whitespace
    while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) {
        ++pos;
    }
    
    if (pos >= json.size()) {
        return "";
    }
    
    // Handle string values (quoted)
    if (json[pos] == '"') {
        ++pos;
        size_t end = json.find('"', pos);
        if (end == std::string::npos) {
            return "";
        }
        return json.substr(pos, end - pos);
    }
    
    // Handle numeric values
    size_t end = pos;
    while (end < json.size() && (std::isdigit(static_cast<unsigned char>(json[end])) || json[end] == '-')) {
        ++end;
    }
    return json.substr(pos, end - pos);
}

static JwtClaims extract_jwt_claims(const std::string& payload_json)
{
    JwtClaims claims;
    
    claims.iss = extract_json_field(payload_json, "iss");
    claims.sub = extract_json_field(payload_json, "sub");
    claims.aud = extract_json_field(payload_json, "aud");
    claims.jti = extract_json_field(payload_json, "jti");
    
    const std::string exp_str = extract_json_field(payload_json, "exp");
    if (!exp_str.empty()) {
        claims.exp = std::stoll(exp_str);
    }
    
    const std::string nbf_str = extract_json_field(payload_json, "nbf");
    if (!nbf_str.empty()) {
        claims.nbf = std::stoll(nbf_str);
    }
    
    const std::string iat_str = extract_json_field(payload_json, "iat");
    if (!iat_str.empty()) {
        claims.iat = std::stoll(iat_str);
    }
    
    return claims;
}

static std::string compute_jwt_signature(
    const std::string& header_b64,
    const std::string& payload_b64,
    const std::string& secret,
    const std::string& algorithm)
{
    const std::string message = header_b64 + "." + payload_b64;
    
    if (algorithm == "HS256") {
        uint8_t mac[32];
        orion::crypto::hmac_sha256(
            reinterpret_cast<const uint8_t*>(secret.data()), secret.size(),
            reinterpret_cast<const uint8_t*>(message.data()), message.size(),
            mac);
        return base64url_encode(std::string(reinterpret_cast<const char*>(mac), 32));
    } else if (algorithm == "HS512") {
        // SHA512 not available in common/src/crypto.h, would need to add support
        return "";
    } else if (algorithm == "HS1") {
        uint8_t mac[20];
        orion::crypto::hmac_sha1(
            reinterpret_cast<const uint8_t*>(secret.data()), secret.size(),
            reinterpret_cast<const uint8_t*>(message.data()), message.size(),
            mac);
        return base64url_encode(std::string(reinterpret_cast<const char*>(mac), 20));
    }
    
    return "";
}

static bool verify_jwt_signature_rs256(
    const std::string& token,
    const std::string& public_key_pem)
{
    const JwtToken jwt = parse_jwt_token(token);
    if (jwt.algorithm != "RS256" || jwt.signature_b64.empty() || public_key_pem.empty()) {
        return false;
    }

    const size_t first_dot = token.find('.');
    const size_t second_dot = token.find('.', first_dot + 1);
    if (first_dot == std::string::npos || second_dot == std::string::npos) {
        return false;
    }

    const std::string header_b64 = token.substr(0, first_dot);
    const std::string payload_b64 = token.substr(first_dot + 1, second_dot - first_dot - 1);
    const std::string message = header_b64 + "." + payload_b64;

    std::string signature_bytes = base64url_decode(jwt.signature_b64);
    if (signature_bytes.empty() && !jwt.signature_b64.empty()) {
        return false;
    }

    BIO* bio = BIO_new_mem_buf(public_key_pem.data(), static_cast<int>(public_key_pem.size()));
    if (!bio) {
        return false;
    }

    EVP_PKEY* pkey = PEM_read_bio_PUBKEY(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);
    if (!pkey) {
        return false;
    }

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    bool verified = false;
    if (ctx && EVP_DigestVerifyInit(ctx, nullptr, EVP_sha256(), nullptr, pkey) == 1) {
        verified = EVP_DigestVerify(
            ctx,
            reinterpret_cast<const unsigned char*>(signature_bytes.data()),
            signature_bytes.size(),
            reinterpret_cast<const unsigned char*>(message.data()),
            message.size()) == 1;
    }

    EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(pkey);
    return verified;
}

static bool verify_jwt_signature(
    const JwtToken& jwt,
    const std::string& header_b64,
    const std::string& payload_b64,
    const std::string& secret)
{
    const std::string computed_sig = compute_jwt_signature(header_b64, payload_b64, secret, jwt.algorithm);
    return computed_sig == jwt.signature_b64;
}

static bool is_jwt_expired(const JwtClaims& claims)
{
    if (claims.exp <= 0) {
        return false; // No expiration
    }
    
    const auto now = std::chrono::system_clock::now().time_since_epoch().count() / 1000000000LL;
    return now > claims.exp;
}

static bool is_jwt_valid_nbf(const JwtClaims& claims)
{
    if (claims.nbf <= 0) {
        return true; // No "not before" restriction
    }
    
    const auto now = std::chrono::system_clock::now().time_since_epoch().count() / 1000000000LL;
    return now >= claims.nbf;
}

}