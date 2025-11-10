#pragma once

#include <grpcpp/grpcpp.h>

namespace saasforge {
namespace payment {

class PaymentServiceImpl {
public:
    PaymentServiceImpl();

    // TODO: Implement methods from payment.proto:
    // - CreateSubscription
    // - UpdateSubscription
    // - CancelSubscription
    // - AddPaymentMethod
    // - RemovePaymentMethod
    // - ProcessPayment
    // - GetInvoice
    // - RecordUsage
};

} // namespace payment
} // namespace saasforge
