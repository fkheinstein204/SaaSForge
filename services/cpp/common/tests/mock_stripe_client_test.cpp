/**
 * @author Heinstein F (@heinsteinh)
 * @created 2025-11-13
 * @description Unit tests for Mock Stripe Client
 */

#include <gtest/gtest.h>
#include "common/mock_stripe_client.h"
#include <thread>
#include <chrono>

namespace saasforge {
namespace common {
namespace test {

class MockStripeClientTest : public ::testing::Test {
protected:
    void SetUp() override {
        client_ = std::make_unique<MockStripeClient>("sk_test_mock_key");
    }

    void TearDown() override {
        client_.reset();
    }

    std::unique_ptr<MockStripeClient> client_;
};

// Test: Customer Operations

TEST_F(MockStripeClientTest, CreateCustomerGeneratesValidId) {
    auto customer = client_->CreateCustomer("test@example.com", "tenant_123");

    // Should have valid Stripe customer ID format
    EXPECT_TRUE(customer.id.find("cus_") == 0);
    EXPECT_GT(customer.id.length(), 4);

    // Should store customer details
    EXPECT_EQ(customer.email, "test@example.com");
    EXPECT_EQ(customer.tenant_id, "tenant_123");

    // Should have creation timestamp
    EXPECT_GT(customer.created, 0);
}

TEST_F(MockStripeClientTest, GetCustomerReturnsCreatedCustomer) {
    auto customer = client_->CreateCustomer("test@example.com", "tenant_123");

    auto retrieved = client_->GetCustomer(customer.id);

    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->id, customer.id);
    EXPECT_EQ(retrieved->email, customer.email);
    EXPECT_EQ(retrieved->tenant_id, customer.tenant_id);
}

TEST_F(MockStripeClientTest, GetCustomerReturnsNulloptForNonexistent) {
    auto retrieved = client_->GetCustomer("cus_nonexistent");

    EXPECT_FALSE(retrieved.has_value());
}

// Test: Subscription Creation

TEST_F(MockStripeClientTest, CreateSubscriptionGeneratesValidId) {
    auto customer = client_->CreateCustomer("test@example.com", "tenant_123");
    auto subscription = client_->CreateSubscription(customer.id, "pro");

    // Should have valid Stripe subscription ID format
    EXPECT_TRUE(subscription.id.find("sub_") == 0);
    EXPECT_GT(subscription.id.length(), 4);

    // Should link to customer
    EXPECT_EQ(subscription.customer_id, customer.id);
    EXPECT_EQ(subscription.plan_id, "pro");
}

TEST_F(MockStripeClientTest, CreateSubscriptionWithoutTrialStartsActive) {
    auto customer = client_->CreateCustomer("test@example.com", "tenant_123");
    auto subscription = client_->CreateSubscription(customer.id, "pro", 0);

    EXPECT_EQ(subscription.status, SubscriptionStatus::ACTIVE);
    EXPECT_EQ(subscription.trial_end, 0);
}

TEST_F(MockStripeClientTest, CreateSubscriptionWithTrialStartsTrialing) {
    auto customer = client_->CreateCustomer("test@example.com", "tenant_123");
    auto subscription = client_->CreateSubscription(customer.id, "pro", 14);

    EXPECT_EQ(subscription.status, SubscriptionStatus::TRIALING);
    EXPECT_GT(subscription.trial_end, 0);

    // Trial should be approximately 14 days from now
    auto now = std::chrono::system_clock::now().time_since_epoch().count() / 1000000000;
    int64_t expected_trial_end = now + (14 * 24 * 3600);
    EXPECT_NEAR(subscription.trial_end, expected_trial_end, 5);  // Within 5 seconds
}

TEST_F(MockStripeClientTest, CreateSubscriptionCalculatesCorrectAmount) {
    auto customer = client_->CreateCustomer("test@example.com", "tenant_123");

    auto free_sub = client_->CreateSubscription(customer.id, "free");
    EXPECT_EQ(free_sub.amount, 0.0);

    auto pro_sub = client_->CreateSubscription(customer.id, "pro");
    EXPECT_EQ(pro_sub.amount, 29.0);

    auto enterprise_sub = client_->CreateSubscription(customer.id, "enterprise");
    EXPECT_EQ(enterprise_sub.amount, 99.0);
}

// Test: Subscription Retrieval

TEST_F(MockStripeClientTest, GetSubscriptionReturnsCreatedSubscription) {
    auto customer = client_->CreateCustomer("test@example.com", "tenant_123");
    auto subscription = client_->CreateSubscription(customer.id, "pro");

    auto retrieved = client_->GetSubscription(subscription.id);

    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->id, subscription.id);
    EXPECT_EQ(retrieved->customer_id, customer.id);
    EXPECT_EQ(retrieved->plan_id, "pro");
    EXPECT_EQ(retrieved->status, subscription.status);
}

TEST_F(MockStripeClientTest, GetSubscriptionReturnsNulloptForNonexistent) {
    auto retrieved = client_->GetSubscription("sub_nonexistent");

    EXPECT_FALSE(retrieved.has_value());
}

// Test: Subscription Updates

TEST_F(MockStripeClientTest, UpdateSubscriptionChangePlanAndAmount) {
    auto customer = client_->CreateCustomer("test@example.com", "tenant_123");
    auto subscription = client_->CreateSubscription(customer.id, "pro");

    EXPECT_EQ(subscription.plan_id, "pro");
    EXPECT_EQ(subscription.amount, 29.0);

    auto updated = client_->UpdateSubscription(subscription.id, "enterprise");

    EXPECT_EQ(updated.plan_id, "enterprise");
    EXPECT_EQ(updated.amount, 99.0);
}

TEST_F(MockStripeClientTest, UpdateSubscriptionPersistsChanges) {
    auto customer = client_->CreateCustomer("test@example.com", "tenant_123");
    auto subscription = client_->CreateSubscription(customer.id, "pro");

    client_->UpdateSubscription(subscription.id, "enterprise");

    auto retrieved = client_->GetSubscription(subscription.id);
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->plan_id, "enterprise");
    EXPECT_EQ(retrieved->amount, 99.0);
}

// Test: Subscription Cancellation

TEST_F(MockStripeClientTest, CancelSubscriptionImmediately) {
    auto customer = client_->CreateCustomer("test@example.com", "tenant_123");
    auto subscription = client_->CreateSubscription(customer.id, "pro");

    EXPECT_EQ(subscription.status, SubscriptionStatus::ACTIVE);
    EXPECT_FALSE(subscription.cancel_at_period_end);

    client_->CancelSubscription(subscription.id, true);  // Immediate

    auto retrieved = client_->GetSubscription(subscription.id);
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->status, SubscriptionStatus::CANCELED);
}

TEST_F(MockStripeClientTest, CancelSubscriptionAtPeriodEnd) {
    auto customer = client_->CreateCustomer("test@example.com", "tenant_123");
    auto subscription = client_->CreateSubscription(customer.id, "pro");

    client_->CancelSubscription(subscription.id, false);  // At period end

    auto retrieved = client_->GetSubscription(subscription.id);
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->status, SubscriptionStatus::ACTIVE);  // Still active
    EXPECT_TRUE(retrieved->cancel_at_period_end);  // Marked for cancellation
}

// Test: Payment Methods

TEST_F(MockStripeClientTest, CreatePaymentMethodGeneratesValidId) {
    auto pm = client_->CreatePaymentMethod("card", "4242424242424242", 12, 2025);

    EXPECT_TRUE(pm.id.find("pm_") == 0);
    EXPECT_EQ(pm.type, "card");
    EXPECT_EQ(pm.card_last4, "4242");
    EXPECT_EQ(pm.exp_month, 12);
    EXPECT_EQ(pm.exp_year, 2025);
}

TEST_F(MockStripeClientTest, AttachPaymentMethodToCustomer) {
    auto customer = client_->CreateCustomer("test@example.com", "tenant_123");
    auto pm = client_->CreatePaymentMethod("card", "4242424242424242", 12, 2025);

    EXPECT_TRUE(customer.default_payment_method.empty());

    client_->AttachPaymentMethod(pm.id, customer.id);

    auto retrieved_customer = client_->GetCustomer(customer.id);
    ASSERT_TRUE(retrieved_customer.has_value());
    EXPECT_EQ(retrieved_customer->default_payment_method, pm.id);
}

TEST_F(MockStripeClientTest, DetachPaymentMethod) {
    auto customer = client_->CreateCustomer("test@example.com", "tenant_123");
    auto pm = client_->CreatePaymentMethod("card", "4242424242424242", 12, 2025);

    client_->AttachPaymentMethod(pm.id, customer.id);
    client_->DetachPaymentMethod(pm.id);

    // Customer should no longer have default payment method
    auto retrieved_customer = client_->GetCustomer(customer.id);
    ASSERT_TRUE(retrieved_customer.has_value());
    EXPECT_TRUE(retrieved_customer->default_payment_method.empty());

    // Payment method should no longer exist
    auto retrieved_pm = client_->GetPaymentMethod(pm.id);
    EXPECT_FALSE(retrieved_pm.has_value());
}

// Test: Invoice Operations

TEST_F(MockStripeClientTest, CreateInvoiceGeneratesValidId) {
    auto customer = client_->CreateCustomer("test@example.com", "tenant_123");
    auto subscription = client_->CreateSubscription(customer.id, "pro");

    auto invoice = client_->CreateInvoice(customer.id, subscription.id);

    EXPECT_TRUE(invoice.id.find("in_") == 0);
    EXPECT_EQ(invoice.customer_id, customer.id);
    EXPECT_EQ(invoice.subscription_id, subscription.id);
    EXPECT_EQ(invoice.status, "draft");
}

TEST_F(MockStripeClientTest, CreateInvoiceCalculatesAmountFromSubscription) {
    auto customer = client_->CreateCustomer("test@example.com", "tenant_123");
    auto subscription = client_->CreateSubscription(customer.id, "pro");

    auto invoice = client_->CreateInvoice(customer.id, subscription.id);

    EXPECT_EQ(invoice.amount_due, 29.0);  // Pro plan
    EXPECT_EQ(invoice.amount_paid, 0.0);
}

TEST_F(MockStripeClientTest, FinalizeInvoiceChangesStatus) {
    auto customer = client_->CreateCustomer("test@example.com", "tenant_123");
    auto subscription = client_->CreateSubscription(customer.id, "pro");
    auto invoice = client_->CreateInvoice(customer.id, subscription.id);

    EXPECT_EQ(invoice.status, "draft");

    client_->FinalizeInvoice(invoice.id);

    auto retrieved = client_->GetInvoice(invoice.id);
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->status, "open");
}

TEST_F(MockStripeClientTest, PayInvoiceUpdatesStatusAndAmount) {
    auto customer = client_->CreateCustomer("test@example.com", "tenant_123");
    auto subscription = client_->CreateSubscription(customer.id, "pro");
    auto invoice = client_->CreateInvoice(customer.id, subscription.id);

    client_->FinalizeInvoice(invoice.id);

    // Payment has 80% success rate, so we may need multiple attempts
    bool payment_succeeded = false;
    for (int i = 0; i < 10; ++i) {
        auto status = client_->PayInvoice(invoice.id);
        if (status == PaymentIntentStatus::SUCCEEDED) {
            payment_succeeded = true;
            break;
        }
    }

    // At least one payment should succeed in 10 attempts (probability: 1 - 0.2^10 ≈ 99.9999%)
    EXPECT_TRUE(payment_succeeded);

    // If payment succeeded, check invoice status
    if (payment_succeeded) {
        auto retrieved = client_->GetInvoice(invoice.id);
        ASSERT_TRUE(retrieved.has_value());
        EXPECT_EQ(retrieved->status, "paid");
        EXPECT_EQ(retrieved->amount_paid, retrieved->amount_due);
    }
}

// Test: Payment Retry Logic (Requirement C-71)

TEST_F(MockStripeClientTest, RecordPaymentFailureIncrementsRetryCount) {
    auto customer = client_->CreateCustomer("test@example.com", "tenant_123");
    auto subscription = client_->CreateSubscription(customer.id, "pro");

    EXPECT_EQ(subscription.retry_count, 0);

    client_->RecordPaymentFailure(subscription.id);

    auto retrieved = client_->GetSubscription(subscription.id);
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->retry_count, 1);
}

TEST_F(MockStripeClientTest, RecordPaymentFailureTransitionsToPastDue) {
    auto customer = client_->CreateCustomer("test@example.com", "tenant_123");
    auto subscription = client_->CreateSubscription(customer.id, "pro");

    EXPECT_EQ(subscription.status, SubscriptionStatus::ACTIVE);

    client_->RecordPaymentFailure(subscription.id);

    auto retrieved = client_->GetSubscription(subscription.id);
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->status, SubscriptionStatus::PAST_DUE);
}

TEST_F(MockStripeClientTest, RecordPaymentFailureAfterThreeRetriesTransitionsToUnpaid) {
    auto customer = client_->CreateCustomer("test@example.com", "tenant_123");
    auto subscription = client_->CreateSubscription(customer.id, "pro");

    // Record 3 failures
    client_->RecordPaymentFailure(subscription.id);
    client_->RecordPaymentFailure(subscription.id);
    client_->RecordPaymentFailure(subscription.id);

    auto retrieved = client_->GetSubscription(subscription.id);
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->retry_count, 3);
    EXPECT_EQ(retrieved->status, SubscriptionStatus::UNPAID);
}

TEST_F(MockStripeClientTest, ShouldRetryPaymentReturnsTrueUnderThreeFailures) {
    auto customer = client_->CreateCustomer("test@example.com", "tenant_123");
    auto subscription = client_->CreateSubscription(customer.id, "pro");

    EXPECT_TRUE(client_->ShouldRetryPayment(subscription.id));

    client_->RecordPaymentFailure(subscription.id);
    EXPECT_TRUE(client_->ShouldRetryPayment(subscription.id));

    client_->RecordPaymentFailure(subscription.id);
    EXPECT_TRUE(client_->ShouldRetryPayment(subscription.id));
}

TEST_F(MockStripeClientTest, ShouldRetryPaymentReturnsFalseAfterThreeFailures) {
    auto customer = client_->CreateCustomer("test@example.com", "tenant_123");
    auto subscription = client_->CreateSubscription(customer.id, "pro");

    // Record 3 failures
    client_->RecordPaymentFailure(subscription.id);
    client_->RecordPaymentFailure(subscription.id);
    client_->RecordPaymentFailure(subscription.id);

    EXPECT_FALSE(client_->ShouldRetryPayment(subscription.id));
}

// Test: Subscription State Machine (Requirement C-64)

TEST_F(MockStripeClientTest, TransitionSubscriptionStateUpdatesStatus) {
    auto customer = client_->CreateCustomer("test@example.com", "tenant_123");
    auto subscription = client_->CreateSubscription(customer.id, "pro");

    EXPECT_EQ(subscription.status, SubscriptionStatus::ACTIVE);

    client_->TransitionSubscriptionState(subscription.id, SubscriptionStatus::PAUSED);

    auto retrieved = client_->GetSubscription(subscription.id);
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->status, SubscriptionStatus::PAUSED);
}

TEST_F(MockStripeClientTest, SubscriptionStateSequence) {
    auto customer = client_->CreateCustomer("test@example.com", "tenant_123");
    auto subscription = client_->CreateSubscription(customer.id, "pro", 7);  // 7-day trial

    // Should start in TRIALING
    EXPECT_EQ(subscription.status, SubscriptionStatus::TRIALING);

    // Transition to ACTIVE (trial ends)
    client_->TransitionSubscriptionState(subscription.id, SubscriptionStatus::ACTIVE);
    auto sub1 = client_->GetSubscription(subscription.id);
    ASSERT_TRUE(sub1.has_value());
    EXPECT_EQ(sub1->status, SubscriptionStatus::ACTIVE);

    // Payment fails → PAST_DUE
    client_->RecordPaymentFailure(subscription.id);
    auto sub2 = client_->GetSubscription(subscription.id);
    ASSERT_TRUE(sub2.has_value());
    EXPECT_EQ(sub2->status, SubscriptionStatus::PAST_DUE);

    // Two more failures → UNPAID
    client_->RecordPaymentFailure(subscription.id);
    client_->RecordPaymentFailure(subscription.id);
    auto sub3 = client_->GetSubscription(subscription.id);
    ASSERT_TRUE(sub3.has_value());
    EXPECT_EQ(sub3->status, SubscriptionStatus::UNPAID);
}

// Test: Helper Methods

TEST_F(MockStripeClientTest, StatusToStringConversion) {
    EXPECT_EQ(MockStripeClient::StatusToString(SubscriptionStatus::TRIALING), "trialing");
    EXPECT_EQ(MockStripeClient::StatusToString(SubscriptionStatus::ACTIVE), "active");
    EXPECT_EQ(MockStripeClient::StatusToString(SubscriptionStatus::PAST_DUE), "past_due");
    EXPECT_EQ(MockStripeClient::StatusToString(SubscriptionStatus::CANCELED), "canceled");
    EXPECT_EQ(MockStripeClient::StatusToString(SubscriptionStatus::UNPAID), "unpaid");
    EXPECT_EQ(MockStripeClient::StatusToString(SubscriptionStatus::PAUSED), "paused");
}

TEST_F(MockStripeClientTest, StringToStatusConversion) {
    EXPECT_EQ(MockStripeClient::StringToStatus("trialing"), SubscriptionStatus::TRIALING);
    EXPECT_EQ(MockStripeClient::StringToStatus("active"), SubscriptionStatus::ACTIVE);
    EXPECT_EQ(MockStripeClient::StringToStatus("past_due"), SubscriptionStatus::PAST_DUE);
    EXPECT_EQ(MockStripeClient::StringToStatus("canceled"), SubscriptionStatus::CANCELED);
    EXPECT_EQ(MockStripeClient::StringToStatus("unpaid"), SubscriptionStatus::UNPAID);
    EXPECT_EQ(MockStripeClient::StringToStatus("paused"), SubscriptionStatus::PAUSED);

    // Unknown status should default to ACTIVE
    EXPECT_EQ(MockStripeClient::StringToStatus("unknown"), SubscriptionStatus::ACTIVE);
}

// Test: Edge Cases

TEST_F(MockStripeClientTest, SchedulePaymentRetryDoesNotCrash) {
    auto customer = client_->CreateCustomer("test@example.com", "tenant_123");
    auto subscription = client_->CreateSubscription(customer.id, "pro");

    // Should not crash with different retry counts
    EXPECT_NO_THROW(client_->SchedulePaymentRetry(subscription.id, 0));
    EXPECT_NO_THROW(client_->SchedulePaymentRetry(subscription.id, 1));
    EXPECT_NO_THROW(client_->SchedulePaymentRetry(subscription.id, 2));
}

TEST_F(MockStripeClientTest, MultipleCustomersAndSubscriptions) {
    // Create multiple customers
    auto customer1 = client_->CreateCustomer("user1@example.com", "tenant_1");
    auto customer2 = client_->CreateCustomer("user2@example.com", "tenant_2");
    auto customer3 = client_->CreateCustomer("user3@example.com", "tenant_3");

    // Create multiple subscriptions
    auto sub1 = client_->CreateSubscription(customer1.id, "free");
    auto sub2 = client_->CreateSubscription(customer2.id, "pro");
    auto sub3 = client_->CreateSubscription(customer3.id, "enterprise");

    // Verify all are stored independently
    auto retrieved_sub1 = client_->GetSubscription(sub1.id);
    auto retrieved_sub2 = client_->GetSubscription(sub2.id);
    auto retrieved_sub3 = client_->GetSubscription(sub3.id);

    ASSERT_TRUE(retrieved_sub1.has_value());
    ASSERT_TRUE(retrieved_sub2.has_value());
    ASSERT_TRUE(retrieved_sub3.has_value());

    EXPECT_EQ(retrieved_sub1->plan_id, "free");
    EXPECT_EQ(retrieved_sub2->plan_id, "pro");
    EXPECT_EQ(retrieved_sub3->plan_id, "enterprise");
}

} // namespace test
} // namespace common
} // namespace saasforge

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
