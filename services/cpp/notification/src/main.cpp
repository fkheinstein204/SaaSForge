#include <iostream>
#include <memory>
#include <string>
#include <grpcpp/grpcpp.h>
#include "notification/notification_service.h"
#include "common/mtls_credentials.h"
#include "common/redis_client.h"
#include "common/db_pool.h"

using grpc::Server;
using grpc::ServerBuilder;

void RunServer() {
    const std::string server_address("0.0.0.0:50054");

    // Load configuration
    const char* redis_url_env = std::getenv("REDIS_URL");
    const char* db_url_env = std::getenv("DATABASE_URL");
    const char* sendgrid_api_key_env = std::getenv("SENDGRID_API_KEY");
    const char* twilio_account_sid_env = std::getenv("TWILIO_ACCOUNT_SID");
    const char* twilio_auth_token_env = std::getenv("TWILIO_AUTH_TOKEN");
    const char* fcm_server_key_env = std::getenv("FCM_SERVER_KEY");

    std::string redis_url = redis_url_env ? redis_url_env : "tcp://127.0.0.1:6379";
    std::string db_url = db_url_env ? db_url_env : "postgresql://localhost/saasforge";
    std::string sendgrid_api_key = sendgrid_api_key_env ? sendgrid_api_key_env : "sg_mock";
    std::string twilio_account_sid = twilio_account_sid_env ? twilio_account_sid_env : "AC_mock";
    std::string twilio_auth_token = twilio_auth_token_env ? twilio_auth_token_env : "auth_mock";
    std::string fcm_server_key = fcm_server_key_env ? fcm_server_key_env : "fcm_mock";

    // Initialize Redis and DB
    auto redis_client = std::make_shared<saasforge::common::RedisClient>(redis_url);
    auto db_pool = std::make_shared<saasforge::common::DbPool>(db_url, 10);

    saasforge::notification::NotificationServiceImpl service(
        redis_client, db_pool, sendgrid_api_key, twilio_account_sid, twilio_auth_token, fcm_server_key
    );

    ServerBuilder builder;

    // Setup mTLS
    const char* ca_cert_path_env = std::getenv("CA_CERT_PATH");
    const char* server_cert_path_env = std::getenv("SERVER_CERT_PATH");
    const char* server_key_path_env = std::getenv("SERVER_KEY_PATH");

    std::string ca_cert_path = ca_cert_path_env ? ca_cert_path_env : "./certs/ca.crt";
    std::string server_cert_path = server_cert_path_env ? server_cert_path_env : "./certs/notification-service.crt";
    std::string server_key_path = server_key_path_env ? server_key_path_env : "./certs/notification-service.key";

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
    std::cout << "Notification Service listening on " << server_address << std::endl;

    server->Wait();
}

int main(int argc, char** argv) {
    RunServer();
    return 0;
}
