#include "common/password_hasher.h"
#include <argon2.h>
#include <cstring>
#include <random>
#include <sstream>
#include <iomanip>
#include <stdexcept>

namespace saasforge {
namespace common {

std::string PasswordHasher::HashPassword(const std::string& password) {
    // Generate random salt
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<unsigned char> dis(0, 255);

    std::vector<unsigned char> salt(SALT_LENGTH);
    for (size_t i = 0; i < SALT_LENGTH; i++) {
        salt[i] = dis(gen);
    }

    // Allocate buffer for hash output
    std::vector<unsigned char> hash(HASH_LENGTH);

    // Hash password using Argon2id
    int result = argon2id_hash_raw(
        TIME_COST,                          // t_cost (iterations)
        MEMORY_COST_KB,                     // m_cost (memory in KB)
        PARALLELISM,                        // parallelism (threads)
        password.c_str(),                   // password
        password.length(),                  // password length
        salt.data(),                        // salt
        SALT_LENGTH,                        // salt length
        hash.data(),                        // output hash
        HASH_LENGTH                         // hash length
    );

    if (result != ARGON2_OK) {
        throw std::runtime_error(std::string("Argon2 hashing failed: ") +
                                 argon2_error_message(result));
    }

    // Encode to PHC string format (includes parameters, salt, and hash)
    // Format: $argon2id$v=19$m=65536,t=3,p=4$salt$hash
    size_t encoded_len = argon2_encodedlen(
        TIME_COST,
        MEMORY_COST_KB,
        PARALLELISM,
        SALT_LENGTH,
        HASH_LENGTH,
        Argon2_id
    );

    std::vector<char> encoded(encoded_len);

    result = argon2id_hash_encoded(
        TIME_COST,
        MEMORY_COST_KB,
        PARALLELISM,
        password.c_str(),
        password.length(),
        salt.data(),
        SALT_LENGTH,
        HASH_LENGTH,
        encoded.data(),
        encoded_len
    );

    if (result != ARGON2_OK) {
        throw std::runtime_error(std::string("Argon2 encoding failed: ") +
                                 argon2_error_message(result));
    }

    return std::string(encoded.data());
}

bool PasswordHasher::VerifyPassword(const std::string& password, const std::string& hash) {
    // Verify password against Argon2id hash
    int result = argon2id_verify(
        hash.c_str(),
        password.c_str(),
        password.length()
    );

    return result == ARGON2_OK;
}

} // namespace common
} // namespace saasforge
