/**
 * @author Heinstein F (@heinsteinh)
 * @created 2025-11-13
 * @description Webhook payload signing implementation
 */

#include "common/webhook_signer.h"
#include <openssl/hmac.h>
#include <openssl/sha.h>
#include <sstream>
#include <iomanip>
#include <cstring>

namespace saasforge {
namespace common {

std::string WebhookSigner::SignPayload(
    const std::string& payload,
    const std::string& secret
) {
    // Use HMAC-SHA256 to sign the payload
    unsigned char hmac_result[EVP_MAX_MD_SIZE];
    unsigned int hmac_len = 0;

    HMAC(
        EVP_sha256(),
        secret.c_str(),
        secret.length(),
        reinterpret_cast<const unsigned char*>(payload.c_str()),
        payload.length(),
        hmac_result,
        &hmac_len
    );

    // Convert to hex string
    return ToHex(hmac_result, hmac_len);
}

bool WebhookSigner::VerifySignature(
    const std::string& payload,
    const std::string& signature,
    const std::string& secret
) {
    // Generate expected signature
    std::string expected_signature = SignPayload(payload, secret);

    // Constant-time comparison to prevent timing attacks
    if (signature.length() != expected_signature.length()) {
        return false;
    }

    // Use OpenSSL's constant-time comparison
    return CRYPTO_memcmp(
        signature.c_str(),
        expected_signature.c_str(),
        signature.length()
    ) == 0;
}

std::string WebhookSigner::GetMockWebhookSecret(
    const std::string& tenant_id,
    const std::string& webhook_id
) {
    // MOCK IMPLEMENTATION - Hardcoded for development
    // TODO: In production, retrieve from database:
    //   SELECT secret FROM webhooks WHERE id = webhook_id AND tenant_id = tenant_id

    // For now, generate a deterministic secret based on IDs
    return "whsec_" + tenant_id + "_" + webhook_id + "_mock_secret_key";
}

std::string WebhookSigner::ToHex(const unsigned char* data, size_t length) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');

    for (size_t i = 0; i < length; ++i) {
        oss << std::setw(2) << static_cast<unsigned>(data[i]);
    }

    return oss.str();
}

} // namespace common
} // namespace saasforge
