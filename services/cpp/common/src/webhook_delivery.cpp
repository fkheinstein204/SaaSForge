/**
 * @author Heinstein F (@heinsteinh)
 * @created 2025-11-13
 * @description Webhook delivery implementation with retry logic
 */

#include "common/webhook_delivery.h"
#include "common/webhook_signer.h"
#include <iostream>
#include <sstream>
#include <pqxx/pqxx>

namespace saasforge {
namespace common {

WebhookDelivery::WebhookDelivery(std::shared_ptr<DbPool> db_pool)
    : db_pool_(db_pool) {
    std::cout << "WebhookDelivery initialized" << std::endl;
}

std::string WebhookDelivery::QueueDelivery(
    const std::string& tenant_id,
    const std::string& webhook_id,
    const std::string& event_type,
    const std::string& payload
) {
    auto conn_guard = db_pool_->AcquireConnection();
    pqxx::work txn(*conn_guard);

    // Get webhook URL and verify it's active
    auto webhook_result = txn.exec_params(
        "SELECT url, status FROM webhooks WHERE id = $1 AND tenant_id = $2",
        webhook_id,
        tenant_id
    );

    if (webhook_result.empty()) {
        throw std::runtime_error("Webhook not found: " + webhook_id);
    }

    std::string url = webhook_result[0]["url"].as<std::string>();
    std::string status = webhook_result[0]["status"].as<std::string>();

    if (status != "active") {
        throw std::runtime_error("Webhook is not active: " + webhook_id);
    }

    // Validate URL for SSRF protection
    if (!ValidateUrl(url)) {
        throw std::runtime_error("Invalid webhook URL (SSRF protection): " + url);
    }

    // Generate HMAC-SHA256 signature for webhook payload (Requirement D-103)
    std::string webhook_secret = WebhookSigner::GetMockWebhookSecret(tenant_id, webhook_id);
    std::string signature = WebhookSigner::SignPayload(payload, webhook_secret);

    // Queue the delivery with signature
    auto result = txn.exec_params(
        "INSERT INTO webhook_deliveries "
        "(tenant_id, webhook_id, event_type, payload, url, signature, status, retry_count, created_at, scheduled_at) "
        "VALUES ($1, $2, $3, $4, $5, $6, $7, 0, NOW(), NOW()) "
        "RETURNING id",
        tenant_id,
        webhook_id,
        event_type,
        payload,
        url,
        signature,
        static_cast<int>(WebhookStatus::PENDING)
    );

    std::string delivery_id = result[0]["id"].as<std::string>();
    txn.commit();

    std::cout << "Webhook delivery queued: " << delivery_id << " to " << url << std::endl;

    return delivery_id;
}

std::vector<WebhookDeliveryRecord> WebhookDelivery::GetNextBatch(int batch_size) {
    auto conn_guard = db_pool_->AcquireConnection();
    pqxx::work txn(*conn_guard);

    // Get deliveries that are:
    // 1. PENDING or RETRY status
    // 2. scheduled_at <= NOW()
    // 3. Ordered by scheduled_at ASC
    // 4. Lock for processing (FOR UPDATE SKIP LOCKED)
    auto result = txn.exec_params(
        "UPDATE webhook_deliveries SET status = $1 "
        "WHERE id IN ("
        "  SELECT id FROM webhook_deliveries "
        "  WHERE (status = $2 OR status = $3) "
        "  AND scheduled_at <= NOW() "
        "  ORDER BY scheduled_at ASC "
        "  LIMIT $4 "
        "  FOR UPDATE SKIP LOCKED"
        ") "
        "RETURNING id, tenant_id, webhook_id, event_type, payload, url, signature, status, retry_count, "
        "http_status_code, "
        "EXTRACT(EPOCH FROM created_at)::bigint as created_at, "
        "EXTRACT(EPOCH FROM scheduled_at)::bigint as scheduled_at, "
        "EXTRACT(EPOCH FROM delivered_at)::bigint as delivered_at, "
        "error_message",
        static_cast<int>(WebhookStatus::SENDING),
        static_cast<int>(WebhookStatus::PENDING),
        static_cast<int>(WebhookStatus::RETRY),
        batch_size
    );

    std::vector<WebhookDeliveryRecord> deliveries;
    for (const auto& row : result) {
        WebhookDeliveryRecord delivery;
        delivery.id = row["id"].as<std::string>();
        delivery.tenant_id = row["tenant_id"].as<std::string>();
        delivery.webhook_id = row["webhook_id"].as<std::string>();
        delivery.event_type = row["event_type"].as<std::string>();
        delivery.payload = row["payload"].as<std::string>();
        delivery.url = row["url"].as<std::string>();
        delivery.signature = row["signature"].as<std::string>();
        delivery.status = static_cast<WebhookStatus>(row["status"].as<int>());
        delivery.retry_count = row["retry_count"].as<int>();
        delivery.http_status_code = row["http_status_code"].is_null() ? 0 : row["http_status_code"].as<int>();
        delivery.created_at = row["created_at"].as<int64_t>();
        delivery.scheduled_at = row["scheduled_at"].as<int64_t>();
        delivery.delivered_at = row["delivered_at"].is_null() ? 0 : row["delivered_at"].as<int64_t>();
        delivery.error_message = row["error_message"].is_null() ? "" : row["error_message"].as<std::string>();

        deliveries.push_back(delivery);
    }

    txn.commit();

    std::cout << "Retrieved " << deliveries.size() << " webhook deliveries from queue" << std::endl;

    return deliveries;
}

void WebhookDelivery::MarkDelivered(const std::string& delivery_id, int http_status) {
    auto conn_guard = db_pool_->AcquireConnection();
    pqxx::work txn(*conn_guard);

    // Get webhook_id to reset failure count
    auto result = txn.exec_params(
        "SELECT webhook_id FROM webhook_deliveries WHERE id = $1",
        delivery_id
    );

    if (result.empty()) {
        throw std::runtime_error("Delivery not found: " + delivery_id);
    }

    std::string webhook_id = result[0]["webhook_id"].as<std::string>();

    // Mark delivery as successful
    txn.exec_params(
        "UPDATE webhook_deliveries SET status = $1, http_status_code = $2, delivered_at = NOW() "
        "WHERE id = $3",
        static_cast<int>(WebhookStatus::DELIVERED),
        http_status,
        delivery_id
    );

    // Reset webhook failure count on success
    txn.exec_params(
        "UPDATE webhooks SET failure_count = 0, last_triggered_at = NOW() WHERE id = $1",
        webhook_id
    );

    txn.commit();

    std::cout << "Webhook delivered successfully: " << delivery_id << " (HTTP " << http_status << ")" << std::endl;
}

void WebhookDelivery::MarkFailed(const std::string& delivery_id, int http_status, const std::string& error_message) {
    auto conn_guard = db_pool_->AcquireConnection();
    pqxx::work txn(*conn_guard);

    // Get current retry count and webhook_id
    auto result = txn.exec_params(
        "SELECT retry_count, webhook_id FROM webhook_deliveries WHERE id = $1",
        delivery_id
    );

    if (result.empty()) {
        throw std::runtime_error("Delivery not found: " + delivery_id);
    }

    int retry_count = result[0]["retry_count"].as<int>();
    std::string webhook_id = result[0]["webhook_id"].as<std::string>();

    if (ShouldRetry(retry_count, http_status)) {
        // Schedule retry with exponential backoff
        int new_retry_count = retry_count + 1;
        int64_t delay_seconds = GetRetryDelay(new_retry_count);

        txn.exec_params(
            "UPDATE webhook_deliveries SET status = $1, retry_count = $2, http_status_code = $3, "
            "error_message = $4, scheduled_at = NOW() + INTERVAL '$5 seconds' "
            "WHERE id = $6",
            static_cast<int>(WebhookStatus::RETRY),
            new_retry_count,
            http_status,
            error_message,
            delay_seconds,
            delivery_id
        );

        std::cout << "Webhook scheduled for retry " << new_retry_count << " in " << delay_seconds << "s: "
                  << delivery_id << std::endl;
    } else {
        // Max retries exhausted
        txn.exec_params(
            "UPDATE webhook_deliveries SET status = $1, http_status_code = $2, error_message = $3 "
            "WHERE id = $4",
            static_cast<int>(WebhookStatus::EXHAUSTED),
            http_status,
            error_message,
            delivery_id
        );

        std::cout << "Webhook max retries exhausted: " << delivery_id << std::endl;
    }

    // Increment webhook failure count
    txn.exec_params(
        "UPDATE webhooks SET failure_count = failure_count + 1 WHERE id = $1",
        webhook_id
    );

    // Check if we should disable the webhook (10+ consecutive failures)
    int consecutive_failures = GetConsecutiveFailures(webhook_id);
    if (consecutive_failures >= 10) {
        DisableWebhook(webhook_id, "Too many consecutive failures (" + std::to_string(consecutive_failures) + ")");
    }

    txn.commit();
}

std::optional<WebhookDeliveryRecord> WebhookDelivery::GetStatus(const std::string& delivery_id) {
    auto conn_guard = db_pool_->AcquireConnection();
    pqxx::work txn(*conn_guard);

    auto result = txn.exec_params(
        "SELECT id, tenant_id, webhook_id, event_type, payload, url, signature, status, retry_count, "
        "http_status_code, "
        "EXTRACT(EPOCH FROM created_at)::bigint as created_at, "
        "EXTRACT(EPOCH FROM scheduled_at)::bigint as scheduled_at, "
        "EXTRACT(EPOCH FROM delivered_at)::bigint as delivered_at, "
        "error_message "
        "FROM webhook_deliveries WHERE id = $1",
        delivery_id
    );

    if (result.empty()) {
        return std::nullopt;
    }

    auto row = result[0];
    WebhookDeliveryRecord delivery;
    delivery.id = row["id"].as<std::string>();
    delivery.tenant_id = row["tenant_id"].as<std::string>();
    delivery.webhook_id = row["webhook_id"].as<std::string>();
    delivery.event_type = row["event_type"].as<std::string>();
    delivery.payload = row["payload"].as<std::string>();
    delivery.url = row["url"].as<std::string>();
    delivery.signature = row["signature"].as<std::string>();
    delivery.status = static_cast<WebhookStatus>(row["status"].as<int>());
    delivery.retry_count = row["retry_count"].as<int>();
    delivery.http_status_code = row["http_status_code"].is_null() ? 0 : row["http_status_code"].as<int>();
    delivery.created_at = row["created_at"].as<int64_t>();
    delivery.scheduled_at = row["scheduled_at"].as<int64_t>();
    delivery.delivered_at = row["delivered_at"].is_null() ? 0 : row["delivered_at"].as<int64_t>();
    delivery.error_message = row["error_message"].is_null() ? "" : row["error_message"].as<std::string>();

    return delivery;
}

int WebhookDelivery::GetConsecutiveFailures(const std::string& webhook_id) {
    auto conn_guard = db_pool_->AcquireConnection();
    pqxx::work txn(*conn_guard);

    auto result = txn.exec_params(
        "SELECT failure_count FROM webhooks WHERE id = $1",
        webhook_id
    );

    if (result.empty()) {
        return 0;
    }

    return result[0]["failure_count"].as<int>();
}

void WebhookDelivery::DisableWebhook(const std::string& webhook_id, const std::string& reason) {
    auto conn_guard = db_pool_->AcquireConnection();
    pqxx::work txn(*conn_guard);

    txn.exec_params(
        "UPDATE webhooks SET status = 'disabled', disabled_reason = $1 WHERE id = $2",
        reason,
        webhook_id
    );

    txn.commit();

    std::cout << "Webhook disabled: " << webhook_id << " (" << reason << ")" << std::endl;
}

int64_t WebhookDelivery::GetRetryDelay(int retry_count) {
    // Retry schedule (Requirement D-104): 1s, 5s, 30s, 5min, 30min
    switch (retry_count) {
        case 0:
            return 0;      // Immediate
        case 1:
            return 1;      // 1 second
        case 2:
            return 5;      // 5 seconds
        case 3:
            return 30;     // 30 seconds
        case 4:
            return 300;    // 5 minutes
        case 5:
            return 1800;   // 30 minutes
        default:
            return 1800;   // Max delay
    }
}

bool WebhookDelivery::ValidateUrl(const std::string& url) {
    // Must start with http:// or https://
    if (url.find("http://") != 0 && url.find("https://") != 0) {
        return false;
    }

    // Extract hostname and port
    size_t protocol_end = url.find("://") + 3;
    size_t host_end = url.find(":", protocol_end);
    if (host_end == std::string::npos) {
        host_end = url.find("/", protocol_end);
    }
    if (host_end == std::string::npos) {
        host_end = url.length();
    }

    std::string host = url.substr(protocol_end, host_end - protocol_end);

    // Reject localhost and loopback addresses
    if (host == "localhost" || host == "127.0.0.1" || host == "0.0.0.0" ||
        host == "::1" || host == "[::1]") {
        return false;
    }

    // Reject private IP ranges (RFC 1918)
    if (host.find("192.168.") == 0 || host.find("10.") == 0) {
        return false;
    }

    // Check 172.16.0.0 - 172.31.255.255 range
    if (host.find("172.") == 0) {
        size_t second_dot = host.find(".", 4);
        if (second_dot != std::string::npos) {
            try {
                int second_octet = std::stoi(host.substr(4, second_dot - 4));
                if (second_octet >= 16 && second_octet <= 31) {
                    return false;  // Private range
                }
            } catch (const std::exception&) {
                return false;  // Invalid format
            }
        }
    }

    // Reject link-local addresses (169.254.0.0/16 - AWS metadata endpoint)
    if (host.find("169.254.") == 0) {
        return false;
    }

    // Validate port if specified
    if (host_end < url.length() && url[host_end] == ':') {
        size_t port_end = url.find("/", host_end);
        if (port_end == std::string::npos) {
            port_end = url.length();
        }

        try {
            int port = std::stoi(url.substr(host_end + 1, port_end - host_end - 1));

            // Only allow standard web ports (prevent access to internal services)
            if (port != 80 && port != 443 && port != 8080 && port != 8443) {
                return false;
            }
        } catch (const std::exception&) {
            return false;  // Invalid port number
        }
    }

    return true;
}

std::string WebhookDelivery::StatusToString(WebhookStatus status) {
    switch (status) {
        case WebhookStatus::PENDING: return "pending";
        case WebhookStatus::SENDING: return "sending";
        case WebhookStatus::DELIVERED: return "delivered";
        case WebhookStatus::FAILED: return "failed";
        case WebhookStatus::RETRY: return "retry";
        case WebhookStatus::EXHAUSTED: return "exhausted";
        default: return "unknown";
    }
}

WebhookStatus WebhookDelivery::StringToStatus(const std::string& status) {
    if (status == "pending") return WebhookStatus::PENDING;
    if (status == "sending") return WebhookStatus::SENDING;
    if (status == "delivered") return WebhookStatus::DELIVERED;
    if (status == "failed") return WebhookStatus::FAILED;
    if (status == "retry") return WebhookStatus::RETRY;
    if (status == "exhausted") return WebhookStatus::EXHAUSTED;
    return WebhookStatus::PENDING;
}

bool WebhookDelivery::ShouldRetry(int retry_count, int http_status) {
    // Don't retry client errors (4xx) except rate limiting (429)
    if (http_status >= 400 && http_status < 500 && http_status != 429) {
        return false;
    }

    // Max 5 retries (Requirement D-104)
    return retry_count < 5;
}

} // namespace common
} // namespace saasforge
