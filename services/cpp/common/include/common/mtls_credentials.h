#pragma once

#include <grpcpp/grpcpp.h>
#include <grpcpp/security/server_credentials.h>
#include <memory>
#include <string>

namespace saasforge {
namespace common {

class MtlsCredentials {
public:
    // Create server credentials with mTLS
    static std::shared_ptr<grpc::ServerCredentials> CreateServerCredentials(
        const std::string& ca_cert_path,
        const std::string& server_cert_path,
        const std::string& server_key_path
    );

    // Create client credentials with mTLS
    static std::shared_ptr<grpc::ChannelCredentials> CreateClientCredentials(
        const std::string& ca_cert_path,
        const std::string& client_cert_path,
        const std::string& client_key_path
    );

private:
    static std::string ReadFile(const std::string& path);
};

} // namespace common
} // namespace saasforge
