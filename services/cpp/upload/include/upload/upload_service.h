#pragma once

#include <grpcpp/grpcpp.h>

namespace saasforge {
namespace upload {

class UploadServiceImpl {
public:
    UploadServiceImpl();

    // TODO: Implement methods from upload.proto:
    // - GeneratePresignedUrl
    // - CompleteUpload
    // - TransformObject
    // - DeleteObject
    // - GetQuota
};

} // namespace upload
} // namespace saasforge
