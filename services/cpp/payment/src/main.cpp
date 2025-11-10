#include <iostream>
#include <memory>
#include <string>
#include <grpcpp/grpcpp.h>
#include "payment/payment_service.h"
#include "common/mtls_credentials.h"

using grpc::Server;
using grpc::ServerBuilder;

void RunServer() {
    const std::string server_address("0.0.0.0:50053");

    const std::string ca_cert_path = "/certs/ca.crt";
    const std::string server_cert_path = "/certs/payment-service.crt";
    const std::string server_key_path = "/certs/payment-service.key";

    saasforge::payment::PaymentServiceImpl service;

    ServerBuilder builder;

    auto creds = saasforge::common::MtlsCredentials::CreateServerCredentials(
        ca_cert_path, server_cert_path, server_key_path
    );

    builder.AddListeningPort(server_address, creds);
    builder.RegisterService(&service);

    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "Payment Service listening on " << server_address << std::endl;

    server->Wait();
}

int main(int argc, char** argv) {
    RunServer();
    return 0;
}
