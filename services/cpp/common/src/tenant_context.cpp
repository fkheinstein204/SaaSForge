#include "common/tenant_context.h"
#include "common/jwt_validator.h"
#include <iostream>
#include <string_view>
#include <algorithm>

namespace saasforge {
namespace common {

TenantContextInterceptor::TenantContextInterceptor(grpc::experimental::ServerRpcInfo* info) {
    // Initialize interceptor with RPC info
}

void TenantContextInterceptor::Intercept(grpc::experimental::InterceptorBatchMethods* methods) {
    // This interceptor runs on every RPC call
    // It validates tenant context from metadata

    if (methods->QueryInterceptionHookPoint(
            grpc::experimental::InterceptionHookPoints::PRE_SEND_INITIAL_METADATA)) {
        // Add any response metadata if needed
    }

    if (methods->QueryInterceptionHookPoint(
            grpc::experimental::InterceptionHookPoints::POST_RECV_INITIAL_METADATA)) {
        // Validate tenant context from incoming metadata
        // Note: Actual validation is done in ExtractFromMetadata by each service
    }

    // Continue with the next interceptor in the chain
    methods->Proceed();
}

std::string TenantContextInterceptor::ExtractJwtFromMetadata(grpc::ServerContext* context) {
    auto metadata = context->client_metadata();

    // Try to extract JWT from Authorization header
    auto auth_header = metadata.find("authorization");
    if (auth_header != metadata.end()) {
        std::string auth_value(auth_header->second.data(), auth_header->second.size());

        // Remove "Bearer " prefix if present
        const std::string bearer_prefix = "Bearer ";
        if (auth_value.size() > bearer_prefix.size() &&
            auth_value.substr(0, bearer_prefix.size()) == bearer_prefix) {
            return auth_value.substr(bearer_prefix.size());
        }

        return auth_value;
    }

    return "";
}

TenantContext TenantContextInterceptor::ExtractFromMetadata(
    grpc::ServerContext* context,
    std::shared_ptr<JwtValidator> jwt_validator
) {
    TenantContext tenant_ctx;
    tenant_ctx.validated = false;

    auto metadata = context->client_metadata();

    // Extract requested tenant_id from metadata
    auto tenant_id_header = metadata.find("x-tenant-id");
    std::string requested_tenant_id;
    if (tenant_id_header != metadata.end()) {
        requested_tenant_id = std::string(tenant_id_header->second.data(),
                                          tenant_id_header->second.size());
    }

    // If JWT validator provided, validate token and check tenant_id
    if (jwt_validator) {
        std::string jwt_token = ExtractJwtFromMetadata(context);

        if (!jwt_token.empty()) {
            auto claims = jwt_validator->Validate(jwt_token);

            if (claims) {
                // CRITICAL SECURITY CHECK: Verify requested tenant_id matches JWT claim
                if (!requested_tenant_id.empty() &&
                    claims->tenant_id != requested_tenant_id) {
                    // SECURITY VIOLATION: Client requested different tenant than they're authenticated for
                    std::cerr << "SECURITY: Tenant ID mismatch - JWT: " << claims->tenant_id
                              << ", Requested: " << requested_tenant_id << std::endl;

                    // Return empty context to indicate validation failure
                    tenant_ctx.validated = false;
                    return tenant_ctx;
                }

                // Validation passed - populate context from JWT claims
                tenant_ctx.tenant_id = claims->tenant_id;
                tenant_ctx.user_id = claims->user_id;
                tenant_ctx.email = claims->email;
                tenant_ctx.roles = claims->roles;
                tenant_ctx.validated = true;

                return tenant_ctx;
            } else {
                // JWT validation failed
                std::cerr << "SECURITY: JWT validation failed" << std::endl;
                tenant_ctx.validated = false;
                return tenant_ctx;
            }
        }
    }

    // If no JWT validator provided, fall back to unsafe extraction
    // This should only be used for services that don't require authentication
    std::cerr << "WARNING: Extracting tenant context without JWT validation" << std::endl;
    return ExtractFromMetadataUnsafe(context);
}

TenantContext TenantContextInterceptor::ExtractFromMetadataUnsafe(grpc::ServerContext* context) {
    TenantContext tenant_ctx;
    tenant_ctx.validated = false;  // Mark as unvalidated

    auto metadata = context->client_metadata();

    // Extract tenant ID
    auto tenant_id = metadata.find("x-tenant-id");
    if (tenant_id != metadata.end()) {
        tenant_ctx.tenant_id = std::string(tenant_id->second.data(), tenant_id->second.size());
    }

    // Extract user ID
    auto user_id = metadata.find("x-user-id");
    if (user_id != metadata.end()) {
        tenant_ctx.user_id = std::string(user_id->second.data(), user_id->second.size());
    }

    // Extract email
    auto email = metadata.find("x-user-email");
    if (email != metadata.end()) {
        tenant_ctx.email = std::string(email->second.data(), email->second.size());
    }

    // Extract roles (comma-separated list)
    auto roles = metadata.find("x-user-roles");
    if (roles != metadata.end()) {
        std::string roles_str(roles->second.data(), roles->second.size());

        // Parse comma-separated roles
        std::string_view roles_view(roles_str);
        size_t start = 0;
        size_t end = roles_view.find(',');

        while (end != std::string_view::npos) {
            auto role = roles_view.substr(start, end - start);
            // Trim whitespace
            while (!role.empty() && std::isspace(role.front())) role.remove_prefix(1);
            while (!role.empty() && std::isspace(role.back())) role.remove_suffix(1);

            if (!role.empty()) {
                tenant_ctx.roles.push_back(std::string(role));
            }

            start = end + 1;
            end = roles_view.find(',', start);
        }

        // Add last role
        if (start < roles_view.length()) {
            auto role = roles_view.substr(start);
            while (!role.empty() && std::isspace(role.front())) role.remove_prefix(1);
            while (!role.empty() && std::isspace(role.back())) role.remove_suffix(1);

            if (!role.empty()) {
                tenant_ctx.roles.push_back(std::string(role));
            }
        }
    }

    return tenant_ctx;
}

} // namespace common
} // namespace saasforge
