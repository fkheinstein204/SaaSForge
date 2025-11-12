#pragma once

#include <string>
#include <cstdint>

namespace saasforge {
namespace common {

/**
 * Secure password hasher using Argon2id
 *
 * Argon2id is the recommended password hashing algorithm as of 2024:
 * - Memory-hard (resistant to GPU attacks)
 * - Time-cost configurable
 * - Winner of Password Hashing Competition (2015)
 * - Combines Argon2i (data-independent) and Argon2d (data-dependent) for side-channel resistance
 */
class PasswordHasher {
public:
    /**
     * Hash a password using Argon2id
     *
     * Parameters (following OWASP recommendations):
     * - Memory cost: 64 MB (65536 KB)
     * - Time cost: 3 iterations
     * - Parallelism: 4 threads
     * - Salt: 16 bytes (generated automatically)
     * - Hash length: 32 bytes
     *
     * @param password Plain text password to hash
     * @return Argon2id hash in PHC string format: $argon2id$v=19$m=65536,t=3,p=4$salt$hash
     */
    static std::string HashPassword(const std::string& password);

    /**
     * Verify a password against an Argon2id hash
     *
     * @param password Plain text password to verify
     * @param hash Argon2id hash in PHC string format
     * @return true if password matches, false otherwise
     */
    static bool VerifyPassword(const std::string& password, const std::string& hash);

private:
    // Argon2id parameters (OWASP recommendations for 2024)
    static constexpr uint32_t MEMORY_COST_KB = 65536;  // 64 MB
    static constexpr uint32_t TIME_COST = 3;           // 3 iterations
    static constexpr uint32_t PARALLELISM = 4;         // 4 threads
    static constexpr uint32_t HASH_LENGTH = 32;        // 32 bytes output
    static constexpr uint32_t SALT_LENGTH = 16;        // 16 bytes salt
};

} // namespace common
} // namespace saasforge
