#include "common/db_pool.h"
#include <stdexcept>
#include <iostream>

namespace saasforge {
namespace common {

DbPool::DbPool(const std::string& connection_string, size_t pool_size)
    : connection_string_(connection_string),
      pool_size_(pool_size),
      shutdown_(false) {
    InitializePool();
}

DbPool::~DbPool() {
    std::unique_lock<std::mutex> lock(mutex_);
    shutdown_ = true;
    cv_.notify_all();

    // Close all connections
    while (!pool_.empty()) {
        pool_.pop();
    }
}

void DbPool::InitializePool() {
    try {
        for (size_t i = 0; i < pool_size_; ++i) {
            auto conn = std::make_shared<pqxx::connection>(connection_string_);
            if (!conn->is_open()) {
                throw std::runtime_error("Failed to open database connection");
            }
            pool_.push(conn);
        }
        std::cout << "Database connection pool initialized with " << pool_size_ << " connections" << std::endl;
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Failed to initialize database pool: ") + e.what());
    }
}

std::shared_ptr<pqxx::connection> DbPool::GetConnection() {
    std::unique_lock<std::mutex> lock(mutex_);

    // Wait until a connection is available
    cv_.wait(lock, [this] { return !pool_.empty() || shutdown_; });

    if (shutdown_) {
        throw std::runtime_error("Database pool is shutting down");
    }

    if (pool_.empty()) {
        throw std::runtime_error("No database connections available");
    }

    auto conn = pool_.front();
    pool_.pop();

    // Verify connection is still alive, reconnect if needed
    try {
        if (!conn->is_open()) {
            conn = std::make_shared<pqxx::connection>(connection_string_);
        }
    } catch (const std::exception& e) {
        // Try to create a new connection
        conn = std::make_shared<pqxx::connection>(connection_string_);
    }

    return conn;
}

void DbPool::ReturnConnection(std::shared_ptr<pqxx::connection> conn) {
    if (!conn) {
        return;
    }

    std::unique_lock<std::mutex> lock(mutex_);

    if (shutdown_) {
        return; // Don't return connections if shutting down
    }

    // Verify connection is still good before returning to pool
    try {
        if (conn->is_open()) {
            pool_.push(conn);
            cv_.notify_one();
        } else {
            // Connection is bad, create a new one
            auto new_conn = std::make_shared<pqxx::connection>(connection_string_);
            pool_.push(new_conn);
            cv_.notify_one();
        }
    } catch (const std::exception& e) {
        // Failed to reconnect, just reduce pool size
        std::cerr << "Failed to return connection to pool: " << e.what() << std::endl;
    }
}

DbPool::ConnectionGuard DbPool::AcquireConnection() {
    return ConnectionGuard(this, GetConnection());
}

} // namespace common
} // namespace saasforge
