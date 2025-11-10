#pragma once

#include <string>
#include <grpcpp/grpcpp.h>

namespace saasforge {
namespace common {

struct TenantContext {
    std::string tenant_id;
    std::string user_id;
    std::string email;
    std::vector<std::string> roles;
};

class TenantContextInterceptor : public grpc::experimental::Interceptor {
public:
    explicit TenantContextInterceptor(grpc::experimental::ServerRpcInfo* info);

    void Intercept(grpc::experimental::InterceptorBatchMethods* methods) override;

    static TenantContext ExtractFromMetadata(grpc::ServerContext* context);
};

} // namespace common
} // namespace saasforge
