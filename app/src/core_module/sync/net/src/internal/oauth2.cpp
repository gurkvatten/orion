#include "oauth2.h"
#include <../common/src/uri.h>
#include <../common/src/conversion_util.h>
#include <../common/src/crypto.h>
#include <../common/src/string_util.h>
#include <../common/src/error.h>
#include <string>
#include <sstream>
#include <iomanip>
#include <cctype>
#include <chrono>
#include <vector>

namespace orion::net {

/*
Notes
Replace auth_base_url with your OAuth2 authority, e.g. "https://auth.example.com".
Replace "/oauth2/token" with the actual token endpoint path.
In case of Authorization: Basic ... instead of client_id/client_secret in the body, use HttpClient::OAuth2Scheme::Basic.
In case of HTTP Digest authentication, pass the challenge header value from WWW-Authenticate to digest_challenge_header.
*/

inline
std::string
trim_string(std::string value)
{
    str::trim(&value);
    return value;
}

static
std::string
make_basic_authorization_header(
    const std::string& id,
    const std::string& secret)
{
    const std::string credentials = id + ':' + secret;
    return "Basic " + base64_encode(credentials);
}

static
std::string
make_form_urlencoded(const Headers& fields)
{
    std::string encoded;
    encoded.reserve(256);

    for (size_t i = 0; i < fields.size(); ++i) {
        if (i > 0) {
            encoded.push_back('&');
        }
        encoded += Uri::encode(fields[i].first);
        encoded.push_back('=');
        encoded += Uri::encode(fields[i].second);
    }

    return encoded;
}

struct DigestAuthChallenge {
    std::string realm;
    std::string nonce;
    std::string opaque;
    std::string algorithm = "MD5";
    std::string qop;
    bool stale = false;
};

static
std::string
parse_digest_field_value(const std::string& value)
{
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
        return std::string(value.substr(1, value.size() - 2));
    }
    return value;
}

static
DigestAuthChallenge
parse_digest_authenticate_header(const std::string& header)
{
    DigestAuthChallenge challenge;
    std::string data = header;
    const std::string prefix = "Digest";
    auto pos = data.find(prefix);
    if (pos != std::string::npos) {
        data = data.substr(pos + prefix.size());
    }

    size_t i = 0;
    while (i < data.size()) {
        while (i < data.size() && (std::isspace(static_cast<unsigned char>(data[i])) || data[i] == ',')) {
            ++i;
        }
        if (i >= data.size()) {
            break;
        }

        size_t name_begin = i;
        while (i < data.size() && data[i] != '=' && data[i] != ',' && data[i] != ' ') {
            ++i;
        }
        const std::string name = std::string(data.substr(name_begin, i - name_begin));

        while (i < data.size() && (std::isspace(static_cast<unsigned char>(data[i])) || data[i] == '=')) {
            ++i;
        }

        if (i >= data.size()) {
            break;
        }

        std::string value;
        if (data[i] == '"') {
            const size_t value_begin = i;
            ++i;
            while (i < data.size() && data[i] != '"') {
                ++i;
            }
            if (i < data.size()) {
                ++i;
            }
            value = std::string(data.substr(value_begin, i - value_begin));
        } else {
            const size_t value_begin = i;
            while (i < data.size() && data[i] != ',') {
                ++i;
            }
            value = std::string(data.substr(value_begin, i - value_begin));
        }

        const std::string parsed = parse_digest_field_value(trim_string(value));

        if (name == "realm") {
            challenge.realm = parsed;
        } else if (name == "nonce") {
            challenge.nonce = parsed;
        } else if (name == "opaque") {
            challenge.opaque = parsed;
        } else if (name == "algorithm") {
            challenge.algorithm = parsed;
        } else if (name == "qop") {
            challenge.qop = parsed;
        } else if (name == "stale") {
            challenge.stale = (parsed == "true" || parsed == "1");
        }
    }

    return challenge;
}

static
std::string
choose_qop(const std::string& qop_header)
{
    if (qop_header.find("auth") != std::string::npos) {
        return "auth";
    }
    const size_t comma = qop_header.find(',');
    if (comma == std::string::npos) {
        return trim_string(qop_header);
    }
    return trim_string(qop_header.substr(0, comma));
}

static
std::string
make_cnonce()
{
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    std::ostringstream ss;
    ss << std::hex << now;
    return ss.str();
}

static
std::string
make_digest_authorization_header(
    const DigestAuthChallenge& challenge,
    const std::string& username,
    const std::string& password,
    const std::string& method,
    std::string uri)
{
    const std::string qop = !challenge.qop.empty() ? choose_qop(challenge.qop) : std::string();
    const std::string cnonce = !qop.empty() ? make_cnonce() : std::string();
    const std::string nc = "00000001";

    const std::string ha1 = orion::crypto::md5_str(username + ":" + challenge.realm + ":" + password);
    const std::string ha2 = orion::crypto::md5_str(method + ":" + uri);

    std::string response;
    if (!qop.empty()) {
        response = orion::crypto::md5_str(ha1 + ":" + challenge.nonce + ":" + nc + ":" + cnonce + ":" + qop + ":" + ha2);
    } else {
        response = orion::crypto::md5_str(ha1 + ":" + challenge.nonce + ":" + ha2);
    }

    std::ostringstream auth;
    auth << "Digest username=\"" << username << "\", realm=\"" << challenge.realm << "\", nonce=\"" << challenge.nonce << "\", uri=\"" << uri << "\", response=\"" << response << "\"";
    if (!challenge.algorithm.empty()) {
        auth << ", algorithm=" << challenge.algorithm;
    }
    if (!qop.empty()) {
        auth << ", qop=" << qop << ", nc=" << nc << ", cnonce=\"" << cnonce << "\"";
    }
    if (!challenge.opaque.empty()) {
        auth << ", opaque=\"" << challenge.opaque << "\"";
    }

    return auth.str();
}

static
Error
oauth2_password_or_client_credentials_grant(
    HttpClient::RequestDto& request,
    std::string username,
    std::string password,
    const std::string& client_id,
    const std::string& client_secret,
    HttpClient::OAuth2Scheme oauth_scheme,
    const std::string& digest_challenge_header = {})
{
    const DigestAuthChallenge digest_challenge = oauth_scheme == HttpClient::OAuth2Scheme::Digest
        ? parse_digest_authenticate_header(digest_challenge_header)
        : DigestAuthChallenge{};

    if (client_id.empty() && username.empty()) {
        return MAKE_ERROR(ErrorCode::argument, "Either client_id or username must be given");
    }
    if (client_secret.empty() && password.empty()) {
        return MAKE_ERROR(ErrorCode::argument, "Either client_secret or password must be given");
    }
    if (username.empty()) {
        username = client_id;
    }
    if (password.empty()) {
        password = client_secret;
    }

    std::string grant_type = client_id.empty() || client_secret.empty() ? "password" : "client_credentials";
    Headers fields = {
        {"grant_type", grant_type},
        {"username", username},
        {"password", password},
    };

    //this could also be in the Authorization Header
    if (!client_id.empty()) {
        fields.emplace_back("client_id", client_id);
    }
    if (!client_secret.empty()) {
        fields.emplace_back("client_secret", client_secret);
    }

    const std::string form_body = make_form_urlencoded(fields);

    HttpClient::RequestDto::Body body;
    request.body.content_type = "application/x-www-form-urlencoded";
    request.body.content = form_body;
    request.body.total_content_length = body.content.size();

    request.method = METHOD_POST;
    headers_set(request.headers, "Accept", "application/json");

    if (oauth_scheme == HttpClient::OAuth2Scheme::Basic) {
        //https://datatracker.ietf.org/doc/html/rfc7617#page-3
        headers_set(request.headers, "Authorization", make_basic_authorization_header(username, password));
    } else if (oauth_scheme == HttpClient::OAuth2Scheme::Digest) {
        headers_set(request.headers, "Authorization", make_digest_authorization_header(digest_challenge, username, password, request.method, request.raw_url));
    }

    return Error::ok();
}

Error
oauth2_password_grant(
    HttpClient::RequestDto& request,
    const std::string& username,
    const std::string& password,
    HttpClient::OAuth2Scheme oauth_scheme,
    const std::string& digest_challenge_header)
{
    return oauth2_password_or_client_credentials_grant(request, username, password, "", "", oauth_scheme, digest_challenge_header);
}

Error
oauth2_client_credentials_grant(
    HttpClient::RequestDto& request,
    const std::string& client_id,
    const std::string& client_secret,
    HttpClient::OAuth2Scheme oauth_scheme,
    const std::string& digest_challenge_header)
{
    return oauth2_password_or_client_credentials_grant(request, "", "", client_id, client_secret, oauth_scheme, digest_challenge_header);
}

Error
oauth2_password_grant_basic(
    HttpClient::RequestDto& request, const std::string& username, const std::string& password)
{
    return oauth2_password_grant(request, username, password, HttpClient::OAuth2Scheme::Basic);
}

Error
oauth2_password_grant_digest(HttpClient::RequestDto& request, const std::string& username, const std::string& password
    , const std::string& digest_challenge_header, HttpClient::OAuth2Scheme oauth_scheme)
{
    return oauth2_password_grant(request, username, password, HttpClient::OAuth2Scheme::Digest, digest_challenge_header);
}

void
oauth2_refresh_token_grant(
    HttpClient::RequestDto& request,
    const std::string& refresh_token,
    const std::string& client_id,
    const std::string& acccess_token,
    const std::string& token_type)
{
    Headers fields = {
        {"grant_type", "refresh_token"},
        {"client_id", client_id},
        {"refresh_token", refresh_token},
        {"acccess_token", acccess_token},
        {"token_type", token_type}
    };

    const std::string form_body = make_form_urlencoded(fields);

    request.method = METHOD_POST;
    request.body.content_type = "application/x-www-form-urlencoded";
    request.body.content = form_body;
    request.body.total_content_length = form_body.size();
}

/*
void oauth2_bearer_request(
    const std::string& api_base_url,
    const std::string& bearer_token,
    const std::string& resource_path,
    HttpClient::ResultCb callback)
{
    //this won't be here so that we can separate the req construction from the actuall call and response handling
    //there''l be a method to construct the needed headers and body instead
    auto client = HttpClient::create(Uri::parse_strict(api_base_url.c_str()));

    client->interceptor_cb([bearer_token](HttpClient::RequestDto& request) {
        headers_set(request.headers, "Accept", "application/json");
        headers_set(request.headers, "Authorization", "Bearer " + bearer_token);
    });

    client->get(resource_path, std::move(callback));
    while (client->process()) {
        // pump callbacks on the calling thread
    }
}
*/
} // namespace orion::net