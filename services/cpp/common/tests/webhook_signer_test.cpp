/**
 * @author Heinstein F (@heinsteinh)
 * @created 2025-11-13
 * @description Tests for webhook payload signing (Requirement D-103)
 */

#include <gtest/gtest.h>
#include "common/webhook_signer.h"

using namespace saasforge::common;

class WebhookSignerTest : public ::testing::Test {
protected:
    const std::string test_payload = R"({"event":"subscription.created","data":{"id":"sub_123"}})";
    const std::string test_secret = "whsec_test_secret_key_12345";
};

// Test that signature generation is deterministic
TEST_F(WebhookSignerTest, SignatureIsDeterministic) {
    std::string sig1 = WebhookSigner::SignPayload(test_payload, test_secret);
    std::string sig2 = WebhookSigner::SignPayload(test_payload, test_secret);

    EXPECT_EQ(sig1, sig2);
    EXPECT_FALSE(sig1.empty());
    EXPECT_EQ(sig1.length(), 64);  // SHA-256 hex = 64 characters
}

// Test that different payloads produce different signatures
TEST_F(WebhookSignerTest, DifferentPayloadsDifferentSignatures) {
    std::string payload1 = R"({"event":"subscription.created"})";
    std::string payload2 = R"({"event":"subscription.deleted"})";

    std::string sig1 = WebhookSigner::SignPayload(payload1, test_secret);
    std::string sig2 = WebhookSigner::SignPayload(payload2, test_secret);

    EXPECT_NE(sig1, sig2);
}

// Test that different secrets produce different signatures
TEST_F(WebhookSignerTest, DifferentSecretsDifferentSignatures) {
    std::string secret1 = "whsec_secret_1";
    std::string secret2 = "whsec_secret_2";

    std::string sig1 = WebhookSigner::SignPayload(test_payload, secret1);
    std::string sig2 = WebhookSigner::SignPayload(test_payload, secret2);

    EXPECT_NE(sig1, sig2);
}

// Test signature verification with valid signature
TEST_F(WebhookSignerTest, VerifyValidSignature) {
    std::string signature = WebhookSigner::SignPayload(test_payload, test_secret);

    bool is_valid = WebhookSigner::VerifySignature(test_payload, signature, test_secret);

    EXPECT_TRUE(is_valid);
}

// Test signature verification rejects invalid signature
TEST_F(WebhookSignerTest, RejectInvalidSignature) {
    std::string signature = "invalid_signature_12345";

    bool is_valid = WebhookSigner::VerifySignature(test_payload, signature, test_secret);

    EXPECT_FALSE(is_valid);
}

// Test signature verification rejects modified payload
TEST_F(WebhookSignerTest, RejectModifiedPayload) {
    std::string signature = WebhookSigner::SignPayload(test_payload, test_secret);
    std::string modified_payload = test_payload + " ";  // Add space

    bool is_valid = WebhookSigner::VerifySignature(modified_payload, signature, test_secret);

    EXPECT_FALSE(is_valid);
}

// Test signature verification with wrong secret
TEST_F(WebhookSignerTest, RejectWrongSecret) {
    std::string signature = WebhookSigner::SignPayload(test_payload, test_secret);
    std::string wrong_secret = "whsec_wrong_secret";

    bool is_valid = WebhookSigner::VerifySignature(test_payload, signature, wrong_secret);

    EXPECT_FALSE(is_valid);
}

// Test empty payload handling
TEST_F(WebhookSignerTest, HandleEmptyPayload) {
    std::string empty_payload = "";

    std::string signature = WebhookSigner::SignPayload(empty_payload, test_secret);

    EXPECT_FALSE(signature.empty());
    EXPECT_EQ(signature.length(), 64);

    bool is_valid = WebhookSigner::VerifySignature(empty_payload, signature, test_secret);
    EXPECT_TRUE(is_valid);
}

// Test large payload handling
TEST_F(WebhookSignerTest, HandleLargePayload) {
    std::string large_payload(10000, 'x');  // 10KB payload

    std::string signature = WebhookSigner::SignPayload(large_payload, test_secret);

    EXPECT_FALSE(signature.empty());
    EXPECT_EQ(signature.length(), 64);

    bool is_valid = WebhookSigner::VerifySignature(large_payload, signature, test_secret);
    EXPECT_TRUE(is_valid);
}

// Test mock secret generation
TEST_F(WebhookSignerTest, MockSecretGeneration) {
    std::string tenant_id = "tenant_123";
    std::string webhook_id = "webhook_456";

    std::string secret = WebhookSigner::GetMockWebhookSecret(tenant_id, webhook_id);

    EXPECT_FALSE(secret.empty());
    EXPECT_TRUE(secret.find("whsec_") == 0);  // Should start with "whsec_"
    EXPECT_TRUE(secret.find(tenant_id) != std::string::npos);
    EXPECT_TRUE(secret.find(webhook_id) != std::string::npos);
}

// Test mock secret is deterministic
TEST_F(WebhookSignerTest, MockSecretIsDeterministic) {
    std::string tenant_id = "tenant_123";
    std::string webhook_id = "webhook_456";

    std::string secret1 = WebhookSigner::GetMockWebhookSecret(tenant_id, webhook_id);
    std::string secret2 = WebhookSigner::GetMockWebhookSecret(tenant_id, webhook_id);

    EXPECT_EQ(secret1, secret2);
}

// Test different tenants get different secrets
TEST_F(WebhookSignerTest, DifferentTenantsDifferentSecrets) {
    std::string webhook_id = "webhook_456";

    std::string secret1 = WebhookSigner::GetMockWebhookSecret("tenant_1", webhook_id);
    std::string secret2 = WebhookSigner::GetMockWebhookSecret("tenant_2", webhook_id);

    EXPECT_NE(secret1, secret2);
}

// Test different webhooks get different secrets
TEST_F(WebhookSignerTest, DifferentWebhooksDifferentSecrets) {
    std::string tenant_id = "tenant_123";

    std::string secret1 = WebhookSigner::GetMockWebhookSecret(tenant_id, "webhook_1");
    std::string secret2 = WebhookSigner::GetMockWebhookSecret(tenant_id, "webhook_2");

    EXPECT_NE(secret1, secret2);
}

// Test signature format is lowercase hex
TEST_F(WebhookSignerTest, SignatureIsLowercaseHex) {
    std::string signature = WebhookSigner::SignPayload(test_payload, test_secret);

    for (char c : signature) {
        EXPECT_TRUE((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'));
    }
}

// Test known test vector (from OpenSSL HMAC-SHA256)
TEST_F(WebhookSignerTest, KnownTestVector) {
    // This is a test vector using OpenSSL's HMAC-SHA256
    std::string payload = "Hello, World!";
    std::string secret = "secret";

    std::string signature = WebhookSigner::SignPayload(payload, secret);

    // Pre-computed HMAC-SHA256 of "Hello, World!" with secret "secret"
    // Verified with OpenSSL HMAC function
    std::string expected = "fcfaffa7fef86515c7beb6b62d779fa4ccf092f2e61c164376054271252821ff";

    EXPECT_EQ(signature, expected);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
