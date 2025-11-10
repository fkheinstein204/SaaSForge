#include "common/redis_client.h"

namespace saasforge {
namespace common {

RedisClient::RedisClient(const std::string& connection_string) {
    redis_ = std::make_unique<sw::redis::Redis>(connection_string);
}

void RedisClient::BlacklistToken(const std::string& jti, int64_t ttl_seconds) {
    std::string key = "blacklist:" + jti;
    redis_->setex(key, ttl_seconds, R"({"reason":"logout"})");
}

bool RedisClient::IsTokenBlacklisted(const std::string& jti) {
    std::string key = "blacklist:" + jti;
    auto value = redis_->get(key);
    return value.has_value();
}

void RedisClient::SetSession(const std::string& session_id, const std::string& data, int64_t ttl_seconds) {
    std::string key = "session:" + session_id;
    redis_->setex(key, ttl_seconds, data);
}

std::optional<std::string> RedisClient::GetSession(const std::string& session_id) {
    std::string key = "session:" + session_id;
    return redis_->get(key);
}

void RedisClient::DeleteSession(const std::string& session_id) {
    std::string key = "session:" + session_id;
    redis_->del(key);
}

int64_t RedisClient::IncrementCounter(const std::string& key, int64_t ttl_seconds) {
    auto count = redis_->incr(key);
    if (count == 1) {
        redis_->expire(key, ttl_seconds);
    }
    return count;
}

} // namespace common
} // namespace saasforge
