/**
 * @author Heinstein F (@heinsteinh)
 * @created 2025-11-13
 * @description Mock Stripe client for payment processing
 *
 * Provides mock implementations of Stripe API operations for testing and development.
 * In production, this would be replaced with actual Stripe SDK calls.
 */

#pragma once

#include <string>
#include <vector>
#include <optional>
#include <cstdint>
#include <map>

namespace saasforge {
namespace common {

/**
 * Subscription status (Requirement C-63)
 */
enum class SubscriptionStatus {
    TRIALING,      // In trial period
    ACTIVE,        // Active and paid
    PAST_DUE,      // Payment failed, in grace period
    CANCELED,      // Canceled by user
    UNPAID,        // Payment failed after retries
    PAUSED         // Temporarily paused
};

/**
 * Payment intent status
 */
enum class PaymentIntentStatus {
    SUCCEEDED,
    FAILED,
    PROCESSING,
    REQUIRES_ACTION  // SCA/3DS required
};

/**
 * Mock Stripe customer
 */
struct StripeCustomer {
    std::string id;              // cus_XXXXX
    std::string email;
    std::string tenant_id;
    std::string default_payment_method;
    int64_t created;
};

/**
 * Mock Stripe subscription
 */
struct StripeSubscription {
    std::string id;              // sub_XXXXX
    std::string customer_id;
    std::string plan_id;
    SubscriptionStatus status;
    int64_t current_period_start;
    int64_t current_period_end;
    int64_t trial_end;
    bool cancel_at_period_end;
    double amount;               // Monthly amount in dollars
    int retry_count;             // Failed payment retry attempts
};

/**
 * Mock Stripe payment method
 */
struct StripePaymentMethod {
    std::string id;              // pm_XXXXX
    std::string type;            // "card", "ach_debit", "sepa_debit"
    std::string card_last4;
    std::string card_brand;      // "visa", "mastercard", etc.
    int exp_month;
    int exp_year;
};

/**
 * Mock Stripe invoice
 */
struct StripeInvoice {
    std::string id;              // in_XXXXX
    std::string customer_id;
    std::string subscription_id;
    double amount_due;
    double amount_paid;
    std::string status;          // "draft", "open", "paid", "void"
    int64_t created;
    int64_t due_date;
    std::vector<std::string> line_items;
};

/**
 * Mock Stripe Client
 *
 * Simulates Stripe API operations for development and testing.
 * Maintains in-memory state for subscriptions, customers, and payment methods.
 */
class MockStripeClient {
public:
    MockStripeClient(const std::string& api_key);

    // Customer operations
    StripeCustomer CreateCustomer(const std::string& email, const std::string& tenant_id);
    std::optional<StripeCustomer> GetCustomer(const std::string& customer_id);

    // Subscription operations
    StripeSubscription CreateSubscription(
        const std::string& customer_id,
        const std::string& plan_id,
        int trial_days = 0
    );
    std::optional<StripeSubscription> GetSubscription(const std::string& subscription_id);
    StripeSubscription UpdateSubscription(
        const std::string& subscription_id,
        const std::string& new_plan_id
    );
    void CancelSubscription(const std::string& subscription_id, bool cancel_immediately = false);

    // Payment method operations
    StripePaymentMethod CreatePaymentMethod(
        const std::string& type,
        const std::string& card_number = "",
        int exp_month = 0,
        int exp_year = 0
    );
    void AttachPaymentMethod(const std::string& payment_method_id, const std::string& customer_id);
    void DetachPaymentMethod(const std::string& payment_method_id);
    std::optional<StripePaymentMethod> GetPaymentMethod(const std::string& payment_method_id);

    // Invoice operations
    StripeInvoice CreateInvoice(const std::string& customer_id, const std::string& subscription_id);
    std::optional<StripeInvoice> GetInvoice(const std::string& invoice_id);
    void FinalizeInvoice(const std::string& invoice_id);
    PaymentIntentStatus PayInvoice(const std::string& invoice_id);

    // Payment retry logic (Requirement C-71)
    void RecordPaymentFailure(const std::string& subscription_id);
    bool ShouldRetryPayment(const std::string& subscription_id);
    void SchedulePaymentRetry(const std::string& subscription_id, int retry_count);

    // Subscription state transitions (Requirement C-64)
    void TransitionSubscriptionState(const std::string& subscription_id, SubscriptionStatus new_status);

    // Helper methods
    static std::string StatusToString(SubscriptionStatus status);
    static SubscriptionStatus StringToStatus(const std::string& status_str);

private:
    std::string api_key_;

    // In-memory storage (in production, Stripe manages this)
    std::map<std::string, StripeCustomer> customers_;
    std::map<std::string, StripeSubscription> subscriptions_;
    std::map<std::string, StripePaymentMethod> payment_methods_;
    std::map<std::string, StripeInvoice> invoices_;

    // ID generators
    std::string GenerateId(const std::string& prefix);
    int64_t GetCurrentTimestamp();
};

} // namespace common
} // namespace saasforge
