#pragma once

#include <grpcpp/grpcpp.h>
#include <memory>
#include "auth.grpc.pb.h"
#include "common/jwt_validator.h"
#include "common/redis_client.h"
#include "common/db_pool.h"

namespace saasforge {
namespace auth {

class AuthServiceImpl final : public ::saasforge::auth::AuthService::Service {
public:
    AuthServiceImpl(
        std::shared_ptr<common::RedisClient> redis_client,
        std::shared_ptr<common::DbPool> db_pool,
        const std::string& jwt_public_key,
        const std::string& jwt_private_key
    );

    // RPC method implementations
    grpc::Status Login(
        grpc::ServerContext* context,
        const LoginRequest* request,
        LoginResponse* response
    ) override;

    grpc::Status Logout(
        grpc::ServerContext* context,
        const LogoutRequest* request,
        LogoutResponse* response
    ) override;

    grpc::Status RefreshToken(
        grpc::ServerContext* context,
        const RefreshTokenRequest* request,
        RefreshTokenResponse* response
    ) override;

    grpc::Status ValidateToken(
        grpc::ServerContext* context,
        const ValidateTokenRequest* request,
        ValidateTokenResponse* response
    ) override;

    grpc::Status CreateApiKey(
        grpc::ServerContext* context,
        const CreateApiKeyRequest* request,
        CreateApiKeyResponse* response
    ) override;

    grpc::Status RevokeApiKey(
        grpc::ServerContext* context,
        const RevokeApiKeyRequest* request,
        RevokeApiKeyResponse* response
    ) override;

    // 2FA / TOTP methods
    grpc::Status EnrollTOTP(
        grpc::ServerContext* context,
        const EnrollTOTPRequest* request,
        EnrollTOTPResponse* response
    ) override;

    grpc::Status VerifyTOTP(
        grpc::ServerContext* context,
        const VerifyTOTPRequest* request,
        VerifyTOTPResponse* response
    ) override;

    grpc::Status DisableTOTP(
        grpc::ServerContext* context,
        const DisableTOTPRequest* request,
        DisableTOTPResponse* response
    ) override;

    grpc::Status GenerateBackupCodes(
        grpc::ServerContext* context,
        const GenerateBackupCodesRequest* request,
        GenerateBackupCodesResponse* response
    ) override;

    // OTP methods
    grpc::Status SendOTP(
        grpc::ServerContext* context,
        const SendOTPRequest* request,
        SendOTPResponse* response
    ) override;

    grpc::Status VerifyOTP(
        grpc::ServerContext* context,
        const VerifyOTPRequest* request,
        VerifyOTPResponse* response
    ) override;

    // OAuth methods
    grpc::Status InitiateOAuth(
        grpc::ServerContext* context,
        const InitiateOAuthRequest* request,
        InitiateOAuthResponse* response
    ) override;

    grpc::Status HandleOAuthCallback(
        grpc::ServerContext* context,
        const OAuthCallbackRequest* request,
        OAuthCallbackResponse* response
    ) override;

    // API Key validation with scopes
    grpc::Status ValidateApiKey(
        grpc::ServerContext* context,
        const ValidateApiKeyRequest* request,
        ValidateApiKeyResponse* response
    ) override;

private:
    std::shared_ptr<common::RedisClient> redis_client_;
    std::shared_ptr<common::DbPool> db_pool_;
    std::shared_ptr<common::JwtValidator> jwt_validator_;
    std::string jwt_private_key_;

    // Helper methods
    std::string GenerateAccessToken(const std::string& user_id, const std::string& tenant_id,
                                    const std::string& email, const std::vector<std::string>& roles);
    std::string GenerateRefreshToken(const std::string& user_id);
    bool VerifyPassword(const std::string& password, const std::string& hashed_password);
    std::string HashPassword(const std::string& password);

    // Rate limiting helper
    bool CheckRateLimit(const std::string& key, int max_attempts, int window_seconds);

    // Scope validation helper (wildcard support for API keys)
    bool ValidateScope(const std::vector<std::string>& granted_scopes, const std::string& requested_scope);

    // OTP generation and storage
    std::string GenerateOTP();
    void StoreOTP(const std::string& email, const std::string& otp, const std::string& purpose, int ttl_seconds);
    std::optional<std::string> GetStoredOTP(const std::string& email, const std::string& purpose);
};

} // namespace auth
} // namespace saasforge
