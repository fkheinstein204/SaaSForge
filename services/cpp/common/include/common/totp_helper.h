/**
 * @author Heinstein F (@heinsteinh)
 * @created 2025-11-13
 * @description TOTP (Time-based One-Time Password) helper for 2FA implementation
 *
 * Implements RFC 6238 (TOTP) for two-factor authentication.
 * Provides secret generation, code validation, backup code management.
 */

#pragma once

#include <string>
#include <vector>
#include <optional>
#include <cstdint>

namespace saasforge {
namespace common {

/**
 * TOTP Helper for Two-Factor Authentication
 *
 * Implements RFC 6238 compliant TOTP generation and validation.
 * Uses 30-second time windows with HMAC-SHA1 algorithm.
 */
class TotpHelper {
public:
    /**
     * Generate a new random TOTP secret (Base32-encoded)
     *
     * @return 32-character Base32-encoded secret (160 bits of entropy)
     */
    static std::string GenerateSecret();

    /**
     * Generate otpauth:// URL for QR code
     *
     * @param secret Base32-encoded secret
     * @param email User's email address
     * @param issuer Service name (e.g., "SaaSForge")
     * @return otpauth://totp/... URL for QR code generation
     */
    static std::string GenerateQrCodeUrl(
        const std::string& secret,
        const std::string& email,
        const std::string& issuer = "SaaSForge"
    );

    /**
     * Validate TOTP code with time window tolerance
     *
     * Validates code against current time ± window.
     * Window of 1 allows 30 seconds before/after current time (90s total).
     *
     * @param secret Base32-encoded secret
     * @param code 6-digit TOTP code from user
     * @param window Number of 30-second intervals to check (default: 1)
     * @return true if code is valid within window
     */
    static bool ValidateCode(
        const std::string& secret,
        const std::string& code,
        int window = 1
    );

    /**
     * Generate a set of backup codes
     *
     * Each code is 8 characters: XXXX-XXXX format.
     * Uses cryptographically secure random generation.
     *
     * @param count Number of backup codes to generate (default: 10)
     * @return Vector of backup codes
     */
    static std::vector<std::string> GenerateBackupCodes(int count = 10);

    /**
     * Hash a backup code for secure storage
     *
     * Uses SHA-256 hashing for backup code storage.
     *
     * @param code Plain backup code
     * @return SHA-256 hex hash of the code
     */
    static std::string HashBackupCode(const std::string& code);

    /**
     * Verify a backup code against its hash
     *
     * @param code Plain backup code from user
     * @param hash Stored SHA-256 hash
     * @return true if code matches hash
     */
    static bool VerifyBackupCode(const std::string& code, const std::string& hash);

private:
    /**
     * Generate TOTP code for a specific time counter
     *
     * @param secret Base32-encoded secret
     * @param time_counter Unix timestamp divided by time step (30s)
     * @return 6-digit TOTP code
     */
    static std::string GenerateTotpCode(const std::string& secret, uint64_t time_counter);

    /**
     * Decode Base32 string to raw bytes
     *
     * @param base32 Base32-encoded string
     * @return Decoded byte vector
     */
    static std::vector<uint8_t> DecodeBase32(const std::string& base32);

    /**
     * Encode raw bytes to Base32 string
     *
     * @param data Raw byte vector
     * @return Base32-encoded string
     */
    static std::string EncodeBase32(const std::vector<uint8_t>& data);

    /**
     * Compute HMAC-SHA1
     *
     * @param key Secret key bytes
     * @param message Message bytes
     * @return HMAC-SHA1 digest (20 bytes)
     */
    static std::vector<uint8_t> HmacSha1(
        const std::vector<uint8_t>& key,
        const std::vector<uint8_t>& message
    );

    /**
     * URL-encode a string
     *
     * @param str String to encode
     * @return URL-encoded string
     */
    static std::string UrlEncode(const std::string& str);

    // Time step in seconds (RFC 6238 default)
    static constexpr int TIME_STEP = 30;

    // TOTP code length (6 digits)
    static constexpr int CODE_LENGTH = 6;
};

} // namespace common
} // namespace saasforge
