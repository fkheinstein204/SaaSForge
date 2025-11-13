/**
 * @author Heinstein F (@heinsteinh)
 * @created 2025-11-13
 * @description Mock Stripe client implementation
 */

#include "common/mock_stripe_client.h"
#include <random>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <iostream>

namespace saasforge {
namespace common {

MockStripeClient::MockStripeClient(const std::string& api_key)
    : api_key_(api_key) {
}

// Customer operations

StripeCustomer MockStripeClient::CreateCustomer(const std::string& email, const std::string& tenant_id) {
    StripeCustomer customer;
    customer.id = GenerateId("cus");
    customer.email = email;
    customer.tenant_id = tenant_id;
    customer.created = GetCurrentTimestamp();

    customers_[customer.id] = customer;

    return customer;
}

std::optional<StripeCustomer> MockStripeClient::GetCustomer(const std::string& customer_id) {
    auto it = customers_.find(customer_id);
    if (it != customers_.end()) {
        return it->second;
    }
    return std::nullopt;
}

// Subscription operations

StripeSubscription MockStripeClient::CreateSubscription(
    const std::string& customer_id,
    const std::string& plan_id,
    int trial_days
) {
    StripeSubscription subscription;
    subscription.id = GenerateId("sub");
    subscription.customer_id = customer_id;
    subscription.plan_id = plan_id;
    subscription.current_period_start = GetCurrentTimestamp();
    subscription.current_period_end = GetCurrentTimestamp() + (30 * 24 * 3600); // 30 days
    subscription.cancel_at_period_end = false;
    subscription.retry_count = 0;

    // Handle trial period (Requirement C-65)
    if (trial_days > 0) {
        subscription.status = SubscriptionStatus::TRIALING;
        subscription.trial_end = GetCurrentTimestamp() + (trial_days * 24 * 3600);
    } else {
        subscription.status = SubscriptionStatus::ACTIVE;
        subscription.trial_end = 0;
    }

    // Calculate monthly amount based on plan
    if (plan_id == "free") {
        subscription.amount = 0.0;
    } else if (plan_id == "pro") {
        subscription.amount = 29.0;
    } else if (plan_id == "enterprise") {
        subscription.amount = 99.0;
    } else {
        subscription.amount = 0.0;
    }

    subscriptions_[subscription.id] = subscription;

    return subscription;
}

std::optional<StripeSubscription> MockStripeClient::GetSubscription(const std::string& subscription_id) {
    auto it = subscriptions_.find(subscription_id);
    if (it != subscriptions_.end()) {
        return it->second;
    }
    return std::nullopt;
}

StripeSubscription MockStripeClient::UpdateSubscription(
    const std::string& subscription_id,
    const std::string& new_plan_id
) {
    auto it = subscriptions_.find(subscription_id);
    if (it == subscriptions_.end()) {
        throw std::runtime_error("Subscription not found");
    }

    auto& subscription = it->second;
    std::string old_plan = subscription.plan_id;
    subscription.plan_id = new_plan_id;

    // Update amount (Requirement C-74: proration will be calculated separately)
    if (new_plan_id == "free") {
        subscription.amount = 0.0;
    } else if (new_plan_id == "pro") {
        subscription.amount = 29.0;
    } else if (new_plan_id == "enterprise") {
        subscription.amount = 99.0;
    }

    std::cout << "Subscription " << subscription_id << " updated from "
              << old_plan << " to " << new_plan_id << std::endl;

    return subscription;
}

void MockStripeClient::CancelSubscription(const std::string& subscription_id, bool cancel_immediately) {
    auto it = subscriptions_.find(subscription_id);
    if (it == subscriptions_.end()) {
        throw std::runtime_error("Subscription not found");
    }

    auto& subscription = it->second;

    if (cancel_immediately) {
        subscription.status = SubscriptionStatus::CANCELED;
        std::cout << "Subscription " << subscription_id << " canceled immediately" << std::endl;
    } else {
        subscription.cancel_at_period_end = true;
        std::cout << "Subscription " << subscription_id << " will cancel at period end" << std::endl;
    }
}

// Payment method operations

StripePaymentMethod MockStripeClient::CreatePaymentMethod(
    const std::string& type,
    const std::string& card_number,
    int exp_month,
    int exp_year
) {
    StripePaymentMethod pm;
    pm.id = GenerateId("pm");
    pm.type = type;

    if (type == "card" && !card_number.empty()) {
        pm.card_last4 = card_number.substr(card_number.length() - 4);
        pm.card_brand = "visa";  // Mock brand
        pm.exp_month = exp_month;
        pm.exp_year = exp_year;
    }

    payment_methods_[pm.id] = pm;

    return pm;
}

void MockStripeClient::AttachPaymentMethod(const std::string& payment_method_id, const std::string& customer_id) {
    auto pm_it = payment_methods_.find(payment_method_id);
    if (pm_it == payment_methods_.end()) {
        throw std::runtime_error("Payment method not found");
    }

    auto cust_it = customers_.find(customer_id);
    if (cust_it == customers_.end()) {
        throw std::runtime_error("Customer not found");
    }

    cust_it->second.default_payment_method = payment_method_id;
    std::cout << "Payment method " << payment_method_id << " attached to customer " << customer_id << std::endl;
}

void MockStripeClient::DetachPaymentMethod(const std::string& payment_method_id) {
    // Remove from customer default payment method
    for (auto& [customer_id, customer] : customers_) {
        if (customer.default_payment_method == payment_method_id) {
            customer.default_payment_method.clear();
        }
    }

    payment_methods_.erase(payment_method_id);
    std::cout << "Payment method " << payment_method_id << " detached" << std::endl;
}

std::optional<StripePaymentMethod> MockStripeClient::GetPaymentMethod(const std::string& payment_method_id) {
    auto it = payment_methods_.find(payment_method_id);
    if (it != payment_methods_.end()) {
        return it->second;
    }
    return std::nullopt;
}

// Invoice operations

StripeInvoice MockStripeClient::CreateInvoice(
    const std::string& customer_id,
    const std::string& subscription_id
) {
    auto sub_opt = GetSubscription(subscription_id);
    if (!sub_opt) {
        throw std::runtime_error("Subscription not found");
    }

    StripeInvoice invoice;
    invoice.id = GenerateId("in");
    invoice.customer_id = customer_id;
    invoice.subscription_id = subscription_id;
    invoice.amount_due = sub_opt->amount;
    invoice.amount_paid = 0.0;
    invoice.status = "draft";
    invoice.created = GetCurrentTimestamp();
    invoice.due_date = GetCurrentTimestamp() + (7 * 24 * 3600); // 7 days from now

    // Add line items (mock)
    invoice.line_items.push_back("Subscription to " + sub_opt->plan_id + " plan");

    invoices_[invoice.id] = invoice;

    return invoice;
}

std::optional<StripeInvoice> MockStripeClient::GetInvoice(const std::string& invoice_id) {
    auto it = invoices_.find(invoice_id);
    if (it != invoices_.end()) {
        return it->second;
    }
    return std::nullopt;
}

void MockStripeClient::FinalizeInvoice(const std::string& invoice_id) {
    auto it = invoices_.find(invoice_id);
    if (it == invoices_.end()) {
        throw std::runtime_error("Invoice not found");
    }

    it->second.status = "open";
    std::cout << "Invoice " << invoice_id << " finalized" << std::endl;
}

PaymentIntentStatus MockStripeClient::PayInvoice(const std::string& invoice_id) {
    auto it = invoices_.find(invoice_id);
    if (it == invoices_.end()) {
        throw std::runtime_error("Invoice not found");
    }

    // Mock payment processing (80% success rate)
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(0.0, 1.0);

    PaymentIntentStatus status;
    if (dis(gen) < 0.8) {
        // Payment succeeded
        it->second.status = "paid";
        it->second.amount_paid = it->second.amount_due;
        status = PaymentIntentStatus::SUCCEEDED;
    } else {
        // Payment failed
        status = PaymentIntentStatus::FAILED;
    }

    return status;
}

// Payment retry logic (Requirement C-71)

void MockStripeClient::RecordPaymentFailure(const std::string& subscription_id) {
    auto it = subscriptions_.find(subscription_id);
    if (it == subscriptions_.end()) {
        return;
    }

    auto& subscription = it->second;
    subscription.retry_count++;

    // Transition to PAST_DUE after first failure
    if (subscription.retry_count == 1) {
        TransitionSubscriptionState(subscription_id, SubscriptionStatus::PAST_DUE);
    }

    // Transition to UNPAID after 3 failures
    if (subscription.retry_count >= 3) {
        TransitionSubscriptionState(subscription_id, SubscriptionStatus::UNPAID);
    }

    std::cout << "Payment failure recorded for subscription " << subscription_id
              << " (retry count: " << subscription.retry_count << ")" << std::endl;
}

bool MockStripeClient::ShouldRetryPayment(const std::string& subscription_id) {
    auto it = subscriptions_.find(subscription_id);
    if (it == subscriptions_.end()) {
        return false;
    }

    // Retry up to 3 times (Day 1, 3, 7)
    return it->second.retry_count < 3;
}

void MockStripeClient::SchedulePaymentRetry(const std::string& subscription_id, int retry_count) {
    // Retry schedule: Day 1, Day 3, Day 7
    int days_until_retry;
    if (retry_count == 0) {
        days_until_retry = 1;
    } else if (retry_count == 1) {
        days_until_retry = 3;
    } else {
        days_until_retry = 7;
    }

    std::cout << "Payment retry scheduled for subscription " << subscription_id
              << " in " << days_until_retry << " days" << std::endl;
}

// Subscription state transitions (Requirement C-64)

void MockStripeClient::TransitionSubscriptionState(
    const std::string& subscription_id,
    SubscriptionStatus new_status
) {
    auto it = subscriptions_.find(subscription_id);
    if (it == subscriptions_.end()) {
        return;
    }

    SubscriptionStatus old_status = it->second.status;
    it->second.status = new_status;

    std::cout << "Subscription " << subscription_id << " transitioned from "
              << StatusToString(old_status) << " to "
              << StatusToString(new_status) << std::endl;
}

// Helper methods

std::string MockStripeClient::StatusToString(SubscriptionStatus status) {
    switch (status) {
        case SubscriptionStatus::TRIALING: return "trialing";
        case SubscriptionStatus::ACTIVE: return "active";
        case SubscriptionStatus::PAST_DUE: return "past_due";
        case SubscriptionStatus::CANCELED: return "canceled";
        case SubscriptionStatus::UNPAID: return "unpaid";
        case SubscriptionStatus::PAUSED: return "paused";
        default: return "unknown";
    }
}

SubscriptionStatus MockStripeClient::StringToStatus(const std::string& status_str) {
    if (status_str == "trialing") return SubscriptionStatus::TRIALING;
    if (status_str == "active") return SubscriptionStatus::ACTIVE;
    if (status_str == "past_due") return SubscriptionStatus::PAST_DUE;
    if (status_str == "canceled") return SubscriptionStatus::CANCELED;
    if (status_str == "unpaid") return SubscriptionStatus::UNPAID;
    if (status_str == "paused") return SubscriptionStatus::PAUSED;
    return SubscriptionStatus::ACTIVE;  // Default
}

std::string MockStripeClient::GenerateId(const std::string& prefix) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);

    std::stringstream ss;
    ss << prefix << "_";
    for (int i = 0; i < 24; i++) {
        ss << std::hex << std::setw(1) << (dis(gen) % 16);
    }

    return ss.str();
}

int64_t MockStripeClient::GetCurrentTimestamp() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

} // namespace common
} // namespace saasforge
