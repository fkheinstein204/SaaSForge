#pragma once

#include <string>
#include <memory>
#include <optional>
#include <jwt-cpp/jwt.h>
#include "redis_client.h"

namespace saasforge {
namespace common {

struct TokenClaims {
    std::string user_id;
    std::string tenant_id;
    std::string email;
    std::vector<std::string> roles;
    int64_t exp;
    int64_t iat;
    std::string jti;
};

class JwtValidator {
public:
    JwtValidator(const std::string& public_key_pem, std::shared_ptr<RedisClient> redis_client);

    // Validates JWT and returns claims if valid
    std::optional<TokenClaims> Validate(const std::string& token);

    // Check if token is blacklisted in Redis
    bool IsBlacklisted(const std::string& jti);

private:
    jwt::verifier<jwt::default_clock, jwt::traits::kazuho_picojson> verifier_;
    std::shared_ptr<RedisClient> redis_client_;
};

} // namespace common
} // namespace saasforge
