#include "auth/auth_service.h"
#include "common/tenant_context.h"
#include "common/password_hasher.h"
#include <jwt-cpp/jwt.h>
#include <iomanip>
#include <sstream>
#include <chrono>
#include <random>
#include <iostream>

namespace saasforge {
namespace auth {

AuthServiceImpl::AuthServiceImpl(
    std::shared_ptr<common::RedisClient> redis_client,
    std::shared_ptr<common::DbPool> db_pool,
    const std::string& jwt_public_key,
    const std::string& jwt_private_key
) : redis_client_(redis_client),
    db_pool_(db_pool),
    jwt_validator_(std::make_shared<common::JwtValidator>(jwt_public_key, redis_client)),
    jwt_private_key_(jwt_private_key) {
    std::cout << "AuthService initialized" << std::endl;
}

grpc::Status AuthServiceImpl::Login(
    grpc::ServerContext* context,
    const LoginRequest* request,
    LoginResponse* response
) {
    try {
        // Validate input
        if (request->email().empty() || request->password().empty()) {
            return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Email and password required");
        }

        // Query database for user
        auto conn_guard = db_pool_->AcquireConnection();
        pqxx::work txn(*conn_guard);

        auto result = txn.exec_params(
            "SELECT id, tenant_id, email, password_hash, totp_secret "
            "FROM users WHERE email = $1 AND deleted_at IS NULL",
            request->email()
        );

        if (result.empty()) {
            return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "Invalid credentials");
        }

        auto row = result[0];
        std::string user_id = row["id"].as<std::string>();
        std::string tenant_id = row["tenant_id"].as<std::string>();
        std::string email = row["email"].as<std::string>();
        std::string password_hash = row["password_hash"].as<std::string>();

        // Verify password
        if (!VerifyPassword(request->password(), password_hash)) {
            return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "Invalid credentials");
        }

        // Handle 2FA if enabled (TODO: implement TOTP validation)
        // if (!row["totp_secret"].is_null() && request->totp_code().empty()) {
        //     return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, "TOTP code required");
        // }

        // Extract roles
        std::vector<std::string> roles;

        // Generate tokens
        std::string access_token = GenerateAccessToken(user_id, tenant_id, email, roles);
        std::string refresh_token = GenerateRefreshToken(user_id);

        // Store refresh token in Redis (30 days TTL)
        redis_client_->SetSession("refresh:" + user_id, refresh_token, 30 * 24 * 3600);

        // Set response
        response->set_access_token(access_token);
        response->set_refresh_token(refresh_token);
        response->set_expires_in(900); // 15 minutes

        txn.commit();

        return grpc::Status::OK;

    } catch (const std::exception& e) {
        return grpc::Status(grpc::StatusCode::INTERNAL, std::string("Login failed: ") + e.what());
    }
}

grpc::Status AuthServiceImpl::Logout(
    grpc::ServerContext* context,
    const LogoutRequest* request,
    LogoutResponse* response
) {
    try {
        // Logout uses refresh_token, not access_token
        if (request->refresh_token().empty()) {
            return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Refresh token required");
        }

        // Extract user_id from refresh token (format: "user_id:random")
        std::string refresh_token = request->refresh_token();
        size_t colon_pos = refresh_token.find(':');
        if (colon_pos == std::string::npos) {
            return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Invalid refresh token format");
        }

        std::string user_id = refresh_token.substr(0, colon_pos);

        // Remove refresh token from Redis
        redis_client_->DeleteSession("refresh:" + user_id);

        response->set_success(true);
        return grpc::Status::OK;

    } catch (const std::exception& e) {
        return grpc::Status(grpc::StatusCode::INTERNAL, std::string("Logout failed: ") + e.what());
    }
}

grpc::Status AuthServiceImpl::RefreshToken(
    grpc::ServerContext* context,
    const RefreshTokenRequest* request,
    RefreshTokenResponse* response
) {
    return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "RefreshToken not yet implemented");
}

grpc::Status AuthServiceImpl::ValidateToken(
    grpc::ServerContext* context,
    const ValidateTokenRequest* request,
    ValidateTokenResponse* response
) {
    try {
        auto claims = jwt_validator_->Validate(request->access_token());

        if (!claims) {
            response->set_valid(false);
            return grpc::Status::OK;
        }

        response->set_valid(true);
        response->set_user_id(claims->user_id);
        response->set_tenant_id(claims->tenant_id);

        for (const auto& role : claims->roles) {
            response->add_roles(role);
        }

        return grpc::Status::OK;

    } catch (const std::exception& e) {
        return grpc::Status(grpc::StatusCode::INTERNAL, std::string("Validation failed: ") + e.what());
    }
}

grpc::Status AuthServiceImpl::CreateApiKey(
    grpc::ServerContext* context,
    const CreateApiKeyRequest* request,
    CreateApiKeyResponse* response
) {
    try {
        auto tenant_ctx = common::TenantContextInterceptor::ExtractFromMetadata(context);

        if (tenant_ctx.user_id.empty()) {
            return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "Authentication required");
        }

        // Generate API key
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 255);

        std::stringstream ss;
        ss << "sk_";
        for (int i = 0; i < 32; i++) {
            ss << std::hex << std::setw(2) << std::setfill('0') << dis(gen);
        }
        std::string api_key = ss.str();

        // Store in database
        auto conn_guard = db_pool_->AcquireConnection();
        pqxx::work txn(*conn_guard);

        // Convert scopes repeated field to string
        std::string scopes_str;
        for (int i = 0; i < request->scopes_size(); i++) {
            if (i > 0) scopes_str += ",";
            scopes_str += request->scopes(i);
        }

        auto result = txn.exec_params(
            "INSERT INTO api_keys (user_id, tenant_id, key_hash, name, scopes, expires_at) "
            "VALUES ($1, $2, $3, $4, $5, NOW() + INTERVAL '1 year') "
            "RETURNING id, created_at",
            tenant_ctx.user_id,
            tenant_ctx.tenant_id,
            HashPassword(api_key),
            request->name(),
            scopes_str
        );

        response->set_api_key(api_key);
        response->set_key_id(result[0]["id"].as<std::string>());

        txn.commit();

        return grpc::Status::OK;

    } catch (const std::exception& e) {
        return grpc::Status(grpc::StatusCode::INTERNAL, std::string("API key creation failed: ") + e.what());
    }
}

grpc::Status AuthServiceImpl::RevokeApiKey(
    grpc::ServerContext* context,
    const RevokeApiKeyRequest* request,
    RevokeApiKeyResponse* response
) {
    try {
        auto tenant_ctx = common::TenantContextInterceptor::ExtractFromMetadata(context);

        if (tenant_ctx.user_id.empty()) {
            return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "Authentication required");
        }

        auto conn_guard = db_pool_->AcquireConnection();
        pqxx::work txn(*conn_guard);

        auto result = txn.exec_params(
            "UPDATE api_keys SET revoked_at = NOW() "
            "WHERE id = $1 AND user_id = $2 AND tenant_id = $3 AND revoked_at IS NULL",
            request->key_id(),
            tenant_ctx.user_id,
            tenant_ctx.tenant_id
        );

        if (result.affected_rows() == 0) {
            return grpc::Status(grpc::StatusCode::NOT_FOUND, "API key not found or already revoked");
        }

        response->set_success(true);
        txn.commit();

        return grpc::Status::OK;

    } catch (const std::exception& e) {
        return grpc::Status(grpc::StatusCode::INTERNAL, std::string("API key revocation failed: ") + e.what());
    }
}

// Helper methods

std::string AuthServiceImpl::GenerateAccessToken(
    const std::string& user_id,
    const std::string& tenant_id,
    const std::string& email,
    const std::vector<std::string>& roles
) {
    auto now = std::chrono::system_clock::now();
    auto exp = now + std::chrono::minutes(15);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    std::stringstream jti_ss;
    for (int i = 0; i < 16; i++) {
        jti_ss << std::hex << std::setw(2) << std::setfill('0') << dis(gen);
    }

    auto token = jwt::create()
        .set_issuer("saasforge")
        .set_type("JWT")
        .set_subject(user_id)
        .set_issued_at(now)
        .set_expires_at(exp)
        .set_id(jti_ss.str())
        .set_payload_claim("tenant_id", jwt::claim(tenant_id))
        .set_payload_claim("email", jwt::claim(email))
        .sign(jwt::algorithm::rs256("", jwt_private_key_, "", ""));

    return token;
}

std::string AuthServiceImpl::GenerateRefreshToken(const std::string& user_id) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);

    std::stringstream ss;
    for (int i = 0; i < 32; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << dis(gen);
    }

    return user_id + ":" + ss.str();
}

bool AuthServiceImpl::VerifyPassword(const std::string& password, const std::string& hashed_password) {
    // Use Argon2id for secure password verification
    return common::PasswordHasher::VerifyPassword(password, hashed_password);
}

std::string AuthServiceImpl::HashPassword(const std::string& password) {
    // Use Argon2id for secure password hashing
    // This replaces the insecure SHA-256 implementation
    return common::PasswordHasher::HashPassword(password);
}

} // namespace auth
} // namespace saasforge
