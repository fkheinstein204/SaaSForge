#pragma once

#include <grpcpp/grpcpp.h>
// #include "auth.grpc.pb.h"  // Generated from proto file

namespace saasforge {
namespace auth {

class AuthServiceImpl /* : public auth::AuthService::Service */ {
public:
    AuthServiceImpl();

    // TODO: Implement methods from auth.proto:
    // - Login
    // - Logout
    // - RefreshToken
    // - ValidateToken
    // - CreateApiKey
    // - RevokeApiKey
};

} // namespace auth
} // namespace saasforge
