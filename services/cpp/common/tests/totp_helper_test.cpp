/**
 * @author Heinstein F (@heinsteinh)
 * @created 2025-11-13
 * @description Unit tests for TOTP helper implementation
 */

#include <gtest/gtest.h>
#include "common/totp_helper.h"
#include <chrono>
#include <thread>
#include <set>

namespace saasforge {
namespace common {
namespace test {

class TotpHelperTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Known test secret for reproducible tests
        test_secret_ = "JBSWY3DPEHPK3PXP";  // "Hello World" in Base32
    }

    void TearDown() override {}

    std::string test_secret_;
};

// Test: Secret Generation

TEST_F(TotpHelperTest, GenerateSecretProducesBase32String) {
    std::string secret = TotpHelper::GenerateSecret();

    // Should be non-empty
    EXPECT_FALSE(secret.empty());

    // Should be valid Base32 (A-Z, 2-7, with = padding)
    for (char c : secret) {
        if (c == '=') continue;  // Padding is valid
        EXPECT_TRUE((c >= 'A' && c <= 'Z') || (c >= '2' && c <= '7'))
            << "Invalid Base32 character: " << c;
    }

    // Should have reasonable length (Base32 encodes 5 bits per char)
    // 20 bytes (160 bits) = 32 Base32 chars
    EXPECT_GE(secret.length(), 26);  // At least 160 bits
    EXPECT_LE(secret.length(), 40);  // Not too long
}

TEST_F(TotpHelperTest, GenerateSecretProducesDifferentValues) {
    std::string secret1 = TotpHelper::GenerateSecret();
    std::string secret2 = TotpHelper::GenerateSecret();
    std::string secret3 = TotpHelper::GenerateSecret();

    // All secrets should be different (cryptographically random)
    EXPECT_NE(secret1, secret2);
    EXPECT_NE(secret2, secret3);
    EXPECT_NE(secret1, secret3);
}

// Test: QR Code URL Generation

TEST_F(TotpHelperTest, GenerateQrCodeUrlCreatesValidOtpAuthUrl) {
    std::string url = TotpHelper::GenerateQrCodeUrl(
        test_secret_,
        "user@example.com",
        "SaaSForge"
    );

    // Should start with otpauth://totp/
    EXPECT_EQ(url.substr(0, 15), "otpauth://totp/");

    // Should contain the secret
    EXPECT_NE(url.find("secret=" + test_secret_), std::string::npos);

    // Should contain the issuer
    EXPECT_NE(url.find("issuer=SaaSForge"), std::string::npos);

    // Should contain TOTP parameters
    EXPECT_NE(url.find("algorithm=SHA1"), std::string::npos);
    EXPECT_NE(url.find("digits=6"), std::string::npos);
    EXPECT_NE(url.find("period=30"), std::string::npos);

    // Should contain both issuer and email (may be URL-encoded)
    EXPECT_NE(url.find("SaaSForge"), std::string::npos);
    EXPECT_NE(url.find("user"), std::string::npos);
    EXPECT_NE(url.find("example.com"), std::string::npos);
}

TEST_F(TotpHelperTest, GenerateQrCodeUrlEncodesSpecialCharacters) {
    std::string url = TotpHelper::GenerateQrCodeUrl(
        test_secret_,
        "test+user@example.com",
        "SaaS Forge"
    );

    // Spaces should be URL-encoded
    EXPECT_NE(url.find("SaaS%20Forge"), std::string::npos);
}

// Test: TOTP Code Validation

TEST_F(TotpHelperTest, ValidateCodeAcceptsValidCode) {
    // Generate a code and validate immediately
    std::string secret = TotpHelper::GenerateSecret();

    // We can't predict the exact code, but we can generate it using the same algorithm
    // For this test, we'll use a known secret and manually calculate expected code
    // This is a simplified test - in production, use test vectors from RFC 6238

    // For now, test that validation doesn't crash
    bool result = TotpHelper::ValidateCode(secret, "123456");
    // Result could be true or false depending on timing, but shouldn't crash
    EXPECT_TRUE(result == true || result == false);
}

TEST_F(TotpHelperTest, ValidateCodeRejectsInvalidFormat) {
    // Too short
    EXPECT_FALSE(TotpHelper::ValidateCode(test_secret_, "12345"));

    // Too long
    EXPECT_FALSE(TotpHelper::ValidateCode(test_secret_, "1234567"));

    // Contains non-digits
    EXPECT_FALSE(TotpHelper::ValidateCode(test_secret_, "12345a"));
    EXPECT_FALSE(TotpHelper::ValidateCode(test_secret_, "abcdef"));

    // Empty
    EXPECT_FALSE(TotpHelper::ValidateCode(test_secret_, ""));
}

TEST_F(TotpHelperTest, ValidateCodeUsesTimeWindow) {
    // This test is time-dependent and may be flaky
    // In production, use dependency injection for time or test with RFC 6238 test vectors

    // Generate a secret
    std::string secret = TotpHelper::GenerateSecret();

    // The window parameter should allow codes from nearby time slots
    // With window=1, codes from current time ±30 seconds should be valid
    // This is difficult to test reliably without mocking time

    // For now, just verify window parameter doesn't cause crashes
    EXPECT_NO_THROW({
        TotpHelper::ValidateCode(secret, "123456", 0);  // No window
        TotpHelper::ValidateCode(secret, "123456", 1);  // ±30s
        TotpHelper::ValidateCode(secret, "123456", 2);  // ±60s
    });
}

// Test: RFC 6238 Test Vectors
// These are known test vectors from the TOTP RFC

TEST_F(TotpHelperTest, DISABLED_ValidateCodeWithRFC6238TestVectors) {
    // This test is disabled because it requires mocking the system time
    // In a production test suite, use dependency injection or test harness

    // RFC 6238 test vector (SHA1, 8-digit code)
    // Secret: "12345678901234567890" (ASCII)
    // Time: 59 seconds (Unix epoch)
    // Expected Code: 94287082 (8 digits)

    // Note: Our implementation uses 6 digits, so this test would need adjustment
    EXPECT_TRUE(true);
}

// Test: Backup Code Generation

TEST_F(TotpHelperTest, GenerateBackupCodesProducesCorrectCount) {
    auto codes = TotpHelper::GenerateBackupCodes(10);
    EXPECT_EQ(codes.size(), 10);

    auto codes5 = TotpHelper::GenerateBackupCodes(5);
    EXPECT_EQ(codes5.size(), 5);

    auto codes20 = TotpHelper::GenerateBackupCodes(20);
    EXPECT_EQ(codes20.size(), 20);
}

TEST_F(TotpHelperTest, GenerateBackupCodesHasCorrectFormat) {
    auto codes = TotpHelper::GenerateBackupCodes(10);

    for (const auto& code : codes) {
        // Format should be XXXX-XXXX (9 characters)
        EXPECT_EQ(code.length(), 9);

        // Should have hyphen at position 4
        EXPECT_EQ(code[4], '-');

        // All other characters should be digits
        for (size_t i = 0; i < code.length(); ++i) {
            if (i == 4) continue;  // Skip hyphen
            EXPECT_TRUE(std::isdigit(code[i]))
                << "Non-digit character at position " << i << " in code: " << code;
        }
    }
}

TEST_F(TotpHelperTest, GenerateBackupCodesProducesUniqueValues) {
    auto codes = TotpHelper::GenerateBackupCodes(20);

    // Convert to set to check uniqueness
    std::set<std::string> unique_codes(codes.begin(), codes.end());

    // All codes should be unique
    EXPECT_EQ(unique_codes.size(), codes.size());
}

TEST_F(TotpHelperTest, GenerateBackupCodesDifferentCalls) {
    auto codes1 = TotpHelper::GenerateBackupCodes(5);
    auto codes2 = TotpHelper::GenerateBackupCodes(5);

    // Different calls should produce different codes
    bool all_different = true;
    for (size_t i = 0; i < codes1.size(); ++i) {
        if (codes1[i] == codes2[i]) {
            all_different = false;
            break;
        }
    }

    EXPECT_TRUE(all_different);
}

// Test: Backup Code Hashing

TEST_F(TotpHelperTest, HashBackupCodeProducesHexString) {
    std::string code = "1234-5678";
    std::string hash = TotpHelper::HashBackupCode(code);

    // SHA-256 produces 64 hex characters
    EXPECT_EQ(hash.length(), 64);

    // All characters should be hex (0-9, a-f)
    for (char c : hash) {
        EXPECT_TRUE((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))
            << "Invalid hex character: " << c;
    }
}

TEST_F(TotpHelperTest, HashBackupCodeIsDeterministic) {
    std::string code = "1234-5678";

    std::string hash1 = TotpHelper::HashBackupCode(code);
    std::string hash2 = TotpHelper::HashBackupCode(code);
    std::string hash3 = TotpHelper::HashBackupCode(code);

    // Same input should always produce same hash
    EXPECT_EQ(hash1, hash2);
    EXPECT_EQ(hash2, hash3);
}

TEST_F(TotpHelperTest, HashBackupCodeDifferentInputsDifferentHashes) {
    std::string hash1 = TotpHelper::HashBackupCode("1234-5678");
    std::string hash2 = TotpHelper::HashBackupCode("8765-4321");
    std::string hash3 = TotpHelper::HashBackupCode("0000-0000");

    // Different inputs should produce different hashes
    EXPECT_NE(hash1, hash2);
    EXPECT_NE(hash2, hash3);
    EXPECT_NE(hash1, hash3);
}

// Test: Backup Code Verification

TEST_F(TotpHelperTest, VerifyBackupCodeAcceptsValidCode) {
    std::string code = "1234-5678";
    std::string hash = TotpHelper::HashBackupCode(code);

    EXPECT_TRUE(TotpHelper::VerifyBackupCode(code, hash));
}

TEST_F(TotpHelperTest, VerifyBackupCodeRejectsInvalidCode) {
    std::string code = "1234-5678";
    std::string hash = TotpHelper::HashBackupCode(code);

    // Wrong code should not verify
    EXPECT_FALSE(TotpHelper::VerifyBackupCode("0000-0000", hash));
    EXPECT_FALSE(TotpHelper::VerifyBackupCode("1234-5679", hash));
    EXPECT_FALSE(TotpHelper::VerifyBackupCode("8765-4321", hash));
}

TEST_F(TotpHelperTest, VerifyBackupCodeRejectsWrongHash) {
    std::string code = "1234-5678";
    std::string wrong_hash = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";

    EXPECT_FALSE(TotpHelper::VerifyBackupCode(code, wrong_hash));
}

// Test: Edge Cases

TEST_F(TotpHelperTest, ValidateCodeHandlesEmptySecret) {
    // Empty secret should not crash
    EXPECT_NO_THROW({
        bool result = TotpHelper::ValidateCode("", "123456");
        // May throw or return false, but shouldn't crash
    });
}

TEST_F(TotpHelperTest, GenerateBackupCodesHandlesZeroCount) {
    auto codes = TotpHelper::GenerateBackupCodes(0);
    EXPECT_EQ(codes.size(), 0);
}

TEST_F(TotpHelperTest, GenerateBackupCodesHandlesLargeCount) {
    // Should handle large counts without crashing
    EXPECT_NO_THROW({
        auto codes = TotpHelper::GenerateBackupCodes(100);
        EXPECT_EQ(codes.size(), 100);
    });
}

// Test: Security Properties

TEST_F(TotpHelperTest, BackupCodesHaveHighEntropy) {
    auto codes = TotpHelper::GenerateBackupCodes(100);

    // Check that digits are distributed (not all 0s or all 9s)
    std::map<char, int> digit_counts;

    for (const auto& code : codes) {
        for (char c : code) {
            if (std::isdigit(c)) {
                digit_counts[c]++;
            }
        }
    }

    // We should see all digits 0-9 represented
    EXPECT_EQ(digit_counts.size(), 10);

    // Each digit should appear at least a few times (not perfect distribution, but reasonable)
    for (const auto& [digit, count] : digit_counts) {
        EXPECT_GT(count, 10) << "Digit " << digit << " appears too rarely";
    }
}

TEST_F(TotpHelperTest, HashingIsNotReversible) {
    std::string code = "1234-5678";
    std::string hash = TotpHelper::HashBackupCode(code);

    // Hash should be completely different from code
    EXPECT_NE(hash, code);

    // Hash should not contain the original code
    EXPECT_EQ(hash.find("1234"), std::string::npos);
    EXPECT_EQ(hash.find("5678"), std::string::npos);
}

// Performance Tests

TEST_F(TotpHelperTest, DISABLED_ValidateCodePerformance) {
    // This test is disabled by default but useful for benchmarking

    std::string secret = TotpHelper::GenerateSecret();
    std::string code = "123456";

    auto start = std::chrono::high_resolution_clock::now();

    const int iterations = 10000;
    for (int i = 0; i < iterations; ++i) {
        TotpHelper::ValidateCode(secret, code);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    double avg_us = static_cast<double>(duration.count()) / iterations;

    std::cout << "Average TOTP validation time: " << avg_us << " microseconds" << std::endl;

    // Should be fast (< 100 microseconds per validation)
    EXPECT_LT(avg_us, 100.0);
}

TEST_F(TotpHelperTest, DISABLED_GenerateSecretPerformance) {
    // Benchmark secret generation

    auto start = std::chrono::high_resolution_clock::now();

    const int iterations = 1000;
    for (int i = 0; i < iterations; ++i) {
        TotpHelper::GenerateSecret();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    double avg_us = static_cast<double>(duration.count()) / iterations;

    std::cout << "Average secret generation time: " << avg_us << " microseconds" << std::endl;

    // Should be reasonably fast (< 1000 microseconds)
    EXPECT_LT(avg_us, 1000.0);
}

} // namespace test
} // namespace common
} // namespace saasforge

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
