#include <iostream>
#include <memory>
#include <string>
#include <fstream>
#include <grpcpp/grpcpp.h>
#include "auth/auth_service.h"
#include "common/mtls_credentials.h"
#include "common/redis_client.h"
#include "common/db_pool.h"

using grpc::Server;
using grpc::ServerBuilder;

std::string ReadFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + path);
    }
    return std::string((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
}

void RunServer() {
    const std::string server_address("0.0.0.0:50051");

    // Load configuration from environment variables with defaults
    const char* redis_url_env = std::getenv("REDIS_URL");
    const char* db_url_env = std::getenv("DATABASE_URL");
    const char* jwt_public_key_path_env = std::getenv("JWT_PUBLIC_KEY_PATH");
    const char* jwt_private_key_path_env = std::getenv("JWT_PRIVATE_KEY_PATH");

    std::string redis_url = redis_url_env ? redis_url_env : "tcp://127.0.0.1:6379";
    std::string db_url = db_url_env ? db_url_env : "postgresql://localhost/saasforge";
    std::string jwt_public_key_path = jwt_public_key_path_env ? jwt_public_key_path_env : "./certs/jwt-public.pem";
    std::string jwt_private_key_path = jwt_private_key_path_env ? jwt_private_key_path_env : "./certs/jwt-private.pem";

    // Initialize Redis client
    auto redis_client = std::make_shared<saasforge::common::RedisClient>(redis_url);

    // Initialize database pool
    auto db_pool = std::make_shared<saasforge::common::DbPool>(db_url, 10);

    // Load JWT keys
    std::string jwt_public_key;
    std::string jwt_private_key;

    try {
        jwt_public_key = ReadFile(jwt_public_key_path);
        jwt_private_key = ReadFile(jwt_private_key_path);
    } catch (const std::exception& e) {
        std::cerr << "Warning: Failed to load JWT keys: " << e.what() << std::endl;
        std::cerr << "Using placeholder keys (NOT FOR PRODUCTION)" << std::endl;
        // In development, services might still work without JWT keys
        jwt_public_key = "";
        jwt_private_key = "";
    }

    // Create auth service
    saasforge::auth::AuthServiceImpl service(
        redis_client,
        db_pool,
        jwt_public_key,
        jwt_private_key
    );

    ServerBuilder builder;

    // Setup mTLS credentials
    const char* ca_cert_path_env = std::getenv("CA_CERT_PATH");
    const char* server_cert_path_env = std::getenv("SERVER_CERT_PATH");
    const char* server_key_path_env = std::getenv("SERVER_KEY_PATH");

    std::string ca_cert_path = ca_cert_path_env ? ca_cert_path_env : "./certs/ca.crt";
    std::string server_cert_path = server_cert_path_env ? server_cert_path_env : "./certs/auth-service.crt";
    std::string server_key_path = server_key_path_env ? server_key_path_env : "./certs/auth-service.key";

    try {
        auto creds = saasforge::common::MtlsCredentials::CreateServerCredentials(
            ca_cert_path, server_cert_path, server_key_path
        );
        builder.AddListeningPort(server_address, creds);
        std::cout << "mTLS enabled with certificates from: " << server_cert_path << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Warning: mTLS setup failed: " << e.what() << std::endl;
        std::cerr << "Falling back to insecure credentials (NOT FOR PRODUCTION)" << std::endl;
        builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    }

    builder.RegisterService(&service);

    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "Auth Service listening on " << server_address << std::endl;

    server->Wait();
}

int main(int argc, char** argv) {
    try {
        RunServer();
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
