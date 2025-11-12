#pragma once

#include <string>
#include <grpcpp/grpcpp.h>
#include <memory>

namespace saasforge {
namespace common {

// Forward declaration
class JwtValidator;

struct TenantContext {
    std::string tenant_id;
    std::string user_id;
    std::string email;
    std::vector<std::string> roles;
    bool validated;  // Indicates if tenant_id was validated against JWT
};

class TenantContextInterceptor : public grpc::experimental::Interceptor {
public:
    explicit TenantContextInterceptor(grpc::experimental::ServerRpcInfo* info);

    void Intercept(grpc::experimental::InterceptorBatchMethods* methods) override;

    // Extract and validate tenant context from JWT token in metadata
    static TenantContext ExtractFromMetadata(
        grpc::ServerContext* context,
        std::shared_ptr<JwtValidator> jwt_validator = nullptr
    );

    // Extract without validation (for backward compatibility, use with caution)
    static TenantContext ExtractFromMetadataUnsafe(grpc::ServerContext* context);

private:
    static std::string ExtractJwtFromMetadata(grpc::ServerContext* context);
};

} // namespace common
} // namespace saasforge
