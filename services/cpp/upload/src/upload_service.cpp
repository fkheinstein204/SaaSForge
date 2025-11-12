#include "upload/upload_service.h"
#include "common/tenant_context.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>

namespace saasforge {
namespace upload {

UploadServiceImpl::UploadServiceImpl(
    std::shared_ptr<common::RedisClient> redis_client,
    std::shared_ptr<common::DbPool> db_pool,
    const std::string& s3_bucket,
    const std::string& s3_region
) : redis_client_(redis_client),
    db_pool_(db_pool),
    s3_bucket_(s3_bucket),
    s3_region_(s3_region) {
    std::cout << "UploadService initialized with bucket: " << s3_bucket_ << std::endl;
}

grpc::Status UploadServiceImpl::GeneratePresignedUrl(
    grpc::ServerContext* context,
    const PresignedUrlRequest* request,
    PresignedUrlResponse* response
) {
    try {
        auto tenant_ctx = common::TenantContextInterceptor::ExtractFromMetadata(context);

        if (tenant_ctx.tenant_id.empty()) {
            return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "Authentication required");
        }

        // Validate inputs
        if (request->filename().empty()) {
            return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Filename required");
        }

        if (request->content_length() <= 0) {
            return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Content length must be positive");
        }

        // Check quota
        if (!CheckQuota(tenant_ctx.tenant_id, request->content_length())) {
            return grpc::Status(grpc::StatusCode::RESOURCE_EXHAUSTED, "Quota exceeded");
        }

        // Generate object key
        std::stringstream key_ss;
        key_ss << tenant_ctx.tenant_id << "/"
               << tenant_ctx.user_id << "/"
               << std::time(nullptr) << "_"
               << request->filename();
        std::string object_key = key_ss.str();

        // Generate mock presigned URL (in production, use AWS SDK)
        std::string presigned_url = GenerateMockPresignedUrl(object_key, 600); // 10 minutes

        // Store upload metadata in database
        auto conn_guard = db_pool_->AcquireConnection();
        pqxx::work txn(*conn_guard);

        auto result = txn.exec_params(
            "INSERT INTO upload_objects (tenant_id, user_id, object_key, filename, size, content_type, status) "
            "VALUES ($1, $2, $3, $4, $5, $6, 'pending') "
            "RETURNING id",
            tenant_ctx.tenant_id,
            tenant_ctx.user_id,
            object_key,
            request->filename(),
            request->content_length(),
            request->content_type()
        );

        response->set_url(presigned_url);
        response->set_upload_id(result[0]["id"].as<std::string>());
        response->set_expires_in(600);

        txn.commit();

        return grpc::Status::OK;

    } catch (const std::exception& e) {
        return grpc::Status(grpc::StatusCode::INTERNAL, std::string("Presigned URL generation failed: ") + e.what());
    }
}

grpc::Status UploadServiceImpl::CompleteUpload(
    grpc::ServerContext* context,
    const CompleteUploadRequest* request,
    CompleteUploadResponse* response
) {
    try {
        auto tenant_ctx = common::TenantContextInterceptor::ExtractFromMetadata(context);

        if (tenant_ctx.tenant_id.empty()) {
            return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "Authentication required");
        }

        // Update upload object status
        auto conn_guard = db_pool_->AcquireConnection();
        pqxx::work txn(*conn_guard);

        // TODO: Verify ETag matches uploaded file

        auto result = txn.exec_params(
            "UPDATE upload_objects SET status = 'completed', checksum = $1, completed_at = NOW() "
            "WHERE id = $2 AND tenant_id = $3 "
            "RETURNING id, size, object_key, checksum",
            request->etag(),
            request->upload_id(),
            tenant_ctx.tenant_id
        );

        if (result.empty()) {
            return grpc::Status(grpc::StatusCode::NOT_FOUND, "Upload not found");
        }

        // Update quota usage
        int64_t file_size = result[0]["size"].as<long long>();
        UpdateQuota(tenant_ctx.tenant_id, file_size);

        response->set_object_id(result[0]["id"].as<std::string>());
        response->set_size(file_size);
        response->set_checksum_sha256(result[0]["checksum"].as<std::string>());
        txn.commit();

        return grpc::Status::OK;

    } catch (const std::exception& e) {
        return grpc::Status(grpc::StatusCode::INTERNAL, std::string("Upload completion failed: ") + e.what());
    }
}

grpc::Status UploadServiceImpl::TransformObject(
    grpc::ServerContext* context,
    const TransformRequest* request,
    TransformResponse* response
) {
    // Mock transformation service
    return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "TransformObject not yet implemented");
}

grpc::Status UploadServiceImpl::DeleteObject(
    grpc::ServerContext* context,
    const DeleteObjectRequest* request,
    DeleteObjectResponse* response
) {
    try {
        auto tenant_ctx = common::TenantContextInterceptor::ExtractFromMetadata(context);

        if (tenant_ctx.tenant_id.empty()) {
            return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "Authentication required");
        }

        auto conn_guard = db_pool_->AcquireConnection();
        pqxx::work txn(*conn_guard);

        // Soft delete
        auto result = txn.exec_params(
            "UPDATE upload_objects SET deleted_at = NOW() "
            "WHERE id = $1 AND tenant_id = $2 AND deleted_at IS NULL "
            "RETURNING size",
            request->object_id(),
            tenant_ctx.tenant_id
        );

        if (result.empty()) {
            return grpc::Status(grpc::StatusCode::NOT_FOUND, "Object not found");
        }

        // Update quota (subtract deleted file size)
        int64_t file_size = result[0]["size"].as<long long>();
        UpdateQuota(tenant_ctx.tenant_id, -file_size);

        response->set_success(true);
        txn.commit();

        return grpc::Status::OK;

    } catch (const std::exception& e) {
        return grpc::Status(grpc::StatusCode::INTERNAL, std::string("Delete failed: ") + e.what());
    }
}

grpc::Status UploadServiceImpl::GetQuota(
    grpc::ServerContext* context,
    const GetQuotaRequest* request,
    GetQuotaResponse* response
) {
    try {
        auto tenant_ctx = common::TenantContextInterceptor::ExtractFromMetadata(context);

        if (tenant_ctx.tenant_id.empty()) {
            return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "Authentication required");
        }

        auto conn_guard = db_pool_->AcquireConnection();
        pqxx::work txn(*conn_guard);

        auto result = txn.exec_params(
            "SELECT used_bytes, limit_bytes FROM quotas WHERE tenant_id = $1",
            tenant_ctx.tenant_id
        );

        if (result.empty()) {
            // Create default quota if not exists
            txn.exec_params(
                "INSERT INTO quotas (tenant_id, used_bytes, limit_bytes) VALUES ($1, 0, 10737418240)", // 10GB default
                tenant_ctx.tenant_id
            );
            response->set_used_bytes(0);
            response->set_limit_bytes(10737418240);
        } else {
            response->set_used_bytes(result[0]["used_bytes"].as<long long>());
            response->set_limit_bytes(result[0]["limit_bytes"].as<long long>());
        }

        txn.commit();

        return grpc::Status::OK;

    } catch (const std::exception& e) {
        return grpc::Status(grpc::StatusCode::INTERNAL, std::string("Get quota failed: ") + e.what());
    }
}

// Helper methods

std::string UploadServiceImpl::GenerateMockPresignedUrl(const std::string& key, int64_t expires_in) {
    // Mock S3 presigned URL (in production, use AWS SDK)
    std::stringstream ss;
    ss << "https://" << s3_bucket_ << ".s3." << s3_region_ << ".amazonaws.com/" << key
       << "?X-Amz-Algorithm=AWS4-HMAC-SHA256"
       << "&X-Amz-Expires=" << expires_in
       << "&X-Amz-SignedHeaders=host"
       << "&X-Amz-Signature=mock_signature_" << std::time(nullptr);
    return ss.str();
}

bool UploadServiceImpl::CheckQuota(const std::string& tenant_id, int64_t file_size) {
    try {
        auto conn_guard = db_pool_->AcquireConnection();
        pqxx::work txn(*conn_guard);

        auto result = txn.exec_params(
            "SELECT used_bytes, limit_bytes FROM quotas WHERE tenant_id = $1",
            tenant_id
        );

        if (result.empty()) {
            return true; // No quota set, allow upload
        }

        int64_t used = result[0]["used_bytes"].as<long long>();
        int64_t limit = result[0]["limit_bytes"].as<long long>();

        return (used + file_size) <= limit;

    } catch (const std::exception& e) {
        std::cerr << "Quota check failed: " << e.what() << std::endl;
        return false;
    }
}

void UploadServiceImpl::UpdateQuota(const std::string& tenant_id, int64_t size_delta) {
    try {
        auto conn_guard = db_pool_->AcquireConnection();
        pqxx::work txn(*conn_guard);

        txn.exec_params(
            "INSERT INTO quotas (tenant_id, used_bytes, limit_bytes) "
            "VALUES ($1, $2, 10737418240) "
            "ON CONFLICT (tenant_id) DO UPDATE SET used_bytes = quotas.used_bytes + $2",
            tenant_id,
            size_delta
        );

        txn.commit();

    } catch (const std::exception& e) {
        std::cerr << "Quota update failed: " << e.what() << std::endl;
    }
}

} // namespace upload
} // namespace saasforge
