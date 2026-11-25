#include <src/openid.h>
#include <rfl/json.hpp>

namespace orion::net::tests {

using AccessToken = std::variant<orion::net::HttpClient::AccessToken>;

HttpClient::AccessToken
test_openid_authentication()
{
    OpenIDAuthentication::PasswordGrantRequestDTO request_dto{
        .realm_name = "kagi",
        .client_id = "kagi:sync",
        .username = "spalda2@hotmail.com",
        .password = "Mikin"
    };
    Uri uri = Uri::parse_strict("https://172.16.56.1:8081/");
    auto openid_auth = OpenIDAuthentication::create(std::move(uri), OpenIDAuthentication::ServerKind::Keycloak);
    OpenIDAuthentication::TokenResponseDTO token_response = openid_auth->password_grant(request_dto);
    orion::log_debug(token_response.error.message().c_str()); 
    AccessToken access_token = token_response.access_token;
    orion::log_debug("password_grant response: %s"
        , rfl::json::write<rfl::AddNamespacedTagsToVariants>(access_token).c_str()); 
    return token_response.access_token;
}

HttpClient::AccessToken
test_openid_refresh_token(const HttpClient::AccessToken& token)
{
    OpenIDAuthentication::RefreshTokenGrantRequestDTO request_dto{
        .realm_name = "kagi",
        .client_id = "kagi:sync",
        .access_token = token.access_token,
        .refresh_token = token.refresh_token,
        .token_type = token.token_type
    };
    Uri uri = Uri::parse_strict("https://172.16.56.1:8081/");
    auto openid_auth = OpenIDAuthentication::create(std::move(uri), OpenIDAuthentication::ServerKind::Keycloak);
    OpenIDAuthentication::TokenResponseDTO refresh_token_response = openid_auth->refresh_token_grant(request_dto);
    orion::log_debug(refresh_token_response.error.message().c_str()); 
    AccessToken refresh_token = refresh_token_response.access_token;
    orion::log_debug("Rrefresh_token_grant response: %s"
        , rfl::json::write<rfl::AddNamespacedTagsToVariants>(refresh_token).c_str()); 
    return refresh_token_response.access_token;
}
}//orion::net::tests end
