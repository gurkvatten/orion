#include <src/net_http_client.h>

namespace orion::net {

Error oauth2_password_grant(
    HttpClient::RequestDto& request,
    const std::string& username,
    const std::string& password,
    HttpClient::OAuth2Scheme oauth_scheme,
    const std::string& digest_challenge_header = {});

Error oauth2_client_credentials_grant(
    HttpClient::RequestDto& request,
    const std::string& client_id,
    const std::string& client_secret,
    HttpClient::OAuth2Scheme oauth_scheme,
    const std::string& digest_challenge_header = {});

Error oauth2_password_grant_basic(
    HttpClient::RequestDto& request, const std::string& username, const std::string& password);

Error oauth2_password_grant_digest(HttpClient::RequestDto& request, const std::string& username, const std::string& password
    , const std::string& digest_challenge_header, HttpClient::OAuth2Scheme oauth_scheme);

void oauth2_refresh_token_grant(
    HttpClient::RequestDto& request,
    const std::string& refresh_token,
    const std::string& client_id,
    const std::string& acccess_token,
    const std::string& token_type = "Bearer");

} //end of orion::net
