#include "common/jwt_validator.h"
#include <stdexcept>

namespace saasforge {
namespace common {

JwtValidator::JwtValidator(const std::string& public_key_pem)
    : verifier_(jwt::verify()
        .allow_algorithm(jwt::algorithm::rs256{public_key_pem})
        .with_issuer("saasforge")) {
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
    // TODO: Implement Redis blacklist check
    return false;
}

} // namespace common
} // namespace saasforge
