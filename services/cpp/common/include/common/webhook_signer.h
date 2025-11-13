/**
 * @author Heinstein F (@heinsteinh)
 * @created 2025-11-13
 * @description Webhook payload signing with HMAC-SHA256 (Requirement D-103)
 */

#pragma once

#include <string>

namespace saasforge {
namespace common {

/**
 * Webhook payload signer for security and authenticity verification
 *
 * SRS D-103: "The system shall sign webhook payloads using HMAC-SHA256
 * with tenant-specific secret for verification."
 *
 * Usage:
 *   std::string signature = WebhookSigner::SignPayload(payload, secret);
 *   // Send in HTTP header: X-Webhook-Signature: sha256={signature}
 */
class WebhookSigner {
public:
    /**
     * Sign webhook payload using HMAC-SHA256
     *
     * @param payload JSON payload to sign
     * @param secret Webhook secret (tenant-specific in production)
     * @return Hex-encoded HMAC-SHA256 signature
     */
    static std::string SignPayload(
        const std::string& payload,
        const std::string& secret
    );

    /**
     * Verify webhook signature (for webhook recipients)
     *
     * @param payload JSON payload received
     * @param signature Signature from X-Webhook-Signature header
     * @param secret Webhook secret
     * @return True if signature is valid
     */
    static bool VerifySignature(
        const std::string& payload,
        const std::string& signature,
        const std::string& secret
    );

    /**
     * Get mock webhook secret for testing/development
     *
     * NOTE: In production, this should be retrieved from database
     * per webhook registration or tenant-specific secret store
     *
     * @param tenant_id Tenant ID
     * @param webhook_id Webhook registration ID
     * @return Mock secret (hardcoded for now)
     */
    static std::string GetMockWebhookSecret(
        const std::string& tenant_id,
        const std::string& webhook_id
    );

private:
    /**
     * Convert binary HMAC output to hex string
     */
    static std::string ToHex(const unsigned char* data, size_t length);
};

} // namespace common
} // namespace saasforge
