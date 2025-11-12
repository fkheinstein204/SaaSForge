#include "common/jwt_validator.h"
#include <stdexcept>

namespace saasforge {
namespace common {

JwtValidator::JwtValidator(const std::string& public_key_pem, std::shared_ptr<RedisClient> redis_client)
    : verifier_(jwt::verify()
        .allow_algorithm(jwt::algorithm::rs256{public_key_pem})
        .with_issuer("saasforge")),
      redis_client_(redis_client) {
}

std::optional<TokenClaims> JwtValidator::Validate(const std::string& token) {
    try {
        auto decoded = jwt::decode(token);

        // Verify signature and claims
        verifier_.verify(decoded);

        // Extract claims
        TokenClaims claims;
        claims.user_id = decoded.get_payload_claim("sub").as_string();
        claims.tenant_id = decoded.get_payload_claim("tenant_id").as_string();
        claims.email = decoded.get_payload_claim("email").as_string();
        claims.jti = decoded.get_id();
        claims.exp = decoded.get_expires_at().time_since_epoch().count();
        claims.iat = decoded.get_issued_at().time_since_epoch().count();

        // Extract roles array if present (optional field)
        try {
            if (decoded.has_payload_claim("roles")) {
                auto roles_claim = decoded.get_payload_claim("roles");
                auto roles_json = roles_claim.to_json();
                if (roles_json.is<picojson::array>()) {
                    auto roles_array = roles_json.get<picojson::array>();
                    for (const auto& role : roles_array) {
                        if (role.is<std::string>()) {
                            claims.roles.push_back(role.get<std::string>());
                        }
                    }
                }
            }
        } catch (...) {
            // Roles claim is optional, ignore parse errors
        }

        // Check blacklist
        if (IsBlacklisted(claims.jti)) {
            return std::nullopt;
        }

        return claims;

    } catch (const std::exception& e) {
        // Invalid token
        return std::nullopt;
    }
}

bool JwtValidator::IsBlacklisted(const std::string& jti) {
    if (!redis_client_) {
        return false; // If no Redis client, skip blacklist check
    }
    return redis_client_->IsTokenBlacklisted(jti);
}

} // namespace common
} // namespace saasforge
