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
};

} // namespace auth
} // namespace saasforge
