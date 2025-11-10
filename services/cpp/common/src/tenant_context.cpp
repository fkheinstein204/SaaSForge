#include "common/tenant_context.h"

namespace saasforge {
namespace common {

TenantContextInterceptor::TenantContextInterceptor(grpc::experimental::ServerRpcInfo* info) {
    // Initialize interceptor
}

void TenantContextInterceptor::Intercept(grpc::experimental::InterceptorBatchMethods* methods) {
    // Extract tenant context from metadata
    // TODO: Implement
}

TenantContext TenantContextInterceptor::ExtractFromMetadata(grpc::ServerContext* context) {
    TenantContext tenant_ctx;

    auto metadata = context->client_metadata();

    auto tenant_id = metadata.find("x-tenant-id");
    if (tenant_id != metadata.end()) {
        tenant_ctx.tenant_id = std::string(tenant_id->second.data(), tenant_id->second.size());
    }

    auto user_id = metadata.find("x-user-id");
    if (user_id != metadata.end()) {
        tenant_ctx.user_id = std::string(user_id->second.data(), user_id->second.size());
    }

    auto email = metadata.find("x-user-email");
    if (email != metadata.end()) {
        tenant_ctx.email = std::string(email->second.data(), email->second.size());
    }

    return tenant_ctx;
}

} // namespace common
} // namespace saasforge
