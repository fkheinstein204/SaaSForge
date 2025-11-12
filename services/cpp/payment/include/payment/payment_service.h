#pragma once

#include <grpcpp/grpcpp.h>
#include <memory>
#include "payment.grpc.pb.h"
#include "common/redis_client.h"
#include "common/db_pool.h"

namespace saasforge {
namespace payment {

class PaymentServiceImpl final : public ::saasforge::payment::PaymentService::Service {
public:
    PaymentServiceImpl(
        std::shared_ptr<common::RedisClient> redis_client,
        std::shared_ptr<common::DbPool> db_pool,
        const std::string& stripe_secret_key,
        const std::string& stripe_webhook_secret
    );

    grpc::Status CreateSubscription(
        grpc::ServerContext* context,
        const CreateSubscriptionRequest* request,
        SubscriptionResponse* response
    ) override;

    grpc::Status UpdateSubscription(
        grpc::ServerContext* context,
        const UpdateSubscriptionRequest* request,
        SubscriptionResponse* response
    ) override;

    grpc::Status CancelSubscription(
        grpc::ServerContext* context,
        const CancelSubscriptionRequest* request,
        SubscriptionResponse* response
    ) override;

    grpc::Status GetSubscription(
        grpc::ServerContext* context,
        const GetSubscriptionRequest* request,
        SubscriptionResponse* response
    ) override;

    grpc::Status AddPaymentMethod(
        grpc::ServerContext* context,
        const AddPaymentMethodRequest* request,
        PaymentMethodResponse* response
    ) override;

    grpc::Status RemovePaymentMethod(
        grpc::ServerContext* context,
        const RemovePaymentMethodRequest* request,
        RemovePaymentMethodResponse* response
    ) override;

    grpc::Status GetInvoice(
        grpc::ServerContext* context,
        const GetInvoiceRequest* request,
        InvoiceResponse* response
    ) override;

    grpc::Status RecordUsage(
        grpc::ServerContext* context,
        const RecordUsageRequest* request,
        RecordUsageResponse* response
    ) override;

private:
    std::shared_ptr<common::RedisClient> redis_client_;
    std::shared_ptr<common::DbPool> db_pool_;
    std::string stripe_secret_key_;
    std::string stripe_webhook_secret_;

    // Helper methods
    std::string GenerateMockStripeId(const std::string& prefix);
    double CalculateMRR(const std::string& plan_id, int32_t quantity);
};

} // namespace payment
} // namespace saasforge
