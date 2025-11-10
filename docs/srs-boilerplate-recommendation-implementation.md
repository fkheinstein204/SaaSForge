# SaaSForge Authentication Architecture
## Hybrid JWT + mTLS Strategy

**Document Version:** 1.0  
**Date:** November 10, 2025  
**Status:** Production-Ready Recommendation

---

## Executive Summary

This document outlines the **industry-standard authentication architecture** for SaaSForge, implementing a hybrid approach that combines:

1. **JWT (JSON Web Tokens)** for user authentication
2. **mTLS (Mutual TLS)** for service-to-service communication
3. **Redis-based token revocation** for instant logout capability
4. **OAuth 2.0/OIDC integration** for third-party authentication

This architecture provides the optimal balance of **performance, security, and operational flexibility** for a production SaaS platform.

---

## 🎯 Authentication Strategy Overview

### Three-Tier Authentication Model

```
┌─────────────────────────────────────────────────────────────┐
│  Tier 1: User Authentication (React → FastAPI)              │
│  ✓ OAuth 2.0/OIDC (Google, GitHub, Microsoft)              │
│  ✓ Email/Password + 2FA (TOTP/OTP)                         │
│  ✓ Issue: JWT (15 min) + Refresh Token (30 days)           │
│  ✓ Redis blacklist for revoked tokens                       │
└────────────────────┬────────────────────────────────────────┘
                     │ JWT in gRPC metadata
                     ▼
┌─────────────────────────────────────────────────────────────┐
│  Tier 2: BFF → gRPC Services                                │
│  ✓ FastAPI validates JWT + checks Redis blacklist          │
│  ✓ Passes validated JWT to C++ gRPC services               │
│  ✓ C++ validates JWT signature + expiry                     │
│  ✓ Extracts user/tenant context for authorization           │
└────────────────────┬────────────────────────────────────────┘
                     │ mTLS between services
                     ▼
┌─────────────────────────────────────────────────────────────┐
│  Tier 3: Service-to-Service (gRPC ↔ gRPC)                  │
│  ✓ mTLS for AuthService ↔ PaymentService ↔ UploadService  │
│  ✓ Certificate-based mutual authentication                  │
│  ✓ No tokens needed for internal communication              │
│  ✓ Automatic cert rotation via cert-manager (K8s)           │
└─────────────────────────────────────────────────────────────┘
```

---

## 🔐 Tier 1: User Authentication (JWT-Based)

### Why JWT for User Authentication?

| Benefit | Impact |
|---------|--------|
| **Stateless** | No server-side session storage; easy horizontal scaling |
| **Cross-Service** | Single token works across all microservices |
| **Performance** | No database lookup on every request |
| **Industry Standard** | Excellent ecosystem support and tooling |

### JWT Token Structure

#### Access Token (Short-Lived: 15 minutes)
```json
{
  "iss": "saasforge.dev",
  "sub": "user_abc123",
  "aud": "api.saasforge.dev",
  "exp": 1699635600,
  "iat": 1699634700,
  "nbf": 1699634700,
  "jti": "token_xyz789",
  "tenant_id": "tenant_123",
  "email": "user@example.com",
  "roles": ["ADMIN"],
  "permissions": ["user:read", "upload:write"],
  "plan": "pro"
}
```

#### Refresh Token (Long-Lived: 30 days)
```json
{
  "iss": "saasforge.dev",
  "sub": "user_abc123",
  "aud": "api.saasforge.dev",
  "exp": 1702226700,
  "iat": 1699634700,
  "jti": "refresh_xyz789",
  "session_id": "session_456",
  "device_id": "device_789",
  "type": "refresh"
}
```

### Token Signing Algorithm: RS256 (RSA + SHA-256)

**Why RS256 over HS256?**
- ✅ Asymmetric signing (private key signs, public key verifies)
- ✅ Multiple services can verify without sharing signing secret
- ✅ Key rotation without service restarts
- ✅ Better security posture for distributed systems

```cpp
// C++ signing configuration
jwt::algorithm::rs256 algo(
    public_key,   // Distributed to all services
    private_key   // Only AuthService has this
);
```

### Redis Token Blacklist (Instant Revocation)

```
Key Pattern: "blacklist:{jti}"
Value: {"user_id": "...", "revoked_at": "...", "reason": "logout"}
TTL: Same as token expiry (auto-cleanup)

# Example Redis commands
SET blacklist:token_xyz789 '{"user_id":"user_abc123","reason":"logout"}' EX 900
EXISTS blacklist:token_xyz789
```

---

## 🔒 Tier 2: BFF to gRPC Service Authentication

### FastAPI JWT Validation Flow

```python
# FastAPI middleware
async def validate_jwt(request: Request):
    # 1. Extract Bearer token
    auth_header = request.headers.get("Authorization")
    token = auth_header.replace("Bearer ", "")
    
    # 2. Check Redis blacklist first (fast path)
    jti = jwt.decode(token, options={"verify_signature": False})["jti"]
    if await redis.exists(f"blacklist:{jti}"):
        raise HTTPException(401, "Token revoked")
    
    # 3. Verify signature and expiry
    try:
        payload = jwt.decode(
            token,
            public_key,
            algorithms=["RS256"],
            audience="api.saasforge.dev",
            issuer="saasforge.dev"
        )
    except jwt.ExpiredSignatureError:
        raise HTTPException(401, "Token expired")
    except jwt.InvalidTokenError as e:
        raise HTTPException(401, f"Invalid token: {e}")
    
    # 4. Attach to request context
    request.state.user_id = payload["sub"]
    request.state.tenant_id = payload["tenant_id"]
    request.state.permissions = payload["permissions"]
    
    return payload
```

### C++ gRPC JWT Validation

```cpp
#include <jwt-cpp/jwt.h>
#include <grpcpp/server_context.h>

class AuthInterceptor : public grpc::experimental::ServerInterceptor {
public:
    grpc::Status Intercept(grpc::ServerContext* context) override {
        // 1. Extract JWT from metadata
        const auto& metadata = context->client_metadata();
        auto auth_it = metadata.find("authorization");
        
        if (auth_it == metadata.end()) {
            return grpc::Status(grpc::UNAUTHENTICATED, "Missing authorization header");
        }
        
        std::string token = std::string(auth_it->second.data(), auth_it->second.size());
        if (token.substr(0, 7) == "Bearer ") {
            token = token.substr(7);
        }
        
        // 2. Decode and verify JWT
        try {
            auto verifier = jwt::verify()
                .allow_algorithm(jwt::algorithm::rs256(public_key_, ""))
                .with_issuer("saasforge.dev")
                .with_audience("api.saasforge.dev");
            
            auto decoded = jwt::decode(token);
            verifier.verify(decoded);
            
            // 3. Check Redis blacklist
            std::string jti = decoded.get_payload_claim("jti").as_string();
            if (redis_->exists("blacklist:" + jti)) {
                return grpc::Status(grpc::UNAUTHENTICATED, "Token revoked");
            }
            
            // 4. Extract claims for authorization
            std::string user_id = decoded.get_payload_claim("sub").as_string();
            std::string tenant_id = decoded.get_payload_claim("tenant_id").as_string();
            
            // 5. Attach to context
            context->AddInitialMetadata("x-user-id", user_id);
            context->AddInitialMetadata("x-tenant-id", tenant_id);
            
            return grpc::Status::OK;
            
        } catch (const std::exception& e) {
            return grpc::Status(grpc::UNAUTHENTICATED, 
                std::string("Invalid token: ") + e.what());
        }
    }
    
private:
    std::string public_key_;
    std::shared_ptr<RedisClient> redis_;
};
```

---

## 🛡️ Tier 3: Service-to-Service Authentication (mTLS)

### Why mTLS for Internal Services?

| Benefit | Description |
|---------|-------------|
| **Strongest Authentication** | Cryptographic proof of identity |
| **No Token Management** | No JWT generation/validation overhead |
| **Native gRPC Support** | Built-in, battle-tested implementation |
| **Automatic Rotation** | cert-manager handles cert lifecycle |
| **Mutual Verification** | Both client and server authenticate |

### mTLS Configuration for gRPC Services

#### Server Configuration (C++)

```cpp
#include <grpcpp/security/server_credentials.h>

std::shared_ptr<grpc::ServerCredentials> CreateMtlsCredentials() {
    grpc::SslServerCredentialsOptions ssl_opts(
        GRPC_SSL_REQUEST_AND_REQUIRE_CLIENT_CERTIFICATE_AND_VERIFY
    );
    
    // Load CA certificate (validates client certs)
    ssl_opts.pem_root_certs = ReadFile("/etc/certs/ca.pem");
    
    // Load server certificate and private key
    grpc::SslServerCredentialsOptions::PemKeyCertPair key_cert_pair;
    key_cert_pair.private_key = ReadFile("/etc/certs/server-key.pem");
    key_cert_pair.cert_chain = ReadFile("/etc/certs/server-cert.pem");
    ssl_opts.pem_key_cert_pairs.push_back(key_cert_pair);
    
    return grpc::SslServerCredentials(ssl_opts);
}

// Server setup
grpc::ServerBuilder builder;
builder.AddListeningPort(
    "0.0.0.0:50051",
    CreateMtlsCredentials()
);
auto server = builder.BuildAndStart();
```

#### Client Configuration (C++)

```cpp
#include <grpcpp/security/credentials.h>

std::shared_ptr<grpc::ChannelCredentials> CreateMtlsClientCredentials() {
    grpc::SslCredentialsOptions ssl_opts;
    
    // Load CA certificate (validates server cert)
    ssl_opts.pem_root_certs = ReadFile("/etc/certs/ca.pem");
    
    // Load client certificate and private key
    ssl_opts.pem_private_key = ReadFile("/etc/certs/client-key.pem");
    ssl_opts.pem_cert_chain = ReadFile("/etc/certs/client-cert.pem");
    
    return grpc::SslCredentials(ssl_opts);
}

// Client setup
auto channel = grpc::CreateChannel(
    "payment-service:50051",
    CreateMtlsClientCredentials()
);
auto stub = PaymentService::NewStub(channel);
```

### Certificate Management with cert-manager (Kubernetes)

```yaml
# Certificate for AuthService
apiVersion: cert-manager.io/v1
kind: Certificate
metadata:
  name: auth-service-cert
  namespace: saasforge
spec:
  secretName: auth-service-tls
  duration: 2160h # 90 days
  renewBefore: 360h # 15 days
  subject:
    organizations:
      - saasforge
  commonName: auth-service.saasforge.svc.cluster.local
  dnsNames:
    - auth-service
    - auth-service.saasforge
    - auth-service.saasforge.svc
    - auth-service.saasforge.svc.cluster.local
  issuerRef:
    name: internal-ca-issuer
    kind: ClusterIssuer
```

---

## 🔄 Token Lifecycle Management

### Access Token Flow

```
1. User Login
   ↓
2. AuthService validates credentials + 2FA
   ↓
3. Generate JWT (exp: now + 15min)
   ↓
4. Return {access_token, refresh_token, expires_in}
   ↓
5. Client stores tokens (memory for access, httpOnly cookie for refresh)
   ↓
6. Client includes Bearer token in requests
   ↓
7. Token expires after 15 minutes
   ↓
8. Client uses refresh token to get new access token
```

### Refresh Token Rotation

```
1. Client sends refresh_token to /v1/auth/refresh
   ↓
2. AuthService validates refresh token
   ↓
3. Check if token was already used (reuse detection)
   ↓
4. If reused: Revoke entire session (security breach)
   ↓
5. If valid: Generate new access + refresh tokens
   ↓
6. Invalidate old refresh token
   ↓
7. Return new tokens
```

### Token Revocation Strategies

#### 1. Logout (Immediate)
```python
# Add token to Redis blacklist
jti = decode_jwt(access_token)["jti"]
redis.setex(
    f"blacklist:{jti}",
    900,  # TTL matches token expiry
    json.dumps({"reason": "logout", "timestamp": now()})
)
```

#### 2. Security Event (All Sessions)
```python
# Revoke all tokens for user
user_sessions = db.query(Session).filter(Session.user_id == user_id).all()
for session in user_sessions:
    # Blacklist refresh token
    redis.setex(f"blacklist:{session.refresh_jti}", TTL, ...)
    # Mark session as revoked
    session.status = "revoked"
db.commit()
```

#### 3. Password Change (All Sessions Except Current)
```python
# Revoke all sessions except the one that changed password
for session in user_sessions:
    if session.id != current_session_id:
        redis.setex(f"blacklist:{session.refresh_jti}", TTL, ...)
```

---

## 🔧 Recommended C++ Libraries

### 1. jwt-cpp (⭐ Recommended for JWT)

**Installation:**
```bash
# Via vcpkg
vcpkg install jwt-cpp

# Via CMake FetchContent
FetchContent_Declare(
  jwt-cpp
  GIT_REPOSITORY https://github.com/Thalhammer/jwt-cpp.git
  GIT_TAG v0.7.0
)
FetchContent_MakeAvailable(jwt-cpp)
```

**CMakeLists.txt:**
```cmake
find_package(jwt-cpp CONFIG REQUIRED)
target_link_libraries(auth_service PRIVATE jwt-cpp::jwt-cpp)
```

**Features:**
- ✅ Header-only library (easy integration)
- ✅ Supports RS256, RS384, RS512, HS256, HS384, HS512, ES256, ES384, ES512
- ✅ Modern C++11/14/17 API
- ✅ Excellent performance
- ✅ Active maintenance
- ✅ OpenSSL and LibreSSL support

**Basic Usage:**
```cpp
#include <jwt-cpp/jwt.h>

// Create token
auto token = jwt::create()
    .set_issuer("saasforge.dev")
    .set_subject("user_123")
    .set_audience("api.saasforge.dev")
    .set_issued_at(std::chrono::system_clock::now())
    .set_expires_at(std::chrono::system_clock::now() + std::chrono::minutes{15})
    .set_payload_claim("tenant_id", jwt::claim(std::string("tenant_123")))
    .set_payload_claim("email", jwt::claim(std::string("user@example.com")))
    .sign(jwt::algorithm::rs256(public_key, private_key));

// Verify token
auto verifier = jwt::verify()
    .allow_algorithm(jwt::algorithm::rs256(public_key))
    .with_issuer("saasforge.dev")
    .with_audience("api.saasforge.dev");

auto decoded = jwt::decode(token);
verifier.verify(decoded);

// Extract claims
std::string user_id = decoded.get_subject();
std::string tenant_id = decoded.get_payload_claim("tenant_id").as_string();
```

### 2. gRPC C++ (Built-in mTLS Support)

**Installation:**
```bash
# Via vcpkg
vcpkg install grpc

# Or build from source
git clone -b v1.59.0 https://github.com/grpc/grpc
cd grpc
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
sudo make install
```

**CMakeLists.txt:**
```cmake
find_package(gRPC CONFIG REQUIRED)
target_link_libraries(auth_service PRIVATE
    gRPC::grpc++
    gRPC::grpc++_reflection
)
```

### 3. Redis Client: redis-plus-plus

**Installation:**
```bash
# Via vcpkg
vcpkg install redis-plus-plus

# Or build from source
git clone https://github.com/sewenew/redis-plus-plus.git
cd redis-plus-plus
mkdir build && cd build
cmake ..
make
sudo make install
```

**CMakeLists.txt:**
```cmake
find_package(redis++ CONFIG REQUIRED)
target_link_libraries(auth_service PRIVATE redis++::redis++)
```

**Usage:**
```cpp
#include <sw/redis++/redis++.h>

auto redis = sw::redis::Redis("tcp://127.0.0.1:6379");

// Check blacklist
bool is_revoked = redis.exists("blacklist:token_xyz");

// Add to blacklist with TTL
redis.setex("blacklist:token_xyz", 900, R"({"reason":"logout"})");
```

---

## 📊 Performance Characteristics

### JWT Validation Performance

| Operation | Latency (p50) | Latency (p99) | Notes |
|-----------|---------------|---------------|-------|
| **Redis Blacklist Check** | 0.5ms | 2ms | Network RTT + lookup |
| **JWT Signature Verification (RS256)** | 1ms | 3ms | CPU-bound, can cache public key |
| **JWT Decode** | 0.1ms | 0.5ms | JSON parsing only |
| **Total Auth Overhead** | 1.6ms | 5.5ms | Includes all checks |

### mTLS Handshake Performance

| Operation | Latency (p50) | Latency (p99) | Notes |
|-----------|---------------|---------------|-------|
| **Full TLS Handshake** | 5ms | 15ms | Only on connection establishment |
| **TLS Resume (session ticket)** | 1ms | 3ms | Subsequent requests |
| **Authenticated Request** | 0ms | 0ms | No overhead after handshake |

### Comparison: JWT vs Session vs mTLS

| Metric | JWT | Session + Redis | mTLS |
|--------|-----|-----------------|------|
| **Auth Latency** | 1.6ms | 2ms | 0ms (after handshake) |
| **Scalability** | Excellent | Good (Redis bottleneck) | Excellent |
| **Revocation Speed** | Instant (with Redis) | Instant | Instant (cert revoke) |
| **Implementation Complexity** | Low | Low | Medium |
| **Security** | ⭐⭐⭐⭐ | ⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ |

---

## 🎯 Implementation Roadmap

### Phase 1: JWT Foundation (Week 1-2)
- [ ] Integrate jwt-cpp library
- [ ] Implement JWT generation in AuthService
- [ ] Create RS256 key pair (4096-bit)
- [ ] Implement JWT validation middleware in FastAPI
- [ ] Add JWT validation interceptor in C++ gRPC services
- [ ] Set up Redis for token blacklist
- [ ] Implement logout with token revocation
- [ ] Add refresh token rotation

### Phase 2: Redis Integration (Week 2)
- [ ] Set up Redis cluster (3 nodes)
- [ ] Implement blacklist operations
- [ ] Add rate limiting with Redis
- [ ] Implement session tracking
- [ ] Add metrics for Redis operations

### Phase 3: mTLS for Services (Week 3-4)
- [ ] Generate CA certificate for internal services
- [ ] Create service certificates (AuthService, PaymentService, UploadService, NotificationService)
- [ ] Implement mTLS server configuration
- [ ] Implement mTLS client configuration
- [ ] Test mutual authentication
- [ ] Set up cert-manager in Kubernetes
- [ ] Automate certificate rotation
- [ ] Monitor certificate expiry

### Phase 4: OAuth 2.0/OIDC (Week 5)
- [ ] Integrate OAuth providers (Google, GitHub, Microsoft)
- [ ] Implement OAuth callback handler
- [ ] Link OAuth accounts to users
- [ ] Add social login UI
- [ ] Test OAuth flows

### Phase 5: Testing & Hardening (Week 6)
- [ ] Unit tests for JWT generation/validation
- [ ] Integration tests for auth flows
- [ ] Load testing (10k req/s)
- [ ] Security audit
- [ ] Penetration testing
- [ ] Token theft scenario testing
- [ ] mTLS failover testing

---

## 🔒 Security Best Practices

### 1. Key Management

```yaml
# Kubernetes Secret for JWT keys
apiVersion: v1
kind: Secret
metadata:
  name: jwt-keys
  namespace: saasforge
type: Opaque
data:
  private-key.pem: <base64-encoded-private-key>
  public-key.pem: <base64-encoded-public-key>
```

**Key Rotation Strategy:**
```
1. Generate new key pair (kid: v2)
2. Deploy new public key to all services
3. AuthService signs with new key (kid: v2)
4. Services accept both v1 and v2 for 24 hours
5. After 24h, remove v1 public key
```

### 2. Token Security Checklist

- ✅ Use RS256 (asymmetric) not HS256 (symmetric)
- ✅ Set short expiry (15 min for access tokens)
- ✅ Rotate refresh tokens on every use
- ✅ Detect and block token reuse
- ✅ Bind tokens to device/session
- ✅ Include `jti` (JWT ID) for revocation
- ✅ Validate `aud`, `iss`, `exp`, `nbf` claims
- ✅ Use Redis blacklist for instant revocation
- ✅ Never log full tokens (only last 8 chars)
- ✅ Store refresh tokens hashed in database

### 3. mTLS Security Checklist

- ✅ Use 2048-bit or 4096-bit RSA keys
- ✅ Set certificate validity to 90 days
- ✅ Automate rotation with cert-manager
- ✅ Use separate CA for internal services
- ✅ Never share service private keys
- ✅ Monitor certificate expiry
- ✅ Implement certificate pinning
- ✅ Validate certificate chain
- ✅ Use TLS 1.3 where possible
- ✅ Disable TLS 1.0 and 1.1

### 4. Redis Security

```conf
# redis.conf
requirepass <strong-password>
bind 127.0.0.1
protected-mode yes
maxmemory 2gb
maxmemory-policy allkeys-lru
tls-port 6380
tls-cert-file /etc/redis/redis.crt
tls-key-file /etc/redis/redis.key
tls-ca-cert-file /etc/redis/ca.crt
```

---

## 🚨 Monitoring & Alerting

### Key Metrics to Track

```yaml
# Prometheus metrics
auth_jwt_validation_duration_seconds_bucket
auth_jwt_validation_errors_total{error_type="expired|invalid|revoked"}
auth_blacklist_check_duration_seconds_bucket
auth_refresh_token_rotations_total
auth_refresh_token_reuse_detected_total  # Critical alert
auth_mtls_handshake_duration_seconds_bucket
auth_mtls_handshake_errors_total{error_type="cert_expired|cert_invalid"}

# Redis metrics
redis_blacklist_keys_count
redis_blacklist_hit_rate
redis_latency_seconds_bucket
```

### Critical Alerts

```yaml
# Alert: Token reuse detected (potential security breach)
- alert: RefreshTokenReuseDetected
  expr: rate(auth_refresh_token_reuse_detected_total[5m]) > 0
  severity: critical
  annotations:
    summary: "Refresh token reuse detected - potential token theft"

# Alert: High JWT validation error rate
- alert: HighJWTValidationErrors
  expr: rate(auth_jwt_validation_errors_total[5m]) > 10
  severity: warning
  annotations:
    summary: "High rate of JWT validation errors"

# Alert: mTLS certificate expiring soon
- alert: MTLSCertificateExpiring
  expr: (cert_expiry_timestamp_seconds - time()) < 604800  # 7 days
  severity: warning
  annotations:
    summary: "mTLS certificate expiring in less than 7 days"

# Alert: Redis blacklist down
- alert: RedisBlacklistDown
  expr: up{job="redis"} == 0
  severity: critical
  annotations:
    summary: "Redis blacklist unavailable - token revocation disabled"
```

---

## 📚 Alternative Approaches Considered

### 1. PASETO (Platform-Agnostic Security Tokens)

**Pros:**
- More secure defaults than JWT
- No algorithm confusion attacks
- Versioned protocol

**Cons:**
- ⚠️ Less ecosystem support
- ⚠️ Fewer C++ libraries (paseto-cpp is immature)
- ⚠️ Limited third-party integration

**Verdict:** Consider for future version after ecosystem matures.

### 2. Session-Based Authentication (Redis)

**Pros:**
- Instant revocation without blacklist
- Fine-grained session control
- Simple to understand

**Cons:**
- ⚠️ Not stateless (Redis bottleneck)
- ⚠️ Requires sticky sessions or session replication
- ⚠️ More database lookups

**Verdict:** Use for admin dashboard, not main API.

### 3. Macaroons (Attenuated Tokens)

**Pros:**
- Contextual caveats (time, location, resource)
- Cryptographically verifiable delegation

**Cons:**
- ⚠️ Complex implementation
- ⚠️ Limited tooling
- ⚠️ Overkill for most use cases

**Verdict:** Overkill for SaaSForge; consider for advanced use cases.

### 4. OAuth 2.0 Only (No JWT)

**Pros:**
- Industry standard for external auth
- Rich ecosystem

**Cons:**
- ⚠️ Requires OAuth server for internal auth
- ⚠️ More complex flow
- ⚠️ Opaque tokens require introspection endpoint

**Verdict:** Use OAuth for third-party login, JWT for API access.

---

## ✅ Final Recommendations

### For SaaSForge Production Deployment:

1. **User Authentication:** JWT (jwt-cpp) with Redis blacklist
   - Short-lived access tokens (15 min)
   - Rotating refresh tokens (30 days)
   - RS256 signing with key rotation

2. **Service-to-Service:** mTLS with cert-manager
   - Mutual authentication between gRPC services
   - Automatic certificate rotation
   - TLS 1.3 where supported

3. **API Keys:** Custom validation with scopes
   - Store hashed in PostgreSQL
   - Validate in FastAPI BFF
   - Convert to internal JWT for gRPC calls

4. **OAuth Integration:** Google, GitHub, Microsoft
   - Implemented in FastAPI BFF
   - Convert OAuth tokens to internal JWTs
   - Link to existing accounts

### Key Benefits of This Approach:

✅ **Performance:** < 2ms auth overhead for JWT, 0ms for mTLS  
✅ **Security:** Industry-standard algorithms, instant revocation, mutual auth  
✅ **Scalability:** Stateless JWT, connection pooling with mTLS  
✅ **Operability:** Automatic cert rotation, comprehensive metrics  
✅ **Flexibility:** Easy to add new auth methods (WebAuthn, SAML)  
✅ **Compliance:** PCI-DSS, SOC 2, GDPR compatible  

---

## 📖 References

1. [RFC 7519 - JSON Web Token (JWT)](https://datatracker.ietf.org/doc/html/rfc7519)
2. [RFC 8725 - JWT Best Current Practices](https://datatracker.ietf.org/doc/html/rfc8725)
3. [jwt-cpp Documentation](https://thalhammer.github.io/jwt-cpp/)
4. [gRPC Authentication Guide](https://grpc.io/docs/guides/auth/)
5. [Redis Security Best Practices](https://redis.io/docs/management/security/)
6. [cert-manager Documentation](https://cert-manager.io/docs/)
7. [OWASP JWT Cheat Sheet](https://cheatsheetseries.owasp.org/cheatsheets/JSON_Web_Token_for_Java_Cheat_Sheet.html)

---

## 🤝 Contributing

For questions or suggestions about this authentication architecture:

- **Architecture Review:** @tech-lead-security
- **Implementation Questions:** @backend-team
- **Security Concerns:** security@saasforge.dev

---

**Document Maintainer:** SaaSForge Architecture Team  
**Last Updated:** November 10, 2025  
**Next Review:** February 10, 2026
