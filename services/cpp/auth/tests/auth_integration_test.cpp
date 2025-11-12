#include <gtest/gtest.h>
#include "auth/auth_service.h"
#include "common/redis_client.h"
#include "common/db_pool.h"
#include "common/password_hasher.h"
#include <memory>
#include <chrono>
#include <thread>

namespace saasforge {
namespace auth {
namespace integration {

// Integration test fixture with real DB and Redis connections
class AuthIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Connect to test database
        db_url_ = "postgresql://test:test@localhost:5433/test_saasforge";
        db_pool_ = std::make_shared<common::DbPool>(db_url_, 10);

        // Connect to test Redis
        redis_url_ = "tcp://localhost:6380";
        redis_client_ = std::make_shared<common::RedisClient>(redis_url_);

        // Load JWT keys from test files
        jwt_public_key_ = LoadTestKey("test_rsa.pub");
        jwt_private_key_ = LoadTestKey("test_rsa.key");

        // Initialize service
        service_ = std::make_unique<AuthServiceImpl>(
            redis_client_,
            db_pool_,
            jwt_public_key_,
            jwt_private_key_
        );

        // Test tenant and user IDs from test_schema.sql fixtures
        test_tenant_id_ = "00000000-0000-0000-0000-000000000001";
        test_user_id_ = "10000000-0000-0000-0000-000000000001";
        test_email_ = "test@example.com";
        test_password_ = "test_password_123";
    }

    void TearDown() override {
        // Cleanup Redis keys created during tests
        redis_client_->DeleteSession("refresh:" + test_user_id_);
    }

    std::string LoadTestKey(const std::string& filename) {
        // For testing, use mock keys
        if (filename.find(".pub") != std::string::npos) {
            return R"(-----BEGIN PUBLIC KEY-----
MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAu1SU1LfVLPHCozMxH2Mo
4lgOEePzNm0tRgeLezV6ffAt0gunVTLw7onLRnrq0/IzW7yWR7QkrmBL7jTKEn5u
+qKhbwKfBstIs+bMY2Zkp18gnTxKLxoS2tFczGkPLPgizskuemMghRniWaoLcyeh
kd3qqGElvW/VDL5AaWTg0nLVkjRo9z+40RQzuVaE8AkAFmxZzow3x+VJYKdjykkJ
0iT9wCS0DRTXu269V264Vf/3jvredZiKRkgwlL9xNAwxXFg0x/XFw005UWVRIkdg
cKWTjpBP2dPwVZ4WWC+9aGVd+Gyn1o0CLelf4rEjGoXbAAEgAqeGUxrcIlbjXfbc
mwIDAQAB
-----END PUBLIC KEY-----)";
        } else {
            return R"(-----BEGIN PRIVATE KEY-----
MIIEvwIBADANBgkqhkiG9w0BAQEFAASCBKkwggSlAgEAAoIBAQC7VJTUt9Us8cKj
MzEfYyjiWA4R4/M2bS1GB4t7NXp98C3SC6dVMvDuictGeurT8jNbvJZHtCSuYEvu
NMoSfm76oqFvAp8Gy0iz5sxjZmSnXyCdPEovGhLa0VzMaQ8s+CLOyS56YyCFGeJZ
qgtzJ6GR3eqoYSW9b9UMvkBpZODSctWSNGj3P7jRFDO5VoTwCQAWbFnOjDfH5Ulg
p2PKSQnSJP3AJLQNFNe7br1XbrhV//eO+t51mIpGSDCUv3E0DDFcWDTH9cXDTTlR
ZVEiR2BwpZOOkE/Z0/BVnhZYL71oZV34bKfWjQIt6V/isSMahdsAASACp4ZTGtwi
VuNd9tybAgMBAAECggEBAKTmjaS6tkK8BlPXClTQ2vpz/N6uxDeS35mXpqasqskV
laAidgg/sWqpjXDbXr93otIMLlWsM+X0CqMDgSXKejLS2jx4GDjI1ZTXg++0AMJ8
sJ74pWzVDOfmCEQ/7wXs3+cbnXhKriO8Z036q92Qc1+N87SI38nkGa0ABH9CN83H
mQqt4fB7UdHzuIRe/me2PGhIq5ZBzj6h3BpoPGzEP+x3l9YmK8t/1cN0pqI+dQwY
dgfGjackLu/2qH80MCF7IyQaseZUOJyKrCLtSD/Iixv/hzDEUPfOCjFDgTpzf3cw
ta8+oE4wHCo1iI1/4TlPkwmXx4qSXtmw4aQPz7IDQvECgYEA8KNThCO2gsC2I9PQ
DM/8Cw0O983WCDY+oi+7JPiNAJwv5DYBqEZB1QYdj06YD16XlC/HAZMsMku1na2T
N0driwenQQWzoev3g2S7gRDoS/FCJSI3jJ+kjgtaA7Qmzlgk1TxODN+G1H91HW7t
0l7VnL27IWyYo2qRRK3jzxqUiPUCgYEAx0oQs2reBQGMVZnApD1jeq7n4MvNLcPv
t8b/eU9iUv6Y4Mj0Suo/AU8lYZXm8ubbqAlwz2VSVunD2tOplHyMUrtCtObAfVDU
AhCndKaA9gApgfb3xw1IKbuQ1u4IF1FJl3VtumfQn//LiH1B3rXhcdyo3/vIttEk
48RakUKClU8CgYEAzV7W3COOlDDcQd935DdtKBFRAPRPAlspQUnzMi5eSHMD/ISL
DY5IiQHbIH83D4bvXq0X7qQoSBSNP7Dvv3HYuqMhf0DaegrlBuJllFVVq9qPVRnK
xt1Il2HgxOBvbhOT+9in1BzA+YJ99UzC85O0Qz06A+CmtHEy4aZ2kj5hHjECgYEA
mNS4+A8Fkss8Js1RieK2LniBxMgmYml3pfVLKGnzmng7H2+cwPLhPIzIuwytXywh
2bzbsYEfYx3EoEVgMEpPhoarQnYPukrJO4gwE2o5Te6T5mJSZGlQJQj9q4ZB2Dfz
et6INsK0oG8XVGXSpQvQh3RUYekCZQkBBFcpqWpbIEsCgYAnM3DQf3FJoSnXaMhr
VBIovic5l0xFkEHskAjFTevO86Fsz1C2aSeRKSqGFoOQ0tmJzBEs1R6KqnHInicD
TQrKhArgLXX4v3CddjfTRJkFWDbE/CkvKZNOrcf1nhaGCPspRJj2KUkj1Fhl9Cnc
dn/RsYEONbwQSjIfMPkvxF+8HQ==
-----END PRIVATE KEY-----)";
        }
    }

    std::shared_ptr<common::RedisClient> redis_client_;
    std::shared_ptr<common::DbPool> db_pool_;
    std::unique_ptr<AuthServiceImpl> service_;
    std::string jwt_public_key_;
    std::string jwt_private_key_;
    std::string db_url_;
    std::string redis_url_;
    std::string test_tenant_id_;
    std::string test_user_id_;
    std::string test_email_;
    std::string test_password_;
};

// Test: Login with valid credentials returns access and refresh tokens
TEST_F(AuthIntegrationTest, LoginWithValidCredentialsReturnsTokens) {
    // Arrange
    LoginRequest request;
    request.set_email(test_email_);
    request.set_password(test_password_);

    LoginResponse response;
    grpc::ServerContext context;

    // Act
    grpc::Status status = service_->Login(&context, &request, &response);

    // Assert
    EXPECT_TRUE(status.ok()) << "Login should succeed: " << status.error_message();
    EXPECT_FALSE(response.access_token().empty()) << "Access token should not be empty";
    EXPECT_FALSE(response.refresh_token().empty()) << "Refresh token should not be empty";
    EXPECT_EQ(response.expires_in(), 900) << "Access token should expire in 15 minutes";

    // Verify refresh token stored in Redis
    auto stored_token = redis_client_->GetSession("refresh:" + test_user_id_);
    EXPECT_TRUE(stored_token.has_value()) << "Refresh token should be stored in Redis";
    EXPECT_EQ(*stored_token, response.refresh_token());
}

// Test: Login with invalid email fails
TEST_F(AuthIntegrationTest, LoginWithInvalidEmailFails) {
    // Arrange
    LoginRequest request;
    request.set_email("nonexistent@example.com");
    request.set_password("any_password");

    LoginResponse response;
    grpc::ServerContext context;

    // Act
    grpc::Status status = service_->Login(&context, &request, &response);

    // Assert
    EXPECT_FALSE(status.ok()) << "Login should fail for nonexistent email";
    EXPECT_EQ(status.error_code(), grpc::StatusCode::UNAUTHENTICATED);
    EXPECT_EQ(status.error_message(), "Invalid credentials");
}

// Test: Login with invalid password fails
TEST_F(AuthIntegrationTest, LoginWithInvalidPasswordFails) {
    // Arrange
    LoginRequest request;
    request.set_email(test_email_);
    request.set_password("wrong_password");

    LoginResponse response;
    grpc::ServerContext context;

    // Act
    grpc::Status status = service_->Login(&context, &request, &response);

    // Assert
    EXPECT_FALSE(status.ok()) << "Login should fail for wrong password";
    EXPECT_EQ(status.error_code(), grpc::StatusCode::UNAUTHENTICATED);
    EXPECT_EQ(status.error_message(), "Invalid credentials");
}

// Test: Login with deleted user account fails
TEST_F(AuthIntegrationTest, LoginWithDeletedAccountFails) {
    // Arrange
    LoginRequest request;
    request.set_email("deleted@example.com");
    request.set_password(test_password_);

    LoginResponse response;
    grpc::ServerContext context;

    // Act
    grpc::Status status = service_->Login(&context, &request, &response);

    // Assert
    EXPECT_FALSE(status.ok()) << "Login should fail for deleted account";
    EXPECT_EQ(status.error_code(), grpc::StatusCode::UNAUTHENTICATED);
}

// Test: Logout removes refresh token from Redis
TEST_F(AuthIntegrationTest, LogoutRemovesRefreshToken) {
    // Arrange - First login to get refresh token
    LoginRequest login_req;
    login_req.set_email(test_email_);
    login_req.set_password(test_password_);
    LoginResponse login_resp;
    grpc::ServerContext login_ctx;

    grpc::Status login_status = service_->Login(&login_ctx, &login_req, &login_resp);
    ASSERT_TRUE(login_status.ok());
    std::string refresh_token = login_resp.refresh_token();

    // Verify token is in Redis
    auto stored = redis_client_->GetSession("refresh:" + test_user_id_);
    ASSERT_TRUE(stored.has_value());

    // Act - Logout
    LogoutRequest logout_req;
    logout_req.set_refresh_token(refresh_token);
    LogoutResponse logout_resp;
    grpc::ServerContext logout_ctx;

    grpc::Status logout_status = service_->Logout(&logout_ctx, &logout_req, &logout_resp);

    // Assert
    EXPECT_TRUE(logout_status.ok()) << "Logout should succeed";
    EXPECT_TRUE(logout_resp.success());

    // Verify token removed from Redis
    auto after_logout = redis_client_->GetSession("refresh:" + test_user_id_);
    EXPECT_FALSE(after_logout.has_value()) << "Refresh token should be removed from Redis";
}

// Test: ValidateToken accepts valid JWT
TEST_F(AuthIntegrationTest, ValidateTokenAcceptsValidJWT) {
    // Arrange - Login to get valid token
    LoginRequest login_req;
    login_req.set_email(test_email_);
    login_req.set_password(test_password_);
    LoginResponse login_resp;
    grpc::ServerContext login_ctx;

    grpc::Status login_status = service_->Login(&login_ctx, &login_req, &login_resp);
    ASSERT_TRUE(login_status.ok());

    // Act - Validate token
    ValidateTokenRequest validate_req;
    validate_req.set_access_token(login_resp.access_token());
    ValidateTokenResponse validate_resp;
    grpc::ServerContext validate_ctx;

    grpc::Status validate_status = service_->ValidateToken(&validate_ctx, &validate_req, &validate_resp);

    // Assert
    EXPECT_TRUE(validate_status.ok()) << "Validation should succeed";
    EXPECT_TRUE(validate_resp.valid()) << "Token should be valid";
    EXPECT_EQ(validate_resp.user_id(), test_user_id_);
    EXPECT_EQ(validate_resp.tenant_id(), test_tenant_id_);
}

// Test: ValidateToken rejects expired/invalid JWT
TEST_F(AuthIntegrationTest, ValidateTokenRejectsInvalidJWT) {
    // Arrange - Create invalid token
    ValidateTokenRequest validate_req;
    validate_req.set_access_token("invalid.jwt.token");
    ValidateTokenResponse validate_resp;
    grpc::ServerContext validate_ctx;

    // Act
    grpc::Status validate_status = service_->ValidateToken(&validate_ctx, &validate_req, &validate_resp);

    // Assert
    EXPECT_TRUE(validate_status.ok()) << "Call should succeed but token should be invalid";
    EXPECT_FALSE(validate_resp.valid()) << "Invalid token should be rejected";
}

// Test: CreateApiKey generates valid key and stores in database
TEST_F(AuthIntegrationTest, CreateApiKeyGeneratesValidKey) {
    // Arrange - First login to get JWT
    LoginRequest login_req;
    login_req.set_email(test_email_);
    login_req.set_password(test_password_);
    LoginResponse login_resp;
    grpc::ServerContext login_ctx;

    grpc::Status login_status = service_->Login(&login_ctx, &login_req, &login_resp);
    ASSERT_TRUE(login_status.ok());

    // Setup context with tenant metadata
    CreateApiKeyRequest api_key_req;
    api_key_req.set_name("Test Integration API Key");
    api_key_req.add_scopes("read");
    api_key_req.add_scopes("write");

    CreateApiKeyResponse api_key_resp;
    grpc::ServerContext api_key_ctx;

    // Add tenant context to metadata
    api_key_ctx.AddInitialMetadata("x-tenant-id", test_tenant_id_);
    api_key_ctx.AddInitialMetadata("x-user-id", test_user_id_);

    // Act
    grpc::Status api_key_status = service_->CreateApiKey(&api_key_ctx, &api_key_req, &api_key_resp);

    // Assert
    EXPECT_TRUE(api_key_status.ok()) << "CreateApiKey should succeed: " << api_key_status.error_message();
    EXPECT_FALSE(api_key_resp.api_key().empty()) << "API key should not be empty";
    EXPECT_TRUE(api_key_resp.api_key().find("sk_") == 0) << "API key should start with 'sk_'";
    EXPECT_FALSE(api_key_resp.key_id().empty()) << "Key ID should not be empty";

    // Verify key stored in database
    auto conn_guard = db_pool_->AcquireConnection();
    pqxx::work txn(*conn_guard);
    auto result = txn.exec_params(
        "SELECT name, scopes FROM api_keys WHERE id = $1 AND tenant_id = $2",
        api_key_resp.key_id(),
        test_tenant_id_
    );

    ASSERT_FALSE(result.empty()) << "API key should be stored in database";
    EXPECT_EQ(result[0]["name"].as<std::string>(), "Test Integration API Key");
    EXPECT_EQ(result[0]["scopes"].as<std::string>(), "read,write");
}

// Test: RevokeApiKey soft-deletes key in database
TEST_F(AuthIntegrationTest, RevokeApiKeySoftDeletesKey) {
    // Arrange - Use pre-populated test API key from test_schema.sql
    std::string test_key_id = "20000000-0000-0000-0000-000000000001";

    RevokeApiKeyRequest revoke_req;
    revoke_req.set_key_id(test_key_id);

    RevokeApiKeyResponse revoke_resp;
    grpc::ServerContext revoke_ctx;

    // Add tenant context to metadata
    revoke_ctx.AddInitialMetadata("x-tenant-id", test_tenant_id_);
    revoke_ctx.AddInitialMetadata("x-user-id", test_user_id_);

    // Act
    grpc::Status revoke_status = service_->RevokeApiKey(&revoke_ctx, &revoke_req, &revoke_resp);

    // Assert
    EXPECT_TRUE(revoke_status.ok()) << "RevokeApiKey should succeed: " << revoke_status.error_message();
    EXPECT_TRUE(revoke_resp.success());

    // Verify key soft-deleted in database
    auto conn_guard = db_pool_->AcquireConnection();
    pqxx::work txn(*conn_guard);
    auto result = txn.exec_params(
        "SELECT revoked_at FROM api_keys WHERE id = $1 AND tenant_id = $2",
        test_key_id,
        test_tenant_id_
    );

    ASSERT_FALSE(result.empty()) << "API key should still exist in database";
    EXPECT_FALSE(result[0]["revoked_at"].is_null()) << "revoked_at should be set";
}

// Performance Test: Password hashing should complete in reasonable time
TEST_F(AuthIntegrationTest, PasswordHashingPerformance) {
    const int iterations = 5;
    std::vector<long long> durations;

    for (int i = 0; i < iterations; i++) {
        auto start = std::chrono::high_resolution_clock::now();

        std::string hash = common::PasswordHasher::HashPassword("test_password_" + std::to_string(i));

        auto end = std::chrono::high_resolution_clock::now();
        auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        durations.push_back(duration_ms);

        EXPECT_FALSE(hash.empty());
    }

    // Calculate average
    long long total = 0;
    for (auto d : durations) total += d;
    long long avg = total / iterations;

    std::cout << "Password hashing performance:" << std::endl;
    std::cout << "  Average: " << avg << "ms" << std::endl;
    std::cout << "  Target: <500ms (OWASP recommendation: 200-500ms)" << std::endl;

    // Argon2id should take ~200ms with our parameters
    EXPECT_LT(avg, 500) << "Password hashing should complete in <500ms";
}

// Performance Test: JWT validation should meet NFR-P1a (≤2ms p95)
TEST_F(AuthIntegrationTest, JWTValidationPerformance) {
    // Arrange - Get valid token
    LoginRequest login_req;
    login_req.set_email(test_email_);
    login_req.set_password(test_password_);
    LoginResponse login_resp;
    grpc::ServerContext login_ctx;

    grpc::Status login_status = service_->Login(&login_ctx, &login_req, &login_resp);
    ASSERT_TRUE(login_status.ok());
    std::string token = login_resp.access_token();

    // Warm up
    for (int i = 0; i < 10; i++) {
        ValidateTokenRequest req;
        req.set_access_token(token);
        ValidateTokenResponse resp;
        grpc::ServerContext ctx;
        service_->ValidateToken(&ctx, &req, &resp);
    }

    // Benchmark
    const int iterations = 100;
    std::vector<long long> durations;

    for (int i = 0; i < iterations; i++) {
        ValidateTokenRequest req;
        req.set_access_token(token);
        ValidateTokenResponse resp;
        grpc::ServerContext ctx;

        auto start = std::chrono::high_resolution_clock::now();
        grpc::Status status = service_->ValidateToken(&ctx, &req, &resp);
        auto end = std::chrono::high_resolution_clock::now();

        EXPECT_TRUE(status.ok());
        EXPECT_TRUE(resp.valid());

        auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        durations.push_back(duration_us);
    }

    // Calculate percentiles
    std::sort(durations.begin(), durations.end());
    long long p50 = durations[iterations * 50 / 100];
    long long p95 = durations[iterations * 95 / 100];
    long long p99 = durations[iterations * 99 / 100];

    std::cout << "JWT validation performance:" << std::endl;
    std::cout << "  p50: " << p50 << "μs (" << (p50 / 1000.0) << "ms)" << std::endl;
    std::cout << "  p95: " << p95 << "μs (" << (p95 / 1000.0) << "ms)" << std::endl;
    std::cout << "  p99: " << p99 << "μs (" << (p99 / 1000.0) << "ms)" << std::endl;
    std::cout << "  Target (NFR-P1a): ≤2ms p95" << std::endl;

    // NFR-P1a: JWT validation should be ≤2ms at p95
    EXPECT_LE(p95, 2000) << "JWT validation should meet NFR-P1a (≤2ms p95)";
}

} // namespace integration
} // namespace auth
} // namespace saasforge

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
