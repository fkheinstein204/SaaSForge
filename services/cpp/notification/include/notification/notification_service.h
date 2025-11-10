#pragma once

#include <grpcpp/grpcpp.h>

namespace saasforge {
namespace notification {

class NotificationServiceImpl {
public:
    NotificationServiceImpl();

    // TODO: Implement methods from notification.proto:
    // - SendEmail
    // - SendSMS
    // - SendPush
    // - TriggerWebhook
    // - GetNotificationStatus
    // - UpdatePreferences
    // - RegisterWebhook
};

} // namespace notification
} // namespace saasforge
