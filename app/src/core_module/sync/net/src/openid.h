#include <src/net_http_client.h>
#include <memory>

namespace orion::net {

class OidcServer;

//so far this only supporst grant_type password and refresh_token.
class OpenIDAuthentication {

public:
    //to be used by UI along with others authorizations mothed in aththorization_code grant_type.
    //so far this isn't completed. This part shoukd, in theory be already implemeted somewhwre.
    struct AuthorizationCodeGrantFormRequestDTO {
        std::string client_id; //mandatory i.e. kagi:ui
        std::string redirect_uri; //optional http%3A%2F%2Flocalhost%3A4000%2Fkagi
        //scope always 'openid'
        //state always some random value to verify at the end of the authentication
        //response_type always 'code' as the most secure one
        //code_challenge goes the above 'code' type hash of some randome string
        //code_challenge_method goes with the code_challenge above, aways 'S256'
    };

    struct PasswordGrantRequestDTO {
        std::string realm_name; //mandatory for some oicd providers e.g. keycloak i.e. kagi
        std::string client_id; //mandatory i.e. kagi:ui
        std::string username; //mandatory
        std::string password; //mandatory
        std::string scope = "openid"; //optional, default 'openid'
    };

    struct RefreshTokenGrantRequestDTO {
        std::string realm_name; //mandatory for some oicd providers e.g. keycloak i.e. kagi
        std::string client_id; //mandatory i.e. kagi:ui
        std::string access_token; //mandatory i.e. kagi:ui
        std::string refresh_token; //mandatory
        std::string token_type;
    };

    struct TokenResponseDTO {
        Error error = Error::ok();
        HttpClient::AccessToken access_token;
    };

    enum ServerKind {
        Keycloak
    };

public:
    OpenIDAuthentication() = delete;

    static std::shared_ptr<OpenIDAuthentication> create(const Uri&& srever_root, ServerKind server_kind); 
    static std::shared_ptr<OpenIDAuthentication> create(std::shared_ptr<HttpClient>& http_client, ServerKind server_kind); 

    TokenResponseDTO password_grant(const PasswordGrantRequestDTO& request_dto);
    TokenResponseDTO refresh_token_grant(const RefreshTokenGrantRequestDTO& request_dto);

    void cancel(HttpClient::CancelCb cb);

    ~OpenIDAuthentication() = default;

private:
    OpenIDAuthentication(std::shared_ptr<HttpClient>& http_client, ServerKind server_kind);

private:
    std::unique_ptr<OidcServer> m_impl;
};

} //orion::net end
