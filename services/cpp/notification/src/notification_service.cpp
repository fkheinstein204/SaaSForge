#include "notification/notification_service.h"
#include "common/tenant_context.h"
#include <iostream>
#include <sstream>
#include <chrono>

namespace saasforge {
namespace notification {

NotificationServiceImpl::NotificationServiceImpl(
    std::shared_ptr<common::RedisClient> redis_client,
    std::shared_ptr<common::DbPool> db_pool,
    const std::string& sendgrid_api_key,
    const std::string& twilio_account_sid,
    const std::string& twilio_auth_token,
    const std::string& fcm_server_key
) : redis_client_(redis_client),
    db_pool_(db_pool),
    sendgrid_api_key_(sendgrid_api_key),
    twilio_account_sid_(twilio_account_sid),
    twilio_auth_token_(twilio_auth_token),
    fcm_server_key_(fcm_server_key) {
    std::cout << "NotificationService initialized" << std::endl;
}

grpc::Status NotificationServiceImpl::SendEmail(
    grpc::ServerContext* context,
    const SendEmailRequest* request,
    NotificationResponse* response
) {
    try {
        auto tenant_ctx = common::TenantContextInterceptor::ExtractFromMetadata(context);

        if (tenant_ctx.tenant_id.empty()) {
            return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "Authentication required");
        }

        // Check user preferences
        if (!CheckUserPreferences(request->user_id(), NotificationChannel::EMAIL)) {
            return grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "Email notifications disabled for user");
        }

        // Mock email sending (in production, call SendGrid API)
        std::cout << "Mock: Sending email to " << request->to() << std::endl;

        // Queue notification
        std::stringstream payload;
        payload << "{\"to\":\"" << request->to() << "\","
                << "\"subject\":\"" << request->subject() << "\","
                << "\"body_html\":\"" << request->body_html() << "\"}";

        std::string notification_id = QueueNotification(
            tenant_ctx.tenant_id,
            request->user_id(),
            NotificationChannel::EMAIL,
            payload.str()
        );

        // Set response
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();

        response->set_id(notification_id);
        response->set_channel(NotificationChannel::EMAIL);
        response->set_status(NotificationStatus::SENT);
        response->set_created_at(timestamp);
        response->set_sent_at(timestamp);
        response->set_retry_count(0);

        return grpc::Status::OK;

    } catch (const std::exception& e) {
        return grpc::Status(grpc::StatusCode::INTERNAL, std::string("Send email failed: ") + e.what());
    }
}

grpc::Status NotificationServiceImpl::SendSMS(
    grpc::ServerContext* context,
    const SendSMSRequest* request,
    NotificationResponse* response
) {
    try {
        auto tenant_ctx = common::TenantContextInterceptor::ExtractFromMetadata(context);

        if (tenant_ctx.tenant_id.empty()) {
            return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "Authentication required");
        }

        // Check user preferences
        if (!CheckUserPreferences(request->user_id(), NotificationChannel::SMS)) {
            return grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "SMS notifications disabled for user");
        }

        // Mock SMS sending (in production, call Twilio API)
        std::cout << "Mock: Sending SMS to " << request->to() << std::endl;

        // Queue notification
        std::stringstream payload;
        payload << "{\"to\":\"" << request->to() << "\","
                << "\"message\":\"" << request->message() << "\"}";

        std::string notification_id = QueueNotification(
            tenant_ctx.tenant_id,
            request->user_id(),
            NotificationChannel::SMS,
            payload.str()
        );

        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();

        response->set_id(notification_id);
        response->set_channel(NotificationChannel::SMS);
        response->set_status(NotificationStatus::SENT);
        response->set_created_at(timestamp);
        response->set_sent_at(timestamp);
        response->set_retry_count(0);

        return grpc::Status::OK;

    } catch (const std::exception& e) {
        return grpc::Status(grpc::StatusCode::INTERNAL, std::string("Send SMS failed: ") + e.what());
    }
}

grpc::Status NotificationServiceImpl::SendPush(
    grpc::ServerContext* context,
    const SendPushRequest* request,
    NotificationResponse* response
) {
    try {
        auto tenant_ctx = common::TenantContextInterceptor::ExtractFromMetadata(context);

        if (tenant_ctx.tenant_id.empty()) {
            return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "Authentication required");
        }

        // Check user preferences
        if (!CheckUserPreferences(request->user_id(), NotificationChannel::PUSH)) {
            return grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "Push notifications disabled for user");
        }

        // Mock push notification (in production, call FCM API)
        std::cout << "Mock: Sending push notification: " << request->title() << std::endl;

        // Queue notification
        std::stringstream payload;
        payload << "{\"title\":\"" << request->title() << "\","
                << "\"body\":\"" << request->body() << "\"}";

        std::string notification_id = QueueNotification(
            tenant_ctx.tenant_id,
            request->user_id(),
            NotificationChannel::PUSH,
            payload.str()
        );

        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();

        response->set_id(notification_id);
        response->set_channel(NotificationChannel::PUSH);
        response->set_status(NotificationStatus::SENT);
        response->set_created_at(timestamp);
        response->set_sent_at(timestamp);
        response->set_retry_count(0);

        return grpc::Status::OK;

    } catch (const std::exception& e) {
        return grpc::Status(grpc::StatusCode::INTERNAL, std::string("Send push failed: ") + e.what());
    }
}

grpc::Status NotificationServiceImpl::TriggerWebhook(
    grpc::ServerContext* context,
    const TriggerWebhookRequest* request,
    NotificationResponse* response
) {
    try {
        auto tenant_ctx = common::TenantContextInterceptor::ExtractFromMetadata(context);

        if (tenant_ctx.tenant_id.empty()) {
            return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "Authentication required");
        }

        // Get webhook URL from database
        auto conn_guard = db_pool_->AcquireConnection();
        pqxx::work txn(*conn_guard);

        auto webhook_result = txn.exec_params(
            "SELECT url, status FROM webhooks WHERE id = $1 AND tenant_id = $2",
            request->webhook_id(),
            tenant_ctx.tenant_id
        );

        if (webhook_result.empty()) {
            return grpc::Status(grpc::StatusCode::NOT_FOUND, "Webhook not found");
        }

        std::string webhook_url = webhook_result[0]["url"].as<std::string>();
        std::string webhook_status = webhook_result[0]["status"].as<std::string>();

        if (webhook_status != "active") {
            return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, "Webhook is not active");
        }

        // Mock webhook call (in production, make HTTP POST to webhook URL)
        std::cout << "Mock: Triggering webhook " << webhook_url << " with event " << request->event_type() << std::endl;

        // Record webhook trigger
        auto result = txn.exec_params(
            "INSERT INTO notifications (tenant_id, user_id, channel, status, payload, created_at, sent_at) "
            "VALUES ($1, '', $2, $3, $4, NOW(), NOW()) "
            "RETURNING id, EXTRACT(EPOCH FROM created_at)::bigint as created_at",
            tenant_ctx.tenant_id,
            static_cast<int>(NotificationChannel::WEBHOOK),
            static_cast<int>(NotificationStatus::SENT),
            request->payload()
        );

        // Update webhook last_triggered_at
        txn.exec_params(
            "UPDATE webhooks SET last_triggered_at = NOW() WHERE id = $1",
            request->webhook_id()
        );

        auto row = result[0];
        response->set_id(row["id"].as<std::string>());
        response->set_channel(NotificationChannel::WEBHOOK);
        response->set_status(NotificationStatus::SENT);
        response->set_created_at(row["created_at"].as<long long>());
        response->set_sent_at(row["created_at"].as<long long>());
        response->set_retry_count(0);

        txn.commit();

        return grpc::Status::OK;

    } catch (const std::exception& e) {
        return grpc::Status(grpc::StatusCode::INTERNAL, std::string("Trigger webhook failed: ") + e.what());
    }
}

grpc::Status NotificationServiceImpl::GetNotificationStatus(
    grpc::ServerContext* context,
    const GetNotificationStatusRequest* request,
    NotificationResponse* response
) {
    try {
        auto tenant_ctx = common::TenantContextInterceptor::ExtractFromMetadata(context);

        if (tenant_ctx.tenant_id.empty()) {
            return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "Authentication required");
        }

        auto conn_guard = db_pool_->AcquireConnection();
        pqxx::work txn(*conn_guard);

        auto result = txn.exec_params(
            "SELECT id, channel, status, "
            "EXTRACT(EPOCH FROM created_at)::bigint as created_at, "
            "EXTRACT(EPOCH FROM sent_at)::bigint as sent_at, "
            "EXTRACT(EPOCH FROM delivered_at)::bigint as delivered_at, "
            "retry_count "
            "FROM notifications WHERE id = $1 AND tenant_id = $2",
            request->notification_id(),
            tenant_ctx.tenant_id
        );

        if (result.empty()) {
            return grpc::Status(grpc::StatusCode::NOT_FOUND, "Notification not found");
        }

        auto row = result[0];
        response->set_id(row["id"].as<std::string>());
        response->set_channel(static_cast<NotificationChannel>(row["channel"].as<int>()));
        response->set_status(static_cast<NotificationStatus>(row["status"].as<int>()));
        response->set_created_at(row["created_at"].as<long long>());

        if (!row["sent_at"].is_null()) {
            response->set_sent_at(row["sent_at"].as<long long>());
        }
        if (!row["delivered_at"].is_null()) {
            response->set_delivered_at(row["delivered_at"].as<long long>());
        }

        response->set_retry_count(row["retry_count"].as<int>());

        txn.commit();

        return grpc::Status::OK;

    } catch (const std::exception& e) {
        return grpc::Status(grpc::StatusCode::INTERNAL, std::string("Get notification status failed: ") + e.what());
    }
}

grpc::Status NotificationServiceImpl::UpdatePreferences(
    grpc::ServerContext* context,
    const UpdatePreferencesRequest* request,
    PreferencesResponse* response
) {
    try {
        auto tenant_ctx = common::TenantContextInterceptor::ExtractFromMetadata(context);

        if (tenant_ctx.tenant_id.empty()) {
            return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "Authentication required");
        }

        auto conn_guard = db_pool_->AcquireConnection();
        pqxx::work txn(*conn_guard);

        auto result = txn.exec_params(
            "INSERT INTO notification_preferences (user_id, tenant_id, email_enabled, sms_enabled, push_enabled, marketing_emails) "
            "VALUES ($1, $2, $3, $4, $5, $6) "
            "ON CONFLICT (user_id, tenant_id) DO UPDATE SET "
            "email_enabled = $3, sms_enabled = $4, push_enabled = $5, marketing_emails = $6, updated_at = NOW() "
            "RETURNING user_id, email_enabled, sms_enabled, push_enabled, marketing_emails, EXTRACT(EPOCH FROM updated_at)::bigint as updated_at",
            request->user_id(),
            tenant_ctx.tenant_id,
            request->email_enabled(),
            request->sms_enabled(),
            request->push_enabled(),
            request->marketing_emails()
        );

        auto row = result[0];
        response->set_user_id(row["user_id"].as<std::string>());
        response->set_email_enabled(row["email_enabled"].as<bool>());
        response->set_sms_enabled(row["sms_enabled"].as<bool>());
        response->set_push_enabled(row["push_enabled"].as<bool>());
        response->set_marketing_emails(row["marketing_emails"].as<bool>());
        response->set_updated_at(row["updated_at"].as<long long>());

        txn.commit();

        return grpc::Status::OK;

    } catch (const std::exception& e) {
        return grpc::Status(grpc::StatusCode::INTERNAL, std::string("Update preferences failed: ") + e.what());
    }
}

grpc::Status NotificationServiceImpl::RegisterWebhook(
    grpc::ServerContext* context,
    const RegisterWebhookRequest* request,
    WebhookResponse* response
) {
    try {
        auto tenant_ctx = common::TenantContextInterceptor::ExtractFromMetadata(context);

        if (tenant_ctx.tenant_id.empty()) {
            return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "Authentication required");
        }

        // Validate webhook URL (basic validation)
        if (request->url().empty() || request->url().find("http") != 0) {
            return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Invalid webhook URL");
        }

        // Convert repeated events to comma-separated string
        std::stringstream events_ss;
        for (int i = 0; i < request->events_size(); i++) {
            if (i > 0) events_ss << ",";
            events_ss << request->events(i);
        }
        std::string events_str = events_ss.str();

        auto conn_guard = db_pool_->AcquireConnection();
        pqxx::work txn(*conn_guard);

        std::string secret = request->has_secret() ? request->secret() : "";

        auto result = txn.exec_params(
            "INSERT INTO webhooks (tenant_id, url, events, secret, status, failure_count) "
            "VALUES ($1, $2, $3, $4, 'active', 0) "
            "RETURNING id, url, events, status, EXTRACT(EPOCH FROM created_at)::bigint as created_at, failure_count",
            tenant_ctx.tenant_id,
            request->url(),
            events_str,
            secret
        );

        auto row = result[0];
        response->set_id(row["id"].as<std::string>());
        response->set_url(row["url"].as<std::string>());

        // Parse events back into repeated field
        std::string events_from_db = row["events"].as<std::string>();
        std::stringstream ss(events_from_db);
        std::string event;
        while (std::getline(ss, event, ',')) {
            response->add_events(event);
        }

        response->set_status(row["status"].as<std::string>());
        response->set_created_at(row["created_at"].as<long long>());
        response->set_failure_count(row["failure_count"].as<int>());

        txn.commit();

        return grpc::Status::OK;

    } catch (const std::exception& e) {
        return grpc::Status(grpc::StatusCode::INTERNAL, std::string("Register webhook failed: ") + e.what());
    }
}

// Helper methods

bool NotificationServiceImpl::CheckUserPreferences(const std::string& user_id, NotificationChannel channel) {
    try {
        auto conn_guard = db_pool_->AcquireConnection();
        pqxx::work txn(*conn_guard);

        auto result = txn.exec_params(
            "SELECT email_enabled, sms_enabled, push_enabled FROM notification_preferences WHERE user_id = $1",
            user_id
        );

        if (result.empty()) {
            return true; // Allow by default if no preferences set
        }

        auto row = result[0];
        switch (channel) {
            case NotificationChannel::EMAIL:
                return row["email_enabled"].as<bool>();
            case NotificationChannel::SMS:
                return row["sms_enabled"].as<bool>();
            case NotificationChannel::PUSH:
                return row["push_enabled"].as<bool>();
            default:
                return true;
        }

    } catch (const std::exception& e) {
        std::cerr << "Check preferences failed: " << e.what() << std::endl;
        return true; // Fail open
    }
}

std::string NotificationServiceImpl::QueueNotification(
    const std::string& tenant_id,
    const std::string& user_id,
    NotificationChannel channel,
    const std::string& payload
) {
    auto conn_guard = db_pool_->AcquireConnection();
    pqxx::work txn(*conn_guard);

    auto result = txn.exec_params(
        "INSERT INTO notifications (tenant_id, user_id, channel, status, payload, created_at, sent_at, retry_count) "
        "VALUES ($1, $2, $3, $4, $5, NOW(), NOW(), 0) "
        "RETURNING id",
        tenant_id,
        user_id,
        static_cast<int>(channel),
        static_cast<int>(NotificationStatus::SENT),
        payload
    );

    std::string notification_id = result[0]["id"].as<std::string>();
    txn.commit();

    return notification_id;
}

} // namespace notification
} // namespace saasforge
