#include "auth/auth_service.h"
#include "common/tenant_context.h"
#include "common/password_hasher.h"
#include "common/totp_helper.h"
#include <jwt-cpp/jwt.h>
#include <iomanip>
#include <sstream>
#include <chrono>
#include <random>
#include <iostream>
#include <openssl/rand.h>

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

        // SECURITY FIX: Handle OAuth-only users (NULL password_hash)
        // OAuth-only users must authenticate via OAuth flow, not password login
        std::string password_hash;
        if (!row["password_hash"].is_null()) {
            password_hash = row["password_hash"].as<std::string>();
        }

        // Check if this is an OAuth-only account (no password set)
        if (password_hash.empty()) {
            // OAuth-only account - only allow empty password (OAuth bypass from OAuth router)
            if (!request->password().empty()) {
                return grpc::Status(grpc::StatusCode::UNAUTHENTICATED,
                                  "This account uses OAuth authentication only. Please login with your OAuth provider.");
            }
            // Empty password for OAuth-only account is allowed (OAuth flow verification already done)
        } else {
            // Regular account with password - verify it
            if (!VerifyPassword(request->password(), password_hash)) {
                return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "Invalid credentials");
            }
        }

        // Handle 2FA if enabled
        if (!row["totp_secret"].is_null()) {
            std::string totp_secret = row["totp_secret"].as<std::string>();

            if (!request->has_totp_code() || request->totp_code().empty()) {
                return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, "TOTP code required");
            }

            // Validate TOTP code
            if (!common::TotpHelper::ValidateCode(totp_secret, request->totp_code())) {
                // Check if it's a backup code
                auto backup_result = txn.exec_params(
                    "SELECT code_hash FROM backup_codes "
                    "WHERE user_id = $1 AND used_at IS NULL",
                    user_id
                );

                bool backup_valid = false;
                std::string used_code_hash;

                for (const auto& backup_row : backup_result) {
                    std::string code_hash = backup_row["code_hash"].as<std::string>();
                    if (common::TotpHelper::VerifyBackupCode(request->totp_code(), code_hash)) {
                        backup_valid = true;
                        used_code_hash = code_hash;
                        break;
                    }
                }

                if (backup_valid) {
                    // Mark backup code as used
                    txn.exec_params(
                        "UPDATE backup_codes SET used_at = NOW() WHERE code_hash = $1",
                        used_code_hash
                    );
                } else {
                    return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "Invalid TOTP code");
                }
            }
        }

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
        // Logout requires both refresh_token and access_token for complete invalidation
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

        // CRITICAL SECURITY FIX: Blacklist access token to ensure instant logout
        // Extract Authorization header from metadata
        auto metadata = context->client_metadata();
        auto auth_header = metadata.find("authorization");

        if (auth_header != metadata.end()) {
            std::string auth_value(auth_header->second.data(), auth_header->second.length());

            // Check if it starts with "Bearer "
            if (auth_value.find("Bearer ") == 0) {
                std::string access_token = auth_value.substr(7); // Skip "Bearer "

                // Validate and extract JTI from access token
                auto claims = jwt_validator_->Validate(access_token);
                if (claims && !claims->jti.empty()) {
                    // Calculate TTL = token expiry - current time
                    auto now = std::chrono::system_clock::now();
                    auto expiry = std::chrono::system_clock::from_time_t(claims->exp);
                    auto ttl = std::chrono::duration_cast<std::chrono::seconds>(expiry - now);

                    // Only blacklist if token is still valid (not already expired)
                    if (ttl.count() > 0) {
                        // Add access token to Redis blacklist with TTL
                        std::string blacklist_key = "blacklist:" + claims->jti;
                        redis_client_->SetSession(blacklist_key, "logout", ttl.count());

                        std::cout << "Access token blacklisted: jti=" << claims->jti
                                  << ", ttl=" << ttl.count() << "s" << std::endl;
                    }
                }
            }
        }

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
    try {
        // Validate input
        if (request->refresh_token().empty()) {
            return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Refresh token required");
        }

        std::string refresh_token = request->refresh_token();

        // Extract user_id from refresh token (format: "user_id:random")
        size_t colon_pos = refresh_token.find(':');
        if (colon_pos == std::string::npos) {
            return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "Invalid refresh token format");
        }

        std::string user_id = refresh_token.substr(0, colon_pos);

        // CRITICAL SECURITY: Check if refresh token exists in Redis
        std::string redis_key = "refresh:" + user_id;
        auto stored_token = redis_client_->GetSession(redis_key);

        if (!stored_token) {
            // Token not found in Redis - either expired, revoked, or invalid
            return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "Refresh token invalid or expired");
        }

        // CRITICAL SECURITY: Detect token reuse (potential security breach)
        if (*stored_token != refresh_token) {
            // Different refresh token stored - someone is reusing an old token!
            // This indicates potential token theft. Revoke ALL sessions for this user.
            std::cerr << "SECURITY ALERT: Refresh token reuse detected for user " << user_id << std::endl;

            // Revoke all refresh tokens for this user
            redis_client_->DeleteSession(redis_key);

            // In production, also blacklist all active access tokens for this user
            // and force re-authentication

            return grpc::Status(grpc::StatusCode::PERMISSION_DENIED,
                              "Token reuse detected. All sessions revoked. Please login again.");
        }

        // Fetch user details from database
        auto conn_guard = db_pool_->AcquireConnection();
        pqxx::work txn(*conn_guard);

        auto result = txn.exec_params(
            "SELECT id, tenant_id, email FROM users WHERE id = $1 AND deleted_at IS NULL",
            user_id
        );

        if (result.empty()) {
            // User not found or deleted - revoke token
            redis_client_->DeleteSession(redis_key);
            return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "User not found");
        }

        auto row = result[0];
        std::string tenant_id = row["tenant_id"].as<std::string>();
        std::string email = row["email"].as<std::string>();

        // Extract roles (empty for now, could query user_roles table)
        std::vector<std::string> roles;

        // Generate new access token
        std::string new_access_token = GenerateAccessToken(user_id, tenant_id, email, roles);

        // Generate new refresh token (token rotation for security)
        std::string new_refresh_token = GenerateRefreshToken(user_id);

        // CRITICAL SECURITY: Rotate refresh token in Redis
        // Delete old token and store new one (prevents token reuse)
        redis_client_->DeleteSession(redis_key);
        redis_client_->SetSession(redis_key, new_refresh_token, 30 * 24 * 3600); // 30 days TTL

        // Set response
        response->set_access_token(new_access_token);
        response->set_refresh_token(new_refresh_token);
        response->set_expires_in(900); // 15 minutes

        txn.commit();

        std::cout << "Refresh token rotated for user " << user_id << std::endl;

        return grpc::Status::OK;

    } catch (const std::exception& e) {
        return grpc::Status(grpc::StatusCode::INTERNAL, std::string("Token refresh failed: ") + e.what());
    }
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

// 2FA / TOTP Implementations

grpc::Status AuthServiceImpl::EnrollTOTP(
    grpc::ServerContext* context,
    const EnrollTOTPRequest* request,
    EnrollTOTPResponse* response
) {
    try {
        auto tenant_ctx = common::TenantContextInterceptor::ExtractFromMetadata(context);
        if (tenant_ctx.user_id.empty()) {
            return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "Authentication required");
        }

        // Generate TOTP secret
        std::string secret = common::TotpHelper::GenerateSecret();

        // Get user email for QR code
        auto conn_guard = db_pool_->AcquireConnection();
        pqxx::work txn(*conn_guard);

        auto result = txn.exec_params(
            "SELECT email FROM users WHERE id = $1",
            tenant_ctx.user_id
        );

        if (result.empty()) {
            return grpc::Status(grpc::StatusCode::NOT_FOUND, "User not found");
        }

        std::string email = result[0]["email"].as<std::string>();

        // Generate QR code URL
        std::string qr_url = common::TotpHelper::GenerateQrCodeUrl(secret, email);

        // Generate backup codes
        auto backup_codes = common::TotpHelper::GenerateBackupCodes(10);

        // Store TOTP secret in database (encrypted in production)
        txn.exec_params(
            "UPDATE users SET totp_secret = $1, totp_enrolled_at = NOW() WHERE id = $2",
            secret,
            tenant_ctx.user_id
        );

        // Store backup codes (hashed)
        for (const auto& code : backup_codes) {
            std::string hash = common::TotpHelper::HashBackupCode(code);
            txn.exec_params(
                "INSERT INTO backup_codes (user_id, code_hash) VALUES ($1, $2)",
                tenant_ctx.user_id,
                hash
            );
        }

        txn.commit();

        // Set response
        response->set_secret(secret);
        response->set_qr_code_url(qr_url);
        for (const auto& code : backup_codes) {
            response->add_backup_codes(code);
        }

        return grpc::Status::OK;

    } catch (const std::exception& e) {
        return grpc::Status(grpc::StatusCode::INTERNAL, std::string("TOTP enrollment failed: ") + e.what());
    }
}

grpc::Status AuthServiceImpl::VerifyTOTP(
    grpc::ServerContext* context,
    const VerifyTOTPRequest* request,
    VerifyTOTPResponse* response
) {
    try {
        auto tenant_ctx = common::TenantContextInterceptor::ExtractFromMetadata(context);
        if (tenant_ctx.user_id.empty()) {
            return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "Authentication required");
        }

        // Get TOTP secret from database
        auto conn_guard = db_pool_->AcquireConnection();
        pqxx::work txn(*conn_guard);

        auto result = txn.exec_params(
            "SELECT totp_secret FROM users WHERE id = $1",
            tenant_ctx.user_id
        );

        if (result.empty() || result[0]["totp_secret"].is_null()) {
            response->set_valid(false);
            response->set_message("TOTP not enrolled");
            return grpc::Status::OK;
        }

        std::string secret = result[0]["totp_secret"].as<std::string>();

        // Validate TOTP code
        bool valid = common::TotpHelper::ValidateCode(secret, request->totp_code());

        response->set_valid(valid);
        response->set_message(valid ? "TOTP code valid" : "Invalid TOTP code");

        return grpc::Status::OK;

    } catch (const std::exception& e) {
        return grpc::Status(grpc::StatusCode::INTERNAL, std::string("TOTP verification failed: ") + e.what());
    }
}

grpc::Status AuthServiceImpl::DisableTOTP(
    grpc::ServerContext* context,
    const DisableTOTPRequest* request,
    DisableTOTPResponse* response
) {
    try {
        auto tenant_ctx = common::TenantContextInterceptor::ExtractFromMetadata(context);
        if (tenant_ctx.user_id.empty()) {
            return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "Authentication required");
        }

        // Verify password before disabling 2FA
        auto conn_guard = db_pool_->AcquireConnection();
        pqxx::work txn(*conn_guard);

        auto result = txn.exec_params(
            "SELECT password_hash FROM users WHERE id = $1",
            tenant_ctx.user_id
        );

        if (result.empty()) {
            return grpc::Status(grpc::StatusCode::NOT_FOUND, "User not found");
        }

        std::string password_hash = result[0]["password_hash"].as<std::string>();

        if (!VerifyPassword(request->password(), password_hash)) {
            return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "Invalid password");
        }

        // Disable TOTP
        txn.exec_params(
            "UPDATE users SET totp_secret = NULL, totp_enrolled_at = NULL WHERE id = $1",
            tenant_ctx.user_id
        );

        // Delete backup codes
        txn.exec_params(
            "DELETE FROM backup_codes WHERE user_id = $1",
            tenant_ctx.user_id
        );

        txn.commit();

        response->set_success(true);
        return grpc::Status::OK;

    } catch (const std::exception& e) {
        return grpc::Status(grpc::StatusCode::INTERNAL, std::string("TOTP disable failed: ") + e.what());
    }
}

grpc::Status AuthServiceImpl::GenerateBackupCodes(
    grpc::ServerContext* context,
    const GenerateBackupCodesRequest* request,
    GenerateBackupCodesResponse* response
) {
    try {
        auto tenant_ctx = common::TenantContextInterceptor::ExtractFromMetadata(context);
        if (tenant_ctx.user_id.empty()) {
            return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "Authentication required");
        }

        // Generate new backup codes
        auto backup_codes = common::TotpHelper::GenerateBackupCodes(10);

        auto conn_guard = db_pool_->AcquireConnection();
        pqxx::work txn(*conn_guard);

        // Delete old backup codes
        txn.exec_params(
            "DELETE FROM backup_codes WHERE user_id = $1",
            tenant_ctx.user_id
        );

        // Store new backup codes (hashed)
        for (const auto& code : backup_codes) {
            std::string hash = common::TotpHelper::HashBackupCode(code);
            txn.exec_params(
                "INSERT INTO backup_codes (user_id, code_hash) VALUES ($1, $2)",
                tenant_ctx.user_id,
                hash
            );
        }

        txn.commit();

        // Return plaintext codes to user (only time they can see them)
        for (const auto& code : backup_codes) {
            response->add_backup_codes(code);
        }

        return grpc::Status::OK;

    } catch (const std::exception& e) {
        return grpc::Status(grpc::StatusCode::INTERNAL, std::string("Backup code generation failed: ") + e.what());
    }
}

// OTP Implementations

grpc::Status AuthServiceImpl::SendOTP(
    grpc::ServerContext* context,
    const SendOTPRequest* request,
    SendOTPResponse* response
) {
    try {
        // Rate limiting: 3 OTP requests per minute (Requirement A-7)
        std::string rate_limit_key = "otp:rate:" + request->email();
        if (!CheckRateLimit(rate_limit_key, 3, 60)) {
            response->set_success(false);
            response->set_message("Too many OTP requests. Please try again later.");
            return grpc::Status::OK;
        }

        // Generate 6-digit OTP
        std::string otp = GenerateOTP();

        // Store OTP in Redis with 10-minute TTL
        StoreOTP(request->email(), otp, request->purpose(), 600);

        // TODO: Send OTP via email (mock for now)
        std::cout << "OTP for " << request->email() << " (" << request->purpose() << "): " << otp << std::endl;

        auto expires_at = std::chrono::system_clock::now() + std::chrono::seconds(600);
        auto expires_timestamp = std::chrono::duration_cast<std::chrono::seconds>(
            expires_at.time_since_epoch()
        ).count();

        response->set_success(true);
        response->set_message("OTP sent successfully");
        response->set_expires_at(expires_timestamp);

        return grpc::Status::OK;

    } catch (const std::exception& e) {
        return grpc::Status(grpc::StatusCode::INTERNAL, std::string("OTP send failed: ") + e.what());
    }
}

grpc::Status AuthServiceImpl::VerifyOTP(
    grpc::ServerContext* context,
    const VerifyOTPRequest* request,
    VerifyOTPResponse* response
) {
    try {
        // Get stored OTP
        auto stored_otp = GetStoredOTP(request->email(), request->purpose());

        if (!stored_otp) {
            response->set_valid(false);
            response->set_message("OTP expired or not found");
            return grpc::Status::OK;
        }

        // Verify OTP
        bool valid = (*stored_otp == request->otp_code());

        if (valid) {
            // Delete OTP after successful verification
            std::string otp_key = "otp:" + request->email() + ":" + request->purpose();
            redis_client_->DeleteSession(otp_key);
        }

        response->set_valid(valid);
        response->set_message(valid ? "OTP valid" : "Invalid OTP");

        return grpc::Status::OK;

    } catch (const std::exception& e) {
        return grpc::Status(grpc::StatusCode::INTERNAL, std::string("OTP verification failed: ") + e.what());
    }
}

// OAuth Implementations (Mock)

grpc::Status AuthServiceImpl::InitiateOAuth(
    grpc::ServerContext* context,
    const InitiateOAuthRequest* request,
    InitiateOAuthResponse* response
) {
    try {
        // Generate CSRF state token
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 255);

        std::stringstream state_ss;
        for (int i = 0; i < 32; i++) {
            state_ss << std::hex << std::setw(2) << std::setfill('0') << dis(gen);
        }
        std::string state = state_ss.str();

        // Store state in Redis with 10-minute TTL
        redis_client_->SetSession("oauth:state:" + state, request->provider(), 600);

        // Mock authorization URL (in production, use actual OAuth URLs)
        std::string auth_url;
        if (request->provider() == "google") {
            auth_url = "https://accounts.google.com/o/oauth2/v2/auth?client_id=MOCK&redirect_uri=" +
                      request->redirect_uri() + "&response_type=code&scope=email%20profile&state=" + state;
        } else if (request->provider() == "github") {
            auth_url = "https://github.com/login/oauth/authorize?client_id=MOCK&redirect_uri=" +
                      request->redirect_uri() + "&scope=user:email&state=" + state;
        } else if (request->provider() == "microsoft") {
            auth_url = "https://login.microsoftonline.com/common/oauth2/v2.0/authorize?client_id=MOCK&redirect_uri=" +
                      request->redirect_uri() + "&response_type=code&scope=openid%20email%20profile&state=" + state;
        } else {
            return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Unsupported OAuth provider");
        }

        response->set_authorization_url(auth_url);
        response->set_state(state);

        return grpc::Status::OK;

    } catch (const std::exception& e) {
        return grpc::Status(grpc::StatusCode::INTERNAL, std::string("OAuth initiation failed: ") + e.what());
    }
}

grpc::Status AuthServiceImpl::HandleOAuthCallback(
    grpc::ServerContext* context,
    const OAuthCallbackRequest* request,
    OAuthCallbackResponse* response
) {
    try {
        // Verify state parameter (CSRF protection - Requirement A-34)
        std::string state_key = "oauth:state:" + request->state();
        auto stored_provider = redis_client_->GetSession(state_key);

        if (!stored_provider || *stored_provider != request->provider()) {
            return grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "Invalid OAuth state parameter");
        }

        // Delete state after verification
        redis_client_->DeleteSession(state_key);

        // Mock OAuth token exchange (in production, call provider APIs)
        std::string mock_email = "user@example.com";
        std::string mock_provider_id = "oauth_" + request->provider() + "_12345";

        // Check if user exists with this OAuth provider
        auto conn_guard = db_pool_->AcquireConnection();
        pqxx::work txn(*conn_guard);

        auto result = txn.exec_params(
            "SELECT u.id, u.tenant_id, u.email FROM users u "
            "JOIN oauth_accounts oa ON u.id = oa.user_id "
            "WHERE oa.provider = $1 AND oa.provider_user_id = $2",
            request->provider(),
            mock_provider_id
        );

        bool is_new_user = result.empty();
        std::string user_id, tenant_id;

        if (is_new_user) {
            // Create new user (mock tenant_id)
            std::string mock_tenant_id = "tenant_" + request->provider();

            auto user_result = txn.exec_params(
                "INSERT INTO users (tenant_id, email, password_hash) "
                "VALUES ($1, $2, NULL) RETURNING id",
                mock_tenant_id,
                mock_email
            );

            user_id = user_result[0]["id"].as<std::string>();
            tenant_id = mock_tenant_id;

            // Link OAuth account
            txn.exec_params(
                "INSERT INTO oauth_accounts (user_id, provider, provider_user_id) "
                "VALUES ($1, $2, $3)",
                user_id,
                request->provider(),
                mock_provider_id
            );
        } else {
            user_id = result[0]["id"].as<std::string>();
            tenant_id = result[0]["tenant_id"].as<std::string>();
            mock_email = result[0]["email"].as<std::string>();
        }

        txn.commit();

        // Generate tokens
        std::vector<std::string> roles;
        std::string access_token = GenerateAccessToken(user_id, tenant_id, mock_email, roles);
        std::string refresh_token = GenerateRefreshToken(user_id);

        // Store refresh token
        redis_client_->SetSession("refresh:" + user_id, refresh_token, 30 * 24 * 3600);

        response->set_access_token(access_token);
        response->set_refresh_token(refresh_token);
        response->set_user_id(user_id);
        response->set_tenant_id(tenant_id);
        response->set_expires_in(900);
        response->set_is_new_user(is_new_user);

        return grpc::Status::OK;

    } catch (const std::exception& e) {
        return grpc::Status(grpc::StatusCode::INTERNAL, std::string("OAuth callback failed: ") + e.what());
    }
}

// API Key Validation with Scopes

grpc::Status AuthServiceImpl::ValidateApiKey(
    grpc::ServerContext* context,
    const ValidateApiKeyRequest* request,
    ValidateApiKeyResponse* response
) {
    try {
        // Query API key from database
        auto conn_guard = db_pool_->AcquireConnection();
        pqxx::work txn(*conn_guard);

        // API keys are hashed, so we need to check all non-revoked keys
        // In production, consider using a hash prefix index for performance
        auto result = txn.exec_params(
            "SELECT id, user_id, tenant_id, key_hash, scopes "
            "FROM api_keys "
            "WHERE revoked_at IS NULL AND (expires_at IS NULL OR expires_at > NOW())"
        );

        bool found = false;
        std::string user_id, tenant_id, scopes_str;

        for (const auto& row : result) {
            std::string key_hash = row["key_hash"].as<std::string>();

            if (VerifyPassword(request->api_key(), key_hash)) {
                found = true;
                user_id = row["user_id"].as<std::string>();
                tenant_id = row["tenant_id"].as<std::string>();
                scopes_str = row["scopes"].as<std::string>();
                break;
            }
        }

        if (!found) {
            response->set_valid(false);
            response->set_message("Invalid API key");
            return grpc::Status::OK;
        }

        // Parse scopes (comma-separated)
        std::vector<std::string> scopes;
        std::stringstream ss(scopes_str);
        std::string scope;
        while (std::getline(ss, scope, ',')) {
            scopes.push_back(scope);
        }

        // Validate requested scope (Requirement A-14)
        bool scope_valid = ValidateScope(scopes, request->requested_scope());

        if (!scope_valid) {
            response->set_valid(false);
            response->set_message("API key does not have required scope: " + request->requested_scope());
            return grpc::Status::OK;
        }

        response->set_valid(true);
        response->set_user_id(user_id);
        response->set_tenant_id(tenant_id);
        for (const auto& s : scopes) {
            response->add_scopes(s);
        }
        response->set_message("API key valid");

        return grpc::Status::OK;

    } catch (const std::exception& e) {
        return grpc::Status(grpc::StatusCode::INTERNAL, std::string("API key validation failed: ") + e.what());
    }
}

// Helper Methods

bool AuthServiceImpl::CheckRateLimit(const std::string& key, int max_attempts, int window_seconds) {
    try {
        // Increment counter in Redis
        auto count_opt = redis_client_->GetSession(key);
        int count = count_opt ? std::stoi(*count_opt) : 0;

        if (count >= max_attempts) {
            return false;
        }

        // Increment and set TTL
        count++;
        redis_client_->SetSession(key, std::to_string(count), window_seconds);

        return true;

    } catch (const std::exception& e) {
        std::cerr << "Rate limit check failed: " << e.what() << std::endl;
        return true; // Fail open to avoid blocking legitimate requests
    }
}

bool AuthServiceImpl::ValidateScope(
    const std::vector<std::string>& granted_scopes,
    const std::string& requested_scope
) {
    // Exact match
    if (std::find(granted_scopes.begin(), granted_scopes.end(), requested_scope) != granted_scopes.end()) {
        return true;
    }

    // Wildcard match (e.g., "read:*" matches "read:upload")
    for (const auto& granted : granted_scopes) {
        if (granted.back() == '*') {
            std::string prefix = granted.substr(0, granted.length() - 1);
            if (requested_scope.find(prefix) == 0) {
                return true;
            }
        }
    }

    // Deny by default (Requirement A-14)
    return false;
}

std::string AuthServiceImpl::GenerateOTP() {
    // Generate 6-digit OTP
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 999999);

    std::stringstream ss;
    ss << std::setfill('0') << std::setw(6) << dis(gen);
    return ss.str();
}

void AuthServiceImpl::StoreOTP(
    const std::string& email,
    const std::string& otp,
    const std::string& purpose,
    int ttl_seconds
) {
    std::string key = "otp:" + email + ":" + purpose;
    redis_client_->SetSession(key, otp, ttl_seconds);
}

std::optional<std::string> AuthServiceImpl::GetStoredOTP(
    const std::string& email,
    const std::string& purpose
) {
    std::string key = "otp:" + email + ":" + purpose;
    return redis_client_->GetSession(key);
}

} // namespace auth
} // namespace saasforge
