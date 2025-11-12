#pragma once

#include <string>
#include <memory>
#include <pqxx/pqxx>
#include <mutex>
#include <queue>
#include <condition_variable>

namespace saasforge {
namespace common {

class DbPool {
public:
    DbPool(const std::string& connection_string, size_t pool_size = 10);
    ~DbPool();

    // Get a connection from the pool (blocks if none available)
    std::shared_ptr<pqxx::connection> GetConnection();

    // Return a connection to the pool
    void ReturnConnection(std::shared_ptr<pqxx::connection> conn);

    // RAII wrapper for automatic connection return
    class ConnectionGuard {
    public:
        ConnectionGuard(DbPool* pool, std::shared_ptr<pqxx::connection> conn)
            : pool_(pool), conn_(conn) {}

        ~ConnectionGuard() {
            if (conn_ && pool_) {
                pool_->ReturnConnection(conn_);
            }
        }

        pqxx::connection& operator*() { return *conn_; }
        pqxx::connection* operator->() { return conn_.get(); }

        // Prevent copying
        ConnectionGuard(const ConnectionGuard&) = delete;
        ConnectionGuard& operator=(const ConnectionGuard&) = delete;

        // Allow moving
        ConnectionGuard(ConnectionGuard&& other) noexcept
            : pool_(other.pool_), conn_(std::move(other.conn_)) {
            other.pool_ = nullptr;
        }

    private:
        DbPool* pool_;
        std::shared_ptr<pqxx::connection> conn_;
    };

    // Get a connection with RAII guard
    ConnectionGuard AcquireConnection();

private:
    std::string connection_string_;
    std::queue<std::shared_ptr<pqxx::connection>> pool_;
    std::mutex mutex_;
    std::condition_variable cv_;
    size_t pool_size_;
    bool shutdown_;

    void InitializePool();
};

} // namespace common
} // namespace saasforge
