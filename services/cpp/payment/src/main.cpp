#include <iostream>
#include <memory>
#include <string>
#include <grpcpp/grpcpp.h>
#include "payment/payment_service.h"
#include "common/mtls_credentials.h"
#include "common/redis_client.h"
#include "common/db_pool.h"

using grpc::Server;
using grpc::ServerBuilder;

void RunServer() {
    const std::string server_address("0.0.0.0:50053");

    // Load configuration
    const char* redis_url_env = std::getenv("REDIS_URL");
    const char* db_url_env = std::getenv("DATABASE_URL");
    const char* stripe_secret_key_env = std::getenv("STRIPE_SECRET_KEY");
    const char* stripe_webhook_secret_env = std::getenv("STRIPE_WEBHOOK_SECRET");

    std::string redis_url = redis_url_env ? redis_url_env : "tcp://127.0.0.1:6379";
    std::string db_url = db_url_env ? db_url_env : "postgresql://localhost/saasforge";
    std::string stripe_secret_key = stripe_secret_key_env ? stripe_secret_key_env : "sk_test_mock";
    std::string stripe_webhook_secret = stripe_webhook_secret_env ? stripe_webhook_secret_env : "whsec_mock";

    // Initialize Redis and DB
    auto redis_client = std::make_shared<saasforge::common::RedisClient>(redis_url);
    auto db_pool = std::make_shared<saasforge::common::DbPool>(db_url, 10);

    saasforge::payment::PaymentServiceImpl service(redis_client, db_pool, stripe_secret_key, stripe_webhook_secret);

    ServerBuilder builder;

    // Setup mTLS
    const char* ca_cert_path_env = std::getenv("CA_CERT_PATH");
    const char* server_cert_path_env = std::getenv("SERVER_CERT_PATH");
    const char* server_key_path_env = std::getenv("SERVER_KEY_PATH");

    std::string ca_cert_path = ca_cert_path_env ? ca_cert_path_env : "./certs/ca.crt";
    std::string server_cert_path = server_cert_path_env ? server_cert_path_env : "./certs/payment-service.crt";
    std::string server_key_path = server_key_path_env ? server_key_path_env : "./certs/payment-service.key";

    try {
        auto creds = saasforge::common::MtlsCredentials::CreateServerCredentials(
            ca_cert_path, server_cert_path, server_key_path
        );
        builder.AddListeningPort(server_address, creds);
        std::cout << "mTLS enabled" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "mTLS setup failed: " << e.what() << std::endl;
        std::cerr << "Falling back to insecure credentials" << std::endl;
        builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    }

    builder.RegisterService(&service);

    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "Payment Service listening on " << server_address << std::endl;

    server->Wait();
}

int main(int argc, char** argv) {
    RunServer();
    return 0;
}
