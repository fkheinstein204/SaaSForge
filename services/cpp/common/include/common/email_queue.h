/**
 * @author Heinstein F (@heinsteinh)
 * @created 2025-11-13
 * @description Email queue management with retry logic (Requirements D-98, E-113)
 */

#pragma once

#include <string>
#include <memory>
#include <chrono>
#include <optional>
#include <vector>
#include "db_pool.h"

namespace saasforge {
namespace common {

/**
 * Email status in queue
 *
 * State transitions:
 * PENDING → SENDING → SENT (success)
 * PENDING → SENDING → FAILED → RETRY → SENDING → SENT (retry success)
 * PENDING → SENDING → FAILED → RETRY → ... → EXHAUSTED (max retries)
 * PENDING → SENDING → BOUNCED (hard bounce)
 */
enum class EmailStatus {
    PENDING = 0,     // Queued, not yet sent
    SENDING = 1,     // Currently being sent
    SENT = 2,        // Successfully sent
    FAILED = 3,      // Failed to send (temporary)
    RETRY = 4,       // Scheduled for retry
    EXHAUSTED = 5,   // Max retries reached, giving up
    BOUNCED = 6      // Hard bounce (permanent failure)
};

/**
 * Email bounce type (Requirement E-113)
 */
enum class BounceType {
    NONE = 0,
    SOFT = 1,    // Temporary failure (mailbox full, server down)
    HARD = 2     // Permanent failure (address doesn't exist)
};

/**
 * Queued email record
 */
struct QueuedEmail {
    std::string id;
    std::string tenant_id;
    std::string user_id;
    std::string to_address;
    std::string subject;
    std::string body_html;
    std::string body_text;
    std::string template_id;
    EmailStatus status;
    int retry_count;
    int64_t created_at;
    int64_t scheduled_at;  // When to send/retry
    int64_t sent_at;
    BounceType bounce_type;
    std::string error_message;
};

/**
 * Email queue helper for managing email delivery with retry logic
 *
 * Retry Strategy (Requirement D-98):
 * - Max 3 retries with exponential backoff: 1s, 5s, 30s
 * - Hard bounces are not retried
 * - Soft bounces follow retry schedule
 *
 * Bounce Handling (Requirement E-113):
 * - Hard bounce: Mark as BOUNCED, suppress future sends
 * - Soft bounce: Retry up to 3 times over 72 hours
 * - Alert when bounce rate exceeds 5%
 */
class EmailQueue {
public:
    explicit EmailQueue(std::shared_ptr<DbPool> db_pool);

    /**
     * Enqueue an email for delivery
     *
     * @param tenant_id Tenant ID for isolation
     * @param user_id User ID who triggered the email
     * @param to_address Recipient email address
     * @param subject Email subject
     * @param body_html HTML body
     * @param body_text Plain text body (fallback)
     * @param template_id Optional template ID
     * @param priority Higher priority = sent sooner (0-10, default 5)
     * @return Email ID
     */
    std::string Enqueue(
        const std::string& tenant_id,
        const std::string& user_id,
        const std::string& to_address,
        const std::string& subject,
        const std::string& body_html,
        const std::string& body_text = "",
        const std::string& template_id = "",
        int priority = 5
    );

    /**
     * Get next batch of emails ready to send
     *
     * @param batch_size Maximum emails to retrieve
     * @return Vector of queued emails
     */
    std::vector<QueuedEmail> GetNextBatch(int batch_size = 10);

    /**
     * Mark email as successfully sent
     *
     * @param email_id Email ID
     */
    void MarkSent(const std::string& email_id);

    /**
     * Mark email as failed and schedule retry if applicable
     *
     * @param email_id Email ID
     * @param error_message Error description
     * @param is_hard_bounce True if permanent failure
     */
    void MarkFailed(const std::string& email_id, const std::string& error_message, bool is_hard_bounce = false);

    /**
     * Mark email as bounced (hard or soft)
     *
     * @param email_id Email ID
     * @param bounce_type Hard or soft bounce
     * @param error_message Bounce reason
     */
    void MarkBounced(const std::string& email_id, BounceType bounce_type, const std::string& error_message);

    /**
     * Get bounce rate for monitoring
     *
     * @param tenant_id Tenant ID (empty for global)
     * @param hours Time window in hours (default 24)
     * @return Bounce rate as percentage (0-100)
     */
    double GetBounceRate(const std::string& tenant_id = "", int hours = 24);

    /**
     * Suppress email address (hard bounce)
     *
     * @param email_address Email to suppress
     * @param reason Suppression reason
     */
    void SuppressAddress(const std::string& email_address, const std::string& reason);

    /**
     * Check if email address is suppressed
     *
     * @param email_address Email to check
     * @return True if suppressed
     */
    bool IsAddressSuppressed(const std::string& email_address);

    /**
     * Get email status by ID
     *
     * @param email_id Email ID
     * @return Email record or nullopt if not found
     */
    std::optional<QueuedEmail> GetStatus(const std::string& email_id);

    /**
     * Calculate next retry time based on retry count
     *
     * Retry schedule:
     * - Retry 0: Immediate
     * - Retry 1: +1 second
     * - Retry 2: +5 seconds
     * - Retry 3: +30 seconds
     *
     * @param retry_count Current retry count
     * @return Seconds to wait before next retry
     */
    static int64_t GetRetryDelay(int retry_count);

    /**
     * Status enum to string
     */
    static std::string StatusToString(EmailStatus status);

    /**
     * String to status enum
     */
    static EmailStatus StringToStatus(const std::string& status);

private:
    std::shared_ptr<DbPool> db_pool_;

    /**
     * Check if email should be retried
     */
    bool ShouldRetry(int retry_count, bool is_hard_bounce);
};

} // namespace common
} // namespace saasforge
