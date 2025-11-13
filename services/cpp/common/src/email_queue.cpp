/**
 * @author Heinstein F (@heinsteinh)
 * @created 2025-11-13
 * @description Email queue implementation with retry logic
 */

#include "common/email_queue.h"
#include <iostream>
#include <sstream>
#include <pqxx/pqxx>

namespace saasforge {
namespace common {

EmailQueue::EmailQueue(std::shared_ptr<DbPool> db_pool)
    : db_pool_(db_pool) {
    std::cout << "EmailQueue initialized" << std::endl;
}

std::string EmailQueue::Enqueue(
    const std::string& tenant_id,
    const std::string& user_id,
    const std::string& to_address,
    const std::string& subject,
    const std::string& body_html,
    const std::string& body_text,
    const std::string& template_id,
    int priority
) {
    // Check if address is suppressed (hard bounce)
    if (IsAddressSuppressed(to_address)) {
        throw std::runtime_error("Email address is suppressed due to hard bounce");
    }

    auto conn_guard = db_pool_->AcquireConnection();
    pqxx::work txn(*conn_guard);

    pqxx::result result;
    if (template_id.empty()) {
        result = txn.exec_params(
            "INSERT INTO email_queue "
            "(tenant_id, user_id, to_address, subject, body_html, body_text, template_id, "
            "status, retry_count, priority, created_at, scheduled_at) "
            "VALUES ($1, $2, $3, $4, $5, $6, NULL, $7, 0, $8, NOW(), NOW()) "
            "RETURNING id",
            tenant_id,
            user_id,
            to_address,
            subject,
            body_html,
            body_text,
            static_cast<int>(EmailStatus::PENDING),
            priority
        );
    } else {
        result = txn.exec_params(
            "INSERT INTO email_queue "
            "(tenant_id, user_id, to_address, subject, body_html, body_text, template_id, "
            "status, retry_count, priority, created_at, scheduled_at) "
            "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, 0, $9, NOW(), NOW()) "
            "RETURNING id",
            tenant_id,
            user_id,
            to_address,
            subject,
            body_html,
            body_text,
            template_id,
            static_cast<int>(EmailStatus::PENDING),
            priority
        );
    }

    std::string email_id = result[0]["id"].c_str();
    txn.commit();

    std::cout << "Email queued: " << email_id << " to " << to_address << std::endl;

    return email_id;
}

std::vector<QueuedEmail> EmailQueue::GetNextBatch(int batch_size) {
    auto conn_guard = db_pool_->AcquireConnection();
    pqxx::work txn(*conn_guard);

    // Get emails that are:
    // 1. PENDING or RETRY status
    // 2. scheduled_at <= NOW()
    // 3. Ordered by priority DESC, scheduled_at ASC
    // 4. Lock for processing (FOR UPDATE SKIP LOCKED)
    auto result = txn.exec_params(
        "UPDATE email_queue SET status = $1 "
        "WHERE id IN ("
        "  SELECT id FROM email_queue "
        "  WHERE (status = $2 OR status = $3) "
        "  AND scheduled_at <= NOW() "
        "  ORDER BY priority DESC, scheduled_at ASC "
        "  LIMIT $4 "
        "  FOR UPDATE SKIP LOCKED"
        ") "
        "RETURNING id, tenant_id, user_id, to_address, subject, body_html, body_text, "
        "template_id, status, retry_count, "
        "EXTRACT(EPOCH FROM created_at)::bigint as created_at, "
        "EXTRACT(EPOCH FROM scheduled_at)::bigint as scheduled_at, "
        "EXTRACT(EPOCH FROM sent_at)::bigint as sent_at, "
        "bounce_type, error_message",
        static_cast<int>(EmailStatus::SENDING),
        static_cast<int>(EmailStatus::PENDING),
        static_cast<int>(EmailStatus::RETRY),
        batch_size
    );

    std::vector<QueuedEmail> emails;
    for (const auto& row : result) {
        QueuedEmail email;
        email.id = row["id"].as<std::string>();
        email.tenant_id = row["tenant_id"].as<std::string>();
        email.user_id = row["user_id"].as<std::string>();
        email.to_address = row["to_address"].as<std::string>();
        email.subject = row["subject"].as<std::string>();
        email.body_html = row["body_html"].as<std::string>();
        email.body_text = row["body_text"].is_null() ? "" : row["body_text"].as<std::string>();
        email.template_id = row["template_id"].is_null() ? "" : row["template_id"].as<std::string>();
        email.status = static_cast<EmailStatus>(row["status"].as<int>());
        email.retry_count = row["retry_count"].as<int>();
        email.created_at = row["created_at"].as<int64_t>();
        email.scheduled_at = row["scheduled_at"].as<int64_t>();
        email.sent_at = row["sent_at"].is_null() ? 0 : row["sent_at"].as<int64_t>();
        email.bounce_type = static_cast<BounceType>(row["bounce_type"].is_null() ? 0 : row["bounce_type"].as<int>());
        email.error_message = row["error_message"].is_null() ? "" : row["error_message"].as<std::string>();

        emails.push_back(email);
    }

    txn.commit();

    std::cout << "Retrieved " << emails.size() << " emails from queue" << std::endl;

    return emails;
}

void EmailQueue::MarkSent(const std::string& email_id) {
    auto conn_guard = db_pool_->AcquireConnection();
    pqxx::work txn(*conn_guard);

    txn.exec_params(
        "UPDATE email_queue SET status = $1, sent_at = NOW() WHERE id = $2",
        static_cast<int>(EmailStatus::SENT),
        email_id
    );

    txn.commit();

    std::cout << "Email marked as sent: " << email_id << std::endl;
}

void EmailQueue::MarkFailed(const std::string& email_id, const std::string& error_message, bool is_hard_bounce) {
    auto conn_guard = db_pool_->AcquireConnection();
    pqxx::work txn(*conn_guard);

    // Get current retry count
    auto result = txn.exec_params(
        "SELECT retry_count, to_address FROM email_queue WHERE id = $1",
        email_id
    );

    if (result.empty()) {
        throw std::runtime_error("Email not found: " + email_id);
    }

    int retry_count = result[0]["retry_count"].as<int>();
    std::string to_address = result[0]["to_address"].as<std::string>();

    if (is_hard_bounce) {
        // Hard bounce - mark as BOUNCED and suppress address
        txn.exec_params(
            "UPDATE email_queue SET status = $1, bounce_type = $2, error_message = $3 "
            "WHERE id = $4",
            static_cast<int>(EmailStatus::BOUNCED),
            static_cast<int>(BounceType::HARD),
            error_message,
            email_id
        );

        // Suppress the address for future sends
        SuppressAddress(to_address, error_message);

        std::cout << "Email marked as hard bounced: " << email_id << " (address suppressed)" << std::endl;
    } else if (ShouldRetry(retry_count, is_hard_bounce)) {
        // Schedule retry with exponential backoff
        int new_retry_count = retry_count + 1;
        int64_t delay_seconds = GetRetryDelay(new_retry_count);

        txn.exec_params(
            "UPDATE email_queue SET status = $1, retry_count = $2, error_message = $3, "
            "scheduled_at = NOW() + INTERVAL '$4 seconds' "
            "WHERE id = $5",
            static_cast<int>(EmailStatus::RETRY),
            new_retry_count,
            error_message,
            delay_seconds,
            email_id
        );

        std::cout << "Email scheduled for retry " << new_retry_count << " in " << delay_seconds << "s: " << email_id << std::endl;
    } else {
        // Max retries exhausted
        txn.exec_params(
            "UPDATE email_queue SET status = $1, error_message = $2 WHERE id = $3",
            static_cast<int>(EmailStatus::EXHAUSTED),
            error_message,
            email_id
        );

        std::cout << "Email max retries exhausted: " << email_id << std::endl;
    }

    txn.commit();
}

void EmailQueue::MarkBounced(const std::string& email_id, BounceType bounce_type, const std::string& error_message) {
    auto conn_guard = db_pool_->AcquireConnection();
    pqxx::work txn(*conn_guard);

    auto result = txn.exec_params(
        "SELECT to_address FROM email_queue WHERE id = $1",
        email_id
    );

    if (result.empty()) {
        throw std::runtime_error("Email not found: " + email_id);
    }

    std::string to_address = result[0]["to_address"].as<std::string>();

    if (bounce_type == BounceType::HARD) {
        // Hard bounce - suppress address
        txn.exec_params(
            "UPDATE email_queue SET status = $1, bounce_type = $2, error_message = $3 WHERE id = $4",
            static_cast<int>(EmailStatus::BOUNCED),
            static_cast<int>(BounceType::HARD),
            error_message,
            email_id
        );

        SuppressAddress(to_address, error_message);

        std::cout << "Hard bounce recorded and address suppressed: " << to_address << std::endl;
    } else {
        // Soft bounce - mark as failed for retry
        txn.exec_params(
            "UPDATE email_queue SET bounce_type = $1, error_message = $2 WHERE id = $3",
            static_cast<int>(BounceType::SOFT),
            error_message,
            email_id
        );

        std::cout << "Soft bounce recorded: " << email_id << std::endl;
    }

    txn.commit();
}

double EmailQueue::GetBounceRate(const std::string& tenant_id, int hours) {
    auto conn_guard = db_pool_->AcquireConnection();
    pqxx::work txn(*conn_guard);

    std::string query;
    if (tenant_id.empty()) {
        query = "SELECT "
                "COUNT(*) FILTER (WHERE status = " + std::to_string(static_cast<int>(EmailStatus::BOUNCED)) + ") as bounced, "
                "COUNT(*) as total "
                "FROM email_queue "
                "WHERE created_at >= NOW() - INTERVAL '" + std::to_string(hours) + " hours'";
    } else {
        query = "SELECT "
                "COUNT(*) FILTER (WHERE status = " + std::to_string(static_cast<int>(EmailStatus::BOUNCED)) + ") as bounced, "
                "COUNT(*) as total "
                "FROM email_queue "
                "WHERE tenant_id = '" + txn.esc(tenant_id) + "' "
                "AND created_at >= NOW() - INTERVAL '" + std::to_string(hours) + " hours'";
    }

    auto result = txn.exec(query);

    if (result.empty()) {
        return 0.0;
    }

    auto row = result[0];
    int64_t bounced = row["bounced"].as<int64_t>();
    int64_t total = row["total"].as<int64_t>();

    if (total == 0) {
        return 0.0;
    }

    double rate = (static_cast<double>(bounced) / static_cast<double>(total)) * 100.0;

    return rate;
}

void EmailQueue::SuppressAddress(const std::string& email_address, const std::string& reason) {
    auto conn_guard = db_pool_->AcquireConnection();
    pqxx::work txn(*conn_guard);

    txn.exec_params(
        "INSERT INTO email_suppression (email_address, reason, created_at) "
        "VALUES ($1, $2, NOW()) "
        "ON CONFLICT (email_address) DO UPDATE SET reason = $2, created_at = NOW()",
        email_address,
        reason
    );

    txn.commit();

    std::cout << "Email address suppressed: " << email_address << std::endl;
}

bool EmailQueue::IsAddressSuppressed(const std::string& email_address) {
    auto conn_guard = db_pool_->AcquireConnection();
    pqxx::work txn(*conn_guard);

    auto result = txn.exec_params(
        "SELECT 1 FROM email_suppression WHERE email_address = $1",
        email_address
    );

    return !result.empty();
}

std::optional<QueuedEmail> EmailQueue::GetStatus(const std::string& email_id) {
    auto conn_guard = db_pool_->AcquireConnection();
    pqxx::work txn(*conn_guard);

    auto result = txn.exec_params(
        "SELECT id, tenant_id, user_id, to_address, subject, body_html, body_text, "
        "template_id, status, retry_count, "
        "EXTRACT(EPOCH FROM created_at)::bigint as created_at, "
        "EXTRACT(EPOCH FROM scheduled_at)::bigint as scheduled_at, "
        "EXTRACT(EPOCH FROM sent_at)::bigint as sent_at, "
        "bounce_type, error_message "
        "FROM email_queue WHERE id = $1",
        email_id
    );

    if (result.empty()) {
        return std::nullopt;
    }

    auto row = result[0];
    QueuedEmail email;
    email.id = row["id"].as<std::string>();
    email.tenant_id = row["tenant_id"].as<std::string>();
    email.user_id = row["user_id"].as<std::string>();
    email.to_address = row["to_address"].as<std::string>();
    email.subject = row["subject"].as<std::string>();
    email.body_html = row["body_html"].as<std::string>();
    email.body_text = row["body_text"].is_null() ? "" : row["body_text"].as<std::string>();
    email.template_id = row["template_id"].is_null() ? "" : row["template_id"].as<std::string>();
    email.status = static_cast<EmailStatus>(row["status"].as<int>());
    email.retry_count = row["retry_count"].as<int>();
    email.created_at = row["created_at"].as<int64_t>();
    email.scheduled_at = row["scheduled_at"].as<int64_t>();
    email.sent_at = row["sent_at"].is_null() ? 0 : row["sent_at"].as<int64_t>();
    email.bounce_type = static_cast<BounceType>(row["bounce_type"].is_null() ? 0 : row["bounce_type"].as<int>());
    email.error_message = row["error_message"].is_null() ? "" : row["error_message"].as<std::string>();

    return email;
}

int64_t EmailQueue::GetRetryDelay(int retry_count) {
    // Retry schedule (Requirement D-98): 1s, 5s, 30s
    switch (retry_count) {
        case 0:
            return 0;   // Immediate
        case 1:
            return 1;   // 1 second
        case 2:
            return 5;   // 5 seconds
        case 3:
            return 30;  // 30 seconds
        default:
            return 30;  // Max delay
    }
}

std::string EmailQueue::StatusToString(EmailStatus status) {
    switch (status) {
        case EmailStatus::PENDING: return "pending";
        case EmailStatus::SENDING: return "sending";
        case EmailStatus::SENT: return "sent";
        case EmailStatus::FAILED: return "failed";
        case EmailStatus::RETRY: return "retry";
        case EmailStatus::EXHAUSTED: return "exhausted";
        case EmailStatus::BOUNCED: return "bounced";
        default: return "unknown";
    }
}

EmailStatus EmailQueue::StringToStatus(const std::string& status) {
    if (status == "pending") return EmailStatus::PENDING;
    if (status == "sending") return EmailStatus::SENDING;
    if (status == "sent") return EmailStatus::SENT;
    if (status == "failed") return EmailStatus::FAILED;
    if (status == "retry") return EmailStatus::RETRY;
    if (status == "exhausted") return EmailStatus::EXHAUSTED;
    if (status == "bounced") return EmailStatus::BOUNCED;
    return EmailStatus::PENDING;
}

bool EmailQueue::ShouldRetry(int retry_count, bool is_hard_bounce) {
    // Hard bounces are never retried
    if (is_hard_bounce) {
        return false;
    }

    // Max 3 retries (Requirement D-98)
    return retry_count < 3;
}

} // namespace common
} // namespace saasforge
