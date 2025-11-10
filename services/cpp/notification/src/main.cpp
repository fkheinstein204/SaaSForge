#include <iostream>
#include <memory>
#include <string>
#include <grpcpp/grpcpp.h>
#include "notification/notification_service.h"
#include "common/mtls_credentials.h"

using grpc::Server;
using grpc::ServerBuilder;

void RunServer() {
    const std::string server_address("0.0.0.0:50054");

    const std::string ca_cert_path = "/certs/ca.crt";
    const std::string server_cert_path = "/certs/notification-service.crt";
    const std::string server_key_path = "/certs/notification-service.key";

    saasforge::notification::NotificationServiceImpl service;

    ServerBuilder builder;

    auto creds = saasforge::common::MtlsCredentials::CreateServerCredentials(
        ca_cert_path, server_cert_path, server_key_path
    );

    builder.AddListeningPort(server_address, creds);
    builder.RegisterService(&service);

    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "Notification Service listening on " << server_address << std::endl;

    server->Wait();
}

int main(int argc, char** argv) {
    RunServer();
    return 0;
}
