/**
 * @author Heinstein F (@heinsteinh)
 * @created 2025-11-13
 * @description TOTP helper implementation
 */

#include "common/totp_helper.h"
#include <openssl/hmac.h>
#include <openssl/sha.h>
#include <openssl/rand.h>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace saasforge {
namespace common {

std::string TotpHelper::GenerateSecret() {
    // Generate 20 random bytes (160 bits) for TOTP secret
    std::vector<uint8_t> random_bytes(20);
    if (RAND_bytes(random_bytes.data(), random_bytes.size()) != 1) {
        throw std::runtime_error("Failed to generate random bytes for TOTP secret");
    }

    return EncodeBase32(random_bytes);
}

std::string TotpHelper::GenerateQrCodeUrl(
    const std::string& secret,
    const std::string& email,
    const std::string& issuer
) {
    std::stringstream ss;
    ss << "otpauth://totp/"
       << UrlEncode(issuer) << ":" << UrlEncode(email)
       << "?secret=" << secret
       << "&issuer=" << UrlEncode(issuer)
       << "&algorithm=SHA1"
       << "&digits=" << CODE_LENGTH
       << "&period=" << TIME_STEP;
    return ss.str();
}

bool TotpHelper::ValidateCode(
    const std::string& secret,
    const std::string& code,
    int window
) {
    if (code.length() != CODE_LENGTH) {
        return false;
    }

    // Verify code contains only digits
    if (!std::all_of(code.begin(), code.end(), ::isdigit)) {
        return false;
    }

    // Get current time counter (Unix timestamp / 30)
    auto now = std::chrono::system_clock::now();
    auto unix_time = std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch()
    ).count();
    uint64_t current_counter = unix_time / TIME_STEP;

    // Check code against current time ± window
    for (int i = -window; i <= window; ++i) {
        uint64_t test_counter = current_counter + i;
        std::string expected_code = GenerateTotpCode(secret, test_counter);

        if (code == expected_code) {
            return true;
        }
    }

    return false;
}

std::vector<std::string> TotpHelper::GenerateBackupCodes(int count) {
    std::vector<std::string> codes;
    codes.reserve(count);

    for (int i = 0; i < count; ++i) {
        // Generate 4 random bytes for each code
        uint8_t random_bytes[4];
        if (RAND_bytes(random_bytes, 4) != 1) {
            throw std::runtime_error("Failed to generate random bytes for backup code");
        }

        // Convert to 8-digit code in XXXX-XXXX format
        uint32_t value = 0;
        for (int j = 0; j < 4; ++j) {
            value = (value << 8) | random_bytes[j];
        }

        // Generate 8-digit code
        std::stringstream ss;
        ss << std::setfill('0') << std::setw(4) << (value % 10000)
           << "-"
           << std::setfill('0') << std::setw(4) << ((value / 10000) % 10000);

        codes.push_back(ss.str());
    }

    return codes;
}

std::string TotpHelper::HashBackupCode(const std::string& code) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(code.c_str()),
           code.length(),
           hash);

    std::stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0')
           << static_cast<int>(hash[i]);
    }

    return ss.str();
}

bool TotpHelper::VerifyBackupCode(const std::string& code, const std::string& hash) {
    return HashBackupCode(code) == hash;
}

// Private methods

std::string TotpHelper::GenerateTotpCode(const std::string& secret, uint64_t time_counter) {
    // Decode Base32 secret to bytes
    auto key = DecodeBase32(secret);

    // Convert time counter to 8-byte big-endian array
    std::vector<uint8_t> message(8);
    for (int i = 7; i >= 0; --i) {
        message[i] = time_counter & 0xFF;
        time_counter >>= 8;
    }

    // Compute HMAC-SHA1
    auto hmac = HmacSha1(key, message);

    // Dynamic truncation (RFC 4226)
    int offset = hmac[19] & 0x0F;
    uint32_t code = ((hmac[offset] & 0x7F) << 24)
                  | ((hmac[offset + 1] & 0xFF) << 16)
                  | ((hmac[offset + 2] & 0xFF) << 8)
                  | (hmac[offset + 3] & 0xFF);

    // Generate 6-digit code
    code = code % static_cast<uint32_t>(std::pow(10, CODE_LENGTH));

    std::stringstream ss;
    ss << std::setfill('0') << std::setw(CODE_LENGTH) << code;
    return ss.str();
}

std::vector<uint8_t> TotpHelper::DecodeBase32(const std::string& base32) {
    static const std::string base32_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
    std::vector<uint8_t> result;

    int buffer = 0;
    int bits_left = 0;

    for (char c : base32) {
        if (c == ' ' || c == '\n' || c == '\r' || c == '\t' || c == '-') {
            continue;  // Skip whitespace and hyphens
        }

        // Find character in base32 alphabet
        size_t value = base32_chars.find(std::toupper(c));
        if (value == std::string::npos) {
            throw std::invalid_argument("Invalid Base32 character: " + std::string(1, c));
        }

        buffer = (buffer << 5) | value;
        bits_left += 5;

        if (bits_left >= 8) {
            result.push_back((buffer >> (bits_left - 8)) & 0xFF);
            bits_left -= 8;
        }
    }

    return result;
}

std::string TotpHelper::EncodeBase32(const std::vector<uint8_t>& data) {
    static const std::string base32_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
    std::string result;

    int buffer = 0;
    int bits_left = 0;

    for (uint8_t byte : data) {
        buffer = (buffer << 8) | byte;
        bits_left += 8;

        while (bits_left >= 5) {
            result += base32_chars[(buffer >> (bits_left - 5)) & 0x1F];
            bits_left -= 5;
        }
    }

    // Handle remaining bits
    if (bits_left > 0) {
        result += base32_chars[(buffer << (5 - bits_left)) & 0x1F];
    }

    // Add padding (optional for TOTP, but standard Base32 includes it)
    while (result.length() % 8 != 0) {
        result += '=';
    }

    return result;
}

std::vector<uint8_t> TotpHelper::HmacSha1(
    const std::vector<uint8_t>& key,
    const std::vector<uint8_t>& message
) {
    unsigned char result[EVP_MAX_MD_SIZE];
    unsigned int result_len = 0;

    HMAC(EVP_sha1(),
         key.data(), key.size(),
         message.data(), message.size(),
         result, &result_len);

    return std::vector<uint8_t>(result, result + result_len);
}

std::string TotpHelper::UrlEncode(const std::string& str) {
    std::stringstream ss;
    ss << std::hex << std::uppercase;

    for (char c : str) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            ss << c;
        } else {
            ss << '%' << std::setw(2) << std::setfill('0') << static_cast<int>(static_cast<unsigned char>(c));
        }
    }

    return ss.str();
}

} // namespace common
} // namespace saasforge
