#include "openid.h"
#include "internal/oauth2.h"
#include <rfl/json.hpp>
#include <format>

namespace orion::net {

class OidcServer {

public:
    OidcServer() = delete;
    OidcServer(std::shared_ptr<HttpClient>& m_http_client) : m_http_client(m_http_client)
    {}

    void
    cancel(HttpClient::CancelCb cb)
    {
        if (m_http_client) {
            m_http_client->cancel(cb);
        }
    }

    virtual OpenIDAuthentication::TokenResponseDTO password_grant(const OpenIDAuthentication::PasswordGrantRequestDTO& request_dto) = 0;
    virtual OpenIDAuthentication::TokenResponseDTO refresh_token_grant(const OpenIDAuthentication::RefreshTokenGrantRequestDTO& request_dto) = 0;
    
    ~OidcServer() = default;

protected:
    std::shared_ptr<HttpClient> m_http_client;
};

class KeycloakServer : public OidcServer {

public:
    KeycloakServer(std::shared_ptr<HttpClient>& m_http_client) : OidcServer(m_http_client)
    {}

    OpenIDAuthentication::TokenResponseDTO
    password_grant(const OpenIDAuthentication::PasswordGrantRequestDTO& password_grant_request_dto) override
    {
        HttpClient::RequestDto request;
        request.raw_url = m_http_client->base_url() + std::format(R"(/realms/{}/protocol/openid-connect/token)", password_grant_request_dto.realm_name);
        Error err = oauth2_password_grant(request, password_grant_request_dto.username, password_grant_request_dto.password, HttpClient::OAuth2Scheme::Bearer);
        if (err) {
            OpenIDAuthentication::TokenResponseDTO token_response;
            token_response.error = err;
            return token_response;
        }
     
        return execute_token_request(request);
    }

    OpenIDAuthentication::TokenResponseDTO
    refresh_token_grant(const OpenIDAuthentication::RefreshTokenGrantRequestDTO& request_dto) override
    {
        HttpClient::RequestDto request;
        request.raw_url = m_http_client->base_url() + std::format(R"(/realms/{}/protocol/openid-connect/token)", request_dto.realm_name);
        oauth2_refresh_token_grant(request, request_dto.refresh_token, request_dto.client_id, request_dto.access_token, request_dto.token_type);

        return execute_token_request(request);
    }

private:
    struct TokenResponseHolder {
        OpenIDAuthentication::TokenResponseDTO token_response;
    };

    OpenIDAuthentication::TokenResponseDTO
    execute_token_request(const HttpClient::RequestDto& request)
    {
        TokenResponseHolder token_response_holder;

        m_http_client->execute(request,
            [&token_response_holder](HttpClient::ResponseDto response) {
                if (response.error) {
                    token_response_holder.token_response.error = response.error;
                    return;
                }

                if (response.state == HttpClient::ResponseState::done) {
                    if (response.status_code != 200) {
                        token_response_holder.token_response.error = MAKE_ERROR(ErrorCode::net_io, "%i", response.status_code);
                        return;
                    }
                    const std::string json = response.body.content;
                    token_response_holder.token_response.access_token = rfl::json::read<HttpClient::AccessToken>(json).value();
                }
            });

            //it blocks for HTTP_CLIENT_DEFAULT_TIMEOUT_MS
            if (m_http_client->block()) {
                token_response_holder.token_response.error = MAKE_ERROR(ErrorCode::timeout, "Request fo access token timed out");
            }

        return token_response_holder.token_response;
    }
};

std::shared_ptr<OpenIDAuthentication>
OpenIDAuthentication::create(const Uri&& srever_root, ServerKind server_kind)
{
    //create a new http client for this server
    auto http_client = HttpClient::create(std::forward<const Uri>(srever_root));
    return create(http_client, server_kind);
}

std::shared_ptr<OpenIDAuthentication>
OpenIDAuthentication::create(std::shared_ptr<HttpClient>& http_client, ServerKind server_kind)
{
    //create a new OidcServer instance with the provided http_client and server_kind
    return std::shared_ptr<OpenIDAuthentication>(new OpenIDAuthentication(http_client, server_kind));
}

OpenIDAuthentication::OpenIDAuthentication(std::shared_ptr<HttpClient>& http_client, ServerKind server_kind) : 
    m_impl(std::make_unique<KeycloakServer>(http_client)) //one server kind for now.
{}

OpenIDAuthentication::TokenResponseDTO
OpenIDAuthentication::password_grant(const PasswordGrantRequestDTO& request_dto)
{
    return m_impl->password_grant(request_dto);
}

OpenIDAuthentication::TokenResponseDTO
OpenIDAuthentication::refresh_token_grant(const RefreshTokenGrantRequestDTO& request_dto)
{
    return m_impl->refresh_token_grant(request_dto);
}

void
OpenIDAuthentication::cancel(HttpClient::CancelCb cb)
{
    m_impl->cancel(cb);
}

} //orion::net end