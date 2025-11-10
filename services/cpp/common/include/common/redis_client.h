#pragma once

#include <string>
#include <memory>
#include <optional>
#include <sw/redis++/redis++.h>

namespace saasforge {
namespace common {

class RedisClient {
public:
    RedisClient(const std::string& connection_string);

    // Token blacklist operations
    void BlacklistToken(const std::string& jti, int64_t ttl_seconds);
    bool IsTokenBlacklisted(const std::string& jti);

    // Session management
    void SetSession(const std::string& session_id, const std::string& data, int64_t ttl_seconds);
    std::optional<std::string> GetSession(const std::string& session_id);
    void DeleteSession(const std::string& session_id);

    // Rate limiting
    int64_t IncrementCounter(const std::string& key, int64_t ttl_seconds);

private:
    std::unique_ptr<sw::redis::Redis> redis_;
};

} // namespace common
} // namespace saasforge
