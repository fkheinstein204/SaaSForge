#include <iostream>
#include <memory>
#include <string>
#include <grpcpp/grpcpp.h>
#include "upload/upload_service.h"
#include "common/mtls_credentials.h"
#include "common/redis_client.h"
#include "common/db_pool.h"

using grpc::Server;
using grpc::ServerBuilder;

void RunServer() {
    const std::string server_address("0.0.0.0:50052");

    // Load configuration
    const char* redis_url_env = std::getenv("REDIS_URL");
    const char* db_url_env = std::getenv("DATABASE_URL");
    const char* s3_bucket_env = std::getenv("S3_BUCKET");
    const char* s3_region_env = std::getenv("S3_REGION");

    std::string redis_url = redis_url_env ? redis_url_env : "tcp://127.0.0.1:6379";
    std::string db_url = db_url_env ? db_url_env : "postgresql://localhost/saasforge";
    std::string s3_bucket = s3_bucket_env ? s3_bucket_env : "saasforge-uploads";
    std::string s3_region = s3_region_env ? s3_region_env : "us-east-1";

    // Initialize Redis and DB
    auto redis_client = std::make_shared<saasforge::common::RedisClient>(redis_url);
    auto db_pool = std::make_shared<saasforge::common::DbPool>(db_url, 10);

    saasforge::upload::UploadServiceImpl service(redis_client, db_pool, s3_bucket, s3_region);

    ServerBuilder builder;

    // Setup mTLS
    const char* ca_cert_path_env = std::getenv("CA_CERT_PATH");
    const char* server_cert_path_env = std::getenv("SERVER_CERT_PATH");
    const char* server_key_path_env = std::getenv("SERVER_KEY_PATH");

    std::string ca_cert_path = ca_cert_path_env ? ca_cert_path_env : "./certs/ca.crt";
    std::string server_cert_path = server_cert_path_env ? server_cert_path_env : "./certs/upload-service.crt";
    std::string server_key_path = server_key_path_env ? server_key_path_env : "./certs/upload-service.key";

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
    std::cout << "Upload Service listening on " << server_address << std::endl;

    server->Wait();
}

int main(int argc, char** argv) {
    RunServer();
    return 0;
}
