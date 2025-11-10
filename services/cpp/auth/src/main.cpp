#include <iostream>
#include <memory>
#include <string>
#include <grpcpp/grpcpp.h>
#include "auth/auth_service.h"
#include "common/mtls_credentials.h"

using grpc::Server;
using grpc::ServerBuilder;

void RunServer() {
    const std::string server_address("0.0.0.0:50051");

    // TODO: Load from environment variables
    const std::string ca_cert_path = "/certs/ca.crt";
    const std::string server_cert_path = "/certs/auth-service.crt";
    const std::string server_key_path = "/certs/auth-service.key";

    saasforge::auth::AuthServiceImpl service;

    ServerBuilder builder;

    // Setup mTLS credentials
    auto creds = saasforge::common::MtlsCredentials::CreateServerCredentials(
        ca_cert_path, server_cert_path, server_key_path
    );

    builder.AddListeningPort(server_address, creds);
    builder.RegisterService(&service);

    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "Auth Service listening on " << server_address << std::endl;

    server->Wait();
}

int main(int argc, char** argv) {
    RunServer();
    return 0;
}
