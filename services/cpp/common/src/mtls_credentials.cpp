#include "common/mtls_credentials.h"
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace saasforge {
namespace common {

std::string MtlsCredentials::ReadFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + path);
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::shared_ptr<grpc::ServerCredentials> MtlsCredentials::CreateServerCredentials(
    const std::string& ca_cert_path,
    const std::string& server_cert_path,
    const std::string& server_key_path
) {
    std::string ca_cert = ReadFile(ca_cert_path);
    std::string server_cert = ReadFile(server_cert_path);
    std::string server_key = ReadFile(server_key_path);

    grpc::SslServerCredentialsOptions ssl_opts;
    ssl_opts.pem_root_certs = ca_cert;

    grpc::SslServerCredentialsOptions::PemKeyCertPair key_cert_pair;
    key_cert_pair.private_key = server_key;
    key_cert_pair.cert_chain = server_cert;
    ssl_opts.pem_key_cert_pairs.push_back(key_cert_pair);

    // Require and verify client certificates
    ssl_opts.client_certificate_request =
        GRPC_SSL_REQUEST_AND_REQUIRE_CLIENT_CERTIFICATE_AND_VERIFY;

    return grpc::SslServerCredentials(ssl_opts);
}

std::shared_ptr<grpc::ChannelCredentials> MtlsCredentials::CreateClientCredentials(
    const std::string& ca_cert_path,
    const std::string& client_cert_path,
    const std::string& client_key_path
) {
    std::string ca_cert = ReadFile(ca_cert_path);
    std::string client_cert = ReadFile(client_cert_path);
    std::string client_key = ReadFile(client_key_path);

    grpc::SslCredentialsOptions ssl_opts;
    ssl_opts.pem_root_certs = ca_cert;
    ssl_opts.pem_cert_chain = client_cert;
    ssl_opts.pem_private_key = client_key;

    return grpc::SslCredentials(ssl_opts);
}

} // namespace common
} // namespace saasforge
