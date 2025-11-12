#pragma once

#include <grpcpp/grpcpp.h>
#include <memory>
#include "upload.grpc.pb.h"
#include "common/redis_client.h"
#include "common/db_pool.h"

namespace saasforge {
namespace upload {

class UploadServiceImpl final : public ::saasforge::upload::UploadService::Service {
public:
    UploadServiceImpl(
        std::shared_ptr<common::RedisClient> redis_client,
        std::shared_ptr<common::DbPool> db_pool,
        const std::string& s3_bucket,
        const std::string& s3_region
    );

    grpc::Status GeneratePresignedUrl(
        grpc::ServerContext* context,
        const PresignedUrlRequest* request,
        PresignedUrlResponse* response
    ) override;

    grpc::Status CompleteUpload(
        grpc::ServerContext* context,
        const CompleteUploadRequest* request,
        CompleteUploadResponse* response
    ) override;

    grpc::Status TransformObject(
        grpc::ServerContext* context,
        const TransformRequest* request,
        TransformResponse* response
    ) override;

    grpc::Status DeleteObject(
        grpc::ServerContext* context,
        const DeleteObjectRequest* request,
        DeleteObjectResponse* response
    ) override;

    grpc::Status GetQuota(
        grpc::ServerContext* context,
        const GetQuotaRequest* request,
        GetQuotaResponse* response
    ) override;

private:
    std::shared_ptr<common::RedisClient> redis_client_;
    std::shared_ptr<common::DbPool> db_pool_;
    std::string s3_bucket_;
    std::string s3_region_;

    // Helper methods
    std::string GenerateMockPresignedUrl(const std::string& key, int64_t expires_in);
    bool CheckQuota(const std::string& tenant_id, int64_t file_size);
    void UpdateQuota(const std::string& tenant_id, int64_t file_size);
};

} // namespace upload
} // namespace saasforge
