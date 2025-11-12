#include "payment/payment_service.h"
#include "common/tenant_context.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <random>

namespace saasforge {
namespace payment {

PaymentServiceImpl::PaymentServiceImpl(
    std::shared_ptr<common::RedisClient> redis_client,
    std::shared_ptr<common::DbPool> db_pool,
    const std::string& stripe_secret_key,
    const std::string& stripe_webhook_secret
) : redis_client_(redis_client),
    db_pool_(db_pool),
    stripe_secret_key_(stripe_secret_key),
    stripe_webhook_secret_(stripe_webhook_secret) {
    std::cout << "PaymentService initialized" << std::endl;
}

grpc::Status PaymentServiceImpl::CreateSubscription(
    grpc::ServerContext* context,
    const CreateSubscriptionRequest* request,
    SubscriptionResponse* response
) {
    try {
        auto tenant_ctx = common::TenantContextInterceptor::ExtractFromMetadata(context);

        if (tenant_ctx.tenant_id.empty()) {
            return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "Authentication required");
        }

        // Validate tenant_id matches context
        if (request->tenant_id() != tenant_ctx.tenant_id) {
            return grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "Tenant ID mismatch");
        }

        // Check idempotency
        if (!request->idempotency_key().empty()) {
            std::string cache_key = "idempotency:" + tenant_ctx.tenant_id + ":" + request->idempotency_key();
            auto cached = redis_client_->GetSession(cache_key);
            if (cached) {
                // Return cached response (in production, deserialize from JSON)
                return grpc::Status(grpc::StatusCode::ALREADY_EXISTS, "Subscription already created with this idempotency key");
            }
        }

        // Generate mock Stripe subscription ID
        std::string stripe_subscription_id = GenerateMockStripeId("sub");

        // Calculate billing period (30 days from now)
        auto now = std::chrono::system_clock::now();
        auto period_start = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
        auto period_end = period_start + (30 * 24 * 3600); // 30 days

        int64_t trial_end = 0;
        SubscriptionStatus status = SubscriptionStatus::ACTIVE;

        if (request->has_trial_days() && request->trial_days() > 0) {
            trial_end = period_start + (request->trial_days() * 24 * 3600);
            status = SubscriptionStatus::TRIALING;
        }

        // Calculate MRR
        double mrr = CalculateMRR(request->plan_id(), request->quantity());

        // Store in database
        auto conn_guard = db_pool_->AcquireConnection();
        pqxx::work txn(*conn_guard);

        auto result = txn.exec_params(
            "INSERT INTO subscriptions (tenant_id, stripe_subscription_id, plan_id, status, "
            "current_period_start, current_period_end, quantity, mrr, payment_method_id) "
            "VALUES ($1, $2, $3, $4, to_timestamp($5), to_timestamp($6), $7, $8, $9) "
            "RETURNING id",
            tenant_ctx.tenant_id,
            stripe_subscription_id,
            request->plan_id(),
            static_cast<int>(status),
            period_start,
            period_end,
            request->quantity(),
            mrr,
            request->payment_method_id()
        );

        std::string subscription_id = result[0]["id"].as<std::string>();

        // Set response
        response->set_id(subscription_id);
        response->set_tenant_id(tenant_ctx.tenant_id);
        response->set_plan_id(request->plan_id());
        response->set_status(status);
        response->set_current_period_start(period_start);
        response->set_current_period_end(period_end);
        response->set_quantity(request->quantity());
        response->set_mrr(mrr);

        txn.commit();

        // Store idempotency key if provided
        if (!request->idempotency_key().empty()) {
            std::string cache_key = "idempotency:" + tenant_ctx.tenant_id + ":" + request->idempotency_key();
            redis_client_->SetSession(cache_key, subscription_id, 86400); // 24 hours
        }

        return grpc::Status::OK;

    } catch (const std::exception& e) {
        return grpc::Status(grpc::StatusCode::INTERNAL, std::string("Subscription creation failed: ") + e.what());
    }
}

grpc::Status PaymentServiceImpl::UpdateSubscription(
    grpc::ServerContext* context,
    const UpdateSubscriptionRequest* request,
    SubscriptionResponse* response
) {
    try {
        auto tenant_ctx = common::TenantContextInterceptor::ExtractFromMetadata(context);

        if (tenant_ctx.tenant_id.empty()) {
            return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "Authentication required");
        }

        auto conn_guard = db_pool_->AcquireConnection();
        pqxx::work txn(*conn_guard);

        // SECURITY FIX: Use parameterized queries instead of dynamic query building
        // Refactored to use exec_params() with proper parameter binding

        pqxx::result result;

        // Case 1: Update both plan_id and quantity
        if (request->has_plan_id() && request->has_quantity()) {
            // Get current plan_id to calculate MRR
            std::string plan_id = request->plan_id();
            int quantity = request->quantity();
            double new_mrr = CalculateMRR(plan_id, quantity);

            result = txn.exec_params(
                "UPDATE subscriptions SET "
                "plan_id = $1, quantity = $2, mrr = $3, updated_at = NOW() "
                "WHERE id = $4 AND tenant_id = $5 "
                "RETURNING id, tenant_id, plan_id, status, "
                "EXTRACT(EPOCH FROM current_period_start)::bigint as period_start, "
                "EXTRACT(EPOCH FROM current_period_end)::bigint as period_end, "
                "quantity, mrr",
                plan_id,
                quantity,
                new_mrr,
                request->subscription_id(),
                tenant_ctx.tenant_id
            );
        }
        // Case 2: Update only plan_id
        else if (request->has_plan_id()) {
            result = txn.exec_params(
                "UPDATE subscriptions SET "
                "plan_id = $1, updated_at = NOW() "
                "WHERE id = $2 AND tenant_id = $3 "
                "RETURNING id, tenant_id, plan_id, status, "
                "EXTRACT(EPOCH FROM current_period_start)::bigint as period_start, "
                "EXTRACT(EPOCH FROM current_period_end)::bigint as period_end, "
                "quantity, mrr",
                request->plan_id(),
                request->subscription_id(),
                tenant_ctx.tenant_id
            );
        }
        // Case 3: Update only quantity (recalculate MRR with existing plan_id)
        else if (request->has_quantity()) {
            // First, get current plan_id
            auto plan_result = txn.exec_params(
                "SELECT plan_id FROM subscriptions WHERE id = $1 AND tenant_id = $2",
                request->subscription_id(),
                tenant_ctx.tenant_id
            );

            if (plan_result.empty()) {
                return grpc::Status(grpc::StatusCode::NOT_FOUND, "Subscription not found");
            }

            std::string plan_id = plan_result[0]["plan_id"].as<std::string>();
            int quantity = request->quantity();
            double new_mrr = CalculateMRR(plan_id, quantity);

            result = txn.exec_params(
                "UPDATE subscriptions SET "
                "quantity = $1, mrr = $2, updated_at = NOW() "
                "WHERE id = $3 AND tenant_id = $4 "
                "RETURNING id, tenant_id, plan_id, status, "
                "EXTRACT(EPOCH FROM current_period_start)::bigint as period_start, "
                "EXTRACT(EPOCH FROM current_period_end)::bigint as period_end, "
                "quantity, mrr",
                quantity,
                new_mrr,
                request->subscription_id(),
                tenant_ctx.tenant_id
            );
        }
        // Case 4: No fields to update
        else {
            return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "No fields to update");
        }

        if (result.empty()) {
            return grpc::Status(grpc::StatusCode::NOT_FOUND, "Subscription not found");
        }

        auto row = result[0];
        response->set_id(row["id"].as<std::string>());
        response->set_tenant_id(row["tenant_id"].as<std::string>());
        response->set_plan_id(row["plan_id"].as<std::string>());
        response->set_status(static_cast<SubscriptionStatus>(row["status"].as<int>()));
        response->set_current_period_start(row["period_start"].as<long long>());
        response->set_current_period_end(row["period_end"].as<long long>());
        response->set_quantity(row["quantity"].as<int>());
        response->set_mrr(row["mrr"].as<double>());

        txn.commit();

        return grpc::Status::OK;

    } catch (const std::exception& e) {
        return grpc::Status(grpc::StatusCode::INTERNAL, std::string("Subscription update failed: ") + e.what());
    }
}

grpc::Status PaymentServiceImpl::CancelSubscription(
    grpc::ServerContext* context,
    const CancelSubscriptionRequest* request,
    SubscriptionResponse* response
) {
    try {
        auto tenant_ctx = common::TenantContextInterceptor::ExtractFromMetadata(context);

        if (tenant_ctx.tenant_id.empty()) {
            return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "Authentication required");
        }

        auto conn_guard = db_pool_->AcquireConnection();
        pqxx::work txn(*conn_guard);

        std::string cancel_clause;
        if (request->immediate()) {
            // Cancel immediately
            cancel_clause = "status = " + std::to_string(static_cast<int>(SubscriptionStatus::CANCELED)) +
                           ", cancel_at = NOW()";
        } else {
            // Cancel at period end
            cancel_clause = "cancel_at = current_period_end";
        }

        auto result = txn.exec_params(
            "UPDATE subscriptions SET " + cancel_clause + " "
            "WHERE id = $1 AND tenant_id = $2 "
            "RETURNING id, tenant_id, plan_id, status, "
            "EXTRACT(EPOCH FROM current_period_start)::bigint as period_start, "
            "EXTRACT(EPOCH FROM current_period_end)::bigint as period_end, "
            "EXTRACT(EPOCH FROM cancel_at)::bigint as cancel_at, "
            "quantity, mrr",
            request->subscription_id(),
            tenant_ctx.tenant_id
        );

        if (result.empty()) {
            return grpc::Status(grpc::StatusCode::NOT_FOUND, "Subscription not found");
        }

        auto row = result[0];
        response->set_id(row["id"].as<std::string>());
        response->set_tenant_id(row["tenant_id"].as<std::string>());
        response->set_plan_id(row["plan_id"].as<std::string>());
        response->set_status(static_cast<SubscriptionStatus>(row["status"].as<int>()));
        response->set_current_period_start(row["period_start"].as<long long>());
        response->set_current_period_end(row["period_end"].as<long long>());
        if (!row["cancel_at"].is_null()) {
            response->set_cancel_at(row["cancel_at"].as<long long>());
        }
        response->set_quantity(row["quantity"].as<int>());
        response->set_mrr(row["mrr"].as<double>());

        txn.commit();

        return grpc::Status::OK;

    } catch (const std::exception& e) {
        return grpc::Status(grpc::StatusCode::INTERNAL, std::string("Subscription cancellation failed: ") + e.what());
    }
}

grpc::Status PaymentServiceImpl::GetSubscription(
    grpc::ServerContext* context,
    const GetSubscriptionRequest* request,
    SubscriptionResponse* response
) {
    try {
        auto tenant_ctx = common::TenantContextInterceptor::ExtractFromMetadata(context);

        if (tenant_ctx.tenant_id.empty()) {
            return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "Authentication required");
        }

        auto conn_guard = db_pool_->AcquireConnection();
        pqxx::work txn(*conn_guard);

        auto result = txn.exec_params(
            "SELECT id, tenant_id, plan_id, status, "
            "EXTRACT(EPOCH FROM current_period_start)::bigint as period_start, "
            "EXTRACT(EPOCH FROM current_period_end)::bigint as period_end, "
            "EXTRACT(EPOCH FROM cancel_at)::bigint as cancel_at, "
            "quantity, mrr "
            "FROM subscriptions WHERE id = $1 AND tenant_id = $2",
            request->subscription_id(),
            tenant_ctx.tenant_id
        );

        if (result.empty()) {
            return grpc::Status(grpc::StatusCode::NOT_FOUND, "Subscription not found");
        }

        auto row = result[0];
        response->set_id(row["id"].as<std::string>());
        response->set_tenant_id(row["tenant_id"].as<std::string>());
        response->set_plan_id(row["plan_id"].as<std::string>());
        response->set_status(static_cast<SubscriptionStatus>(row["status"].as<int>()));
        response->set_current_period_start(row["period_start"].as<long long>());
        response->set_current_period_end(row["period_end"].as<long long>());
        if (!row["cancel_at"].is_null()) {
            response->set_cancel_at(row["cancel_at"].as<long long>());
        }
        response->set_quantity(row["quantity"].as<int>());
        response->set_mrr(row["mrr"].as<double>());

        txn.commit();

        return grpc::Status::OK;

    } catch (const std::exception& e) {
        return grpc::Status(grpc::StatusCode::INTERNAL, std::string("Get subscription failed: ") + e.what());
    }
}

grpc::Status PaymentServiceImpl::AddPaymentMethod(
    grpc::ServerContext* context,
    const AddPaymentMethodRequest* request,
    PaymentMethodResponse* response
) {
    try {
        auto tenant_ctx = common::TenantContextInterceptor::ExtractFromMetadata(context);

        if (tenant_ctx.tenant_id.empty()) {
            return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "Authentication required");
        }

        // Mock Stripe payment method (in production, call Stripe API)
        std::string mock_type = "card";
        std::string mock_last4 = "4242";
        std::string mock_brand = "visa";
        int mock_exp_month = 12;
        int mock_exp_year = 2025;

        // Store in database
        auto conn_guard = db_pool_->AcquireConnection();
        pqxx::work txn(*conn_guard);

        auto result = txn.exec_params(
            "INSERT INTO payment_methods (tenant_id, stripe_payment_method_id, type, last4, brand, exp_month, exp_year) "
            "VALUES ($1, $2, $3, $4, $5, $6, $7) "
            "RETURNING id",
            tenant_ctx.tenant_id,
            request->stripe_payment_method_id(),
            mock_type,
            mock_last4,
            mock_brand,
            mock_exp_month,
            mock_exp_year
        );

        response->set_id(result[0]["id"].as<std::string>());
        response->set_type(mock_type);
        response->set_last4(mock_last4);
        response->set_brand(mock_brand);
        response->set_exp_month(mock_exp_month);
        response->set_exp_year(mock_exp_year);

        txn.commit();

        return grpc::Status::OK;

    } catch (const std::exception& e) {
        return grpc::Status(grpc::StatusCode::INTERNAL, std::string("Add payment method failed: ") + e.what());
    }
}

grpc::Status PaymentServiceImpl::RemovePaymentMethod(
    grpc::ServerContext* context,
    const RemovePaymentMethodRequest* request,
    RemovePaymentMethodResponse* response
) {
    try {
        auto tenant_ctx = common::TenantContextInterceptor::ExtractFromMetadata(context);

        if (tenant_ctx.tenant_id.empty()) {
            return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "Authentication required");
        }

        auto conn_guard = db_pool_->AcquireConnection();
        pqxx::work txn(*conn_guard);

        // Soft delete
        auto result = txn.exec_params(
            "UPDATE payment_methods SET deleted_at = NOW() "
            "WHERE id = $1 AND tenant_id = $2 AND deleted_at IS NULL",
            request->payment_method_id(),
            tenant_ctx.tenant_id
        );

        if (result.affected_rows() == 0) {
            return grpc::Status(grpc::StatusCode::NOT_FOUND, "Payment method not found");
        }

        response->set_success(true);
        txn.commit();

        return grpc::Status::OK;

    } catch (const std::exception& e) {
        return grpc::Status(grpc::StatusCode::INTERNAL, std::string("Remove payment method failed: ") + e.what());
    }
}

grpc::Status PaymentServiceImpl::GetInvoice(
    grpc::ServerContext* context,
    const GetInvoiceRequest* request,
    InvoiceResponse* response
) {
    try {
        auto tenant_ctx = common::TenantContextInterceptor::ExtractFromMetadata(context);

        if (tenant_ctx.tenant_id.empty()) {
            return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "Authentication required");
        }

        auto conn_guard = db_pool_->AcquireConnection();
        pqxx::work txn(*conn_guard);

        auto result = txn.exec_params(
            "SELECT id, tenant_id, subscription_id, amount_due, amount_paid, status, "
            "EXTRACT(EPOCH FROM due_date)::bigint as due_date, "
            "EXTRACT(EPOCH FROM paid_at)::bigint as paid_at, "
            "pdf_url "
            "FROM invoices WHERE id = $1 AND tenant_id = $2",
            request->invoice_id(),
            tenant_ctx.tenant_id
        );

        if (result.empty()) {
            return grpc::Status(grpc::StatusCode::NOT_FOUND, "Invoice not found");
        }

        auto row = result[0];
        response->set_id(row["id"].as<std::string>());
        response->set_tenant_id(row["tenant_id"].as<std::string>());
        response->set_subscription_id(row["subscription_id"].as<std::string>());
        response->set_amount_due(row["amount_due"].as<double>());
        response->set_amount_paid(row["amount_paid"].as<double>());
        response->set_status(row["status"].as<std::string>());
        response->set_due_date(row["due_date"].as<long long>());

        if (!row["paid_at"].is_null()) {
            response->set_paid_at(row["paid_at"].as<long long>());
        }
        if (!row["pdf_url"].is_null()) {
            response->set_pdf_url(row["pdf_url"].as<std::string>());
        }

        txn.commit();

        return grpc::Status::OK;

    } catch (const std::exception& e) {
        return grpc::Status(grpc::StatusCode::INTERNAL, std::string("Get invoice failed: ") + e.what());
    }
}

grpc::Status PaymentServiceImpl::RecordUsage(
    grpc::ServerContext* context,
    const RecordUsageRequest* request,
    RecordUsageResponse* response
) {
    try {
        auto tenant_ctx = common::TenantContextInterceptor::ExtractFromMetadata(context);

        if (tenant_ctx.tenant_id.empty()) {
            return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "Authentication required");
        }

        // Verify subscription belongs to tenant
        auto conn_guard = db_pool_->AcquireConnection();
        pqxx::work txn(*conn_guard);

        auto sub_check = txn.exec_params(
            "SELECT id FROM subscriptions WHERE id = $1 AND tenant_id = $2",
            request->subscription_id(),
            tenant_ctx.tenant_id
        );

        if (sub_check.empty()) {
            return grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "Subscription not found or access denied");
        }

        // Record usage
        auto result = txn.exec_params(
            "INSERT INTO usage_records (tenant_id, subscription_id, metric_name, quantity, timestamp) "
            "VALUES ($1, $2, $3, $4, to_timestamp($5)) "
            "RETURNING id",
            tenant_ctx.tenant_id,
            request->subscription_id(),
            request->metric_name(),
            request->quantity(),
            request->timestamp()
        );

        response->set_success(true);
        response->set_usage_record_id(result[0]["id"].as<std::string>());

        txn.commit();

        return grpc::Status::OK;

    } catch (const std::exception& e) {
        return grpc::Status(grpc::StatusCode::INTERNAL, std::string("Record usage failed: ") + e.what());
    }
}

// Helper methods

std::string PaymentServiceImpl::GenerateMockStripeId(const std::string& prefix) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);

    std::stringstream ss;
    ss << prefix << "_";
    for (int i = 0; i < 24; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << dis(gen);
    }

    return ss.str();
}

double PaymentServiceImpl::CalculateMRR(const std::string& plan_id, int32_t quantity) {
    // Mock MRR calculation (in production, lookup plan pricing)
    double base_price = 0.0;

    if (plan_id == "starter") {
        base_price = 29.0;
    } else if (plan_id == "professional") {
        base_price = 99.0;
    } else if (plan_id == "enterprise") {
        base_price = 299.0;
    } else {
        base_price = 49.0; // default
    }

    return base_price * quantity;
}

} // namespace payment
} // namespace saasforge
