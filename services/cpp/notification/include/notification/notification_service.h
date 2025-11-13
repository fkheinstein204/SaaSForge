#pragma once

#include <grpcpp/grpcpp.h>
#include <memory>
#include "notification.grpc.pb.h"
#include "common/redis_client.h"
#include "common/db_pool.h"

namespace saasforge {
namespace notification {

class NotificationServiceImpl final : public ::saasforge::notification::NotificationService::Service {
public:
    NotificationServiceImpl(
        std::shared_ptr<common::RedisClient> redis_client,
        std::shared_ptr<common::DbPool> db_pool,
        const std::string& sendgrid_api_key,
        const std::string& twilio_account_sid,
        const std::string& twilio_auth_token,
        const std::string& fcm_server_key
    );

    grpc::Status SendEmail(
        grpc::ServerContext* context,
        const SendEmailRequest* request,
        NotificationResponse* response
    ) override;

    grpc::Status SendSMS(
        grpc::ServerContext* context,
        const SendSMSRequest* request,
        NotificationResponse* response
    ) override;

    grpc::Status SendPush(
        grpc::ServerContext* context,
        const SendPushRequest* request,
        NotificationResponse* response
    ) override;

    grpc::Status TriggerWebhook(
        grpc::ServerContext* context,
        const TriggerWebhookRequest* request,
        NotificationResponse* response
    ) override;

    grpc::Status GetNotificationStatus(
        grpc::ServerContext* context,
        const GetNotificationStatusRequest* request,
        NotificationResponse* response
    ) override;

    grpc::Status UpdatePreferences(
        grpc::ServerContext* context,
        const UpdatePreferencesRequest* request,
        PreferencesResponse* response
    ) override;

    grpc::Status RegisterWebhook(
        grpc::ServerContext* context,
        const RegisterWebhookRequest* request,
        WebhookResponse* response
    ) override;

private:
    std::shared_ptr<common::RedisClient> redis_client_;
    std::shared_ptr<common::DbPool> db_pool_;
    std::string sendgrid_api_key_;
    std::string twilio_account_sid_;
    std::string twilio_auth_token_;
    std::string fcm_server_key_;

    // Helper methods
    bool CheckUserPreferences(const std::string& user_id, NotificationChannel channel);
    std::string QueueNotification(const std::string& tenant_id, const std::string& user_id,
                                   NotificationChannel channel, const std::string& payload);
    bool ValidateWebhookUrl(const std::string& url);
};

} // namespace notification
} // namespace saasforge
