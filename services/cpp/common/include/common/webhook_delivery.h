/**
 * @author Heinstein F (@heinsteinh)
 * @created 2025-11-13
 * @description Webhook delivery with retry logic (Requirement D-104)
 */

#pragma once

#include <string>
#include <memory>
#include <optional>
#include <vector>
#include "db_pool.h"

namespace saasforge {
namespace common {

/**
 * Webhook delivery status
 *
 * State transitions:
 * PENDING → SENDING → DELIVERED (success)
 * PENDING → SENDING → FAILED → RETRY → SENDING → DELIVERED (retry success)
 * PENDING → SENDING → FAILED → RETRY → ... → EXHAUSTED (max retries)
 */
enum class WebhookStatus {
    PENDING = 0,     // Queued, not yet sent
    SENDING = 1,     // Currently being sent
    DELIVERED = 2,   // Successfully delivered (2xx response)
    FAILED = 3,      // Failed to deliver (temporary)
    RETRY = 4,       // Scheduled for retry
    EXHAUSTED = 5    // Max retries reached, giving up
};

/**
 * Webhook delivery record
 */
struct WebhookDeliveryRecord {
    std::string id;
    std::string tenant_id;
    std::string webhook_id;
    std::string event_type;
    std::string payload;
    std::string url;
    WebhookStatus status;
    int retry_count;
    int http_status_code;
    int64_t created_at;
    int64_t scheduled_at;
    int64_t delivered_at;
    std::string error_message;
};

/**
 * Webhook delivery manager with retry logic
 *
 * Retry Strategy (Requirement D-104):
 * - Max 5 retries with exponential backoff: 1s, 5s, 30s, 5min, 30min
 * - Follow maximum 2 redirects with re-validation
 * - Disable webhook after 10 consecutive failures
 *
 * SSRF Protection:
 * - Reject private IP ranges (10.0.0.0/8, 172.16.0.0/12, 192.168.0.0/16)
 * - Reject localhost and loopback addresses
 * - Only allow standard ports (80, 443, 8080, 8443)
 */
class WebhookDelivery {
public:
    explicit WebhookDelivery(std::shared_ptr<DbPool> db_pool);

    /**
     * Queue a webhook for delivery
     *
     * @param tenant_id Tenant ID for isolation
     * @param webhook_id Webhook registration ID
     * @param event_type Event type (e.g., "subscription.created")
     * @param payload JSON payload
     * @return Delivery ID
     */
    std::string QueueDelivery(
        const std::string& tenant_id,
        const std::string& webhook_id,
        const std::string& event_type,
        const std::string& payload
    );

    /**
     * Get next batch of webhooks ready to deliver
     *
     * @param batch_size Maximum webhooks to retrieve
     * @return Vector of webhook deliveries
     */
    std::vector<WebhookDeliveryRecord> GetNextBatch(int batch_size = 10);

    /**
     * Mark webhook as successfully delivered
     *
     * @param delivery_id Delivery ID
     * @param http_status HTTP status code (2xx)
     */
    void MarkDelivered(const std::string& delivery_id, int http_status);

    /**
     * Mark webhook as failed and schedule retry if applicable
     *
     * @param delivery_id Delivery ID
     * @param http_status HTTP status code (4xx, 5xx, or 0 for connection error)
     * @param error_message Error description
     */
    void MarkFailed(const std::string& delivery_id, int http_status, const std::string& error_message);

    /**
     * Get webhook delivery status by ID
     *
     * @param delivery_id Delivery ID
     * @return Delivery record or nullopt if not found
     */
    std::optional<WebhookDeliveryRecord> GetStatus(const std::string& delivery_id);

    /**
     * Get consecutive failure count for a webhook
     *
     * @param webhook_id Webhook ID
     * @return Number of consecutive failures
     */
    int GetConsecutiveFailures(const std::string& webhook_id);

    /**
     * Disable webhook after too many failures
     *
     * @param webhook_id Webhook ID
     * @param reason Disable reason
     */
    void DisableWebhook(const std::string& webhook_id, const std::string& reason);

    /**
     * Calculate next retry time based on retry count
     *
     * Retry schedule (Requirement D-104):
     * - Retry 0: Immediate
     * - Retry 1: +1 second
     * - Retry 2: +5 seconds
     * - Retry 3: +30 seconds
     * - Retry 4: +5 minutes (300 seconds)
     * - Retry 5: +30 minutes (1800 seconds)
     *
     * @param retry_count Current retry count
     * @return Seconds to wait before next retry
     */
    static int64_t GetRetryDelay(int retry_count);

    /**
     * Validate webhook URL for SSRF protection
     *
     * Checks:
     * - Must be http:// or https://
     * - No private IP ranges (10.0.0.0/8, 172.16.0.0/12, 192.168.0.0/16)
     * - No localhost/loopback (127.0.0.1, ::1)
     * - No link-local addresses (169.254.0.0/16)
     * - Only standard ports (80, 443, 8080, 8443)
     *
     * @param url Webhook URL to validate
     * @return True if URL is safe
     */
    static bool ValidateUrl(const std::string& url);

    /**
     * Status enum to string
     */
    static std::string StatusToString(WebhookStatus status);

    /**
     * String to status enum
     */
    static WebhookStatus StringToStatus(const std::string& status);

private:
    std::shared_ptr<DbPool> db_pool_;

    /**
     * Check if webhook should be retried
     */
    bool ShouldRetry(int retry_count, int http_status);
};

} // namespace common
} // namespace saasforge
