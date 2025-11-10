#include "payment/payment_service.h"
#include <iostream>

namespace saasforge {
namespace payment {

PaymentServiceImpl::PaymentServiceImpl() {
    std::cout << "PaymentService initialized" << std::endl;
}

} // namespace payment
} // namespace saasforge
