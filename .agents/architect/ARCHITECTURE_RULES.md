# SaaSForge Architecture Rules

## 🏗️ Three-Tier Architecture

```
┌─────────────────────────────────────────┐
│     React UI (Port 3000)                │
│  - Presentation layer only              │
│  - No business logic                    │
│  - State management (Zustand)           │
│  - Calls BFF via REST                   │
└─────────────┬───────────────────────────┘
              │ HTTP/REST
┌─────────────▼───────────────────────────┐
│     FastAPI BFF (Port 8000)             │
│  - Backend for Frontend                 │
│  - Request aggregation                  │
│  - JWT validation                       │
│  - Rate limiting                        │
│  - Calls services via gRPC              │
└─────────────┬───────────────────────────┘
              │ gRPC + mTLS
┌─────────────▼───────────────────────────┐
│  C++ gRPC Services (Ports 50051-54)     │
│  - Business logic                       │
│  - Data persistence                     │
│  - Stateless                            │
│  - Multi-tenant isolation               │
└─────────────────────────────────────────┘
```

## ❌ Anti-Patterns

### UI Layer Violations
- ❌ Direct gRPC calls
- ❌ Direct database access
- ❌ Business logic in components
- ❌ Unencrypted token storage
- ❌ Client-side validation only

### BFF Layer Violations
- ❌ Business logic implementation
- ❌ Direct database access
- ❌ Synchronous sequential gRPC calls (>3)
- ❌ State persistence
- ❌ Complex calculations

### Service Layer Violations
- ❌ Shared mutable state
- ❌ Direct HTTP/REST endpoints
- ❌ UI-specific logic
- ❌ Unvalidated tenant context
- ❌ Missing error codes

## ✅ Correct Patterns

### 1. UI → BFF → Service Flow

**Good Example:**
```typescript
// UI: components/UploadButton.tsx
const handleUpload = async () => {
  const response = await apiClient.post('/v1/uploads/presign', data);
  // Use presigned URL
}

// BFF: api/routers/upload.py
async def generate_presigned_url(request: PresignedUrlRequest):
    return await upload_service_stub.GeneratePresignedUrl(request)

// Service: services/cpp/upload/src/upload_service.cpp
grpc::Status GeneratePresignedUrl(...) {
    // Business logic: validate quota, generate URL
}
```

**Bad Example:**
```typescript
// ❌ UI directly accessing gRPC
const stub = new UploadServiceClient(...);
await stub.generatePresignedUrl(...);
```

### 2. Tenant Isolation

**Good Example:**
```python
# BFF extracts from JWT
tenant_id = request.state.tenant_id

# Service validates against resource
if resource.tenant_id != request.tenant_id:
    return grpc::Status(PERMISSION_DENIED)
```

**Bad Example:**
```python
# ❌ Client provides tenant_id
tenant_id = request.body.tenant_id  # Security vulnerability!
```

### 3. Parallel gRPC Calls

**Good Example:**
```python
# BFF aggregates multiple services
results = await asyncio.gather(
    auth_stub.ValidateToken(token),
    user_stub.GetProfile(user_id),
    quota_stub.CheckQuota(tenant_id)
)
```

**Bad Example:**
```python
# ❌ Sequential calls (slow)
user = await auth_stub.ValidateToken(token)
profile = await user_stub.GetProfile(user_id)
quota = await quota_stub.CheckQuota(tenant_id)
```

### 4. Error Handling

**Good Example:**
```cpp
// Service returns standard gRPC status
return grpc::Status(
    grpc::StatusCode::PERMISSION_DENIED,
    "AUTH_003: Invalid credentials"
);
```

**Bad Example:**
```cpp
// ❌ Throwing exceptions
throw std::runtime_error("Invalid credentials");
```

## 🎯 Design Principles

### 1. Separation of Concerns
- **UI**: Presentation, user interaction
- **BFF**: Aggregation, orchestration, client-specific logic
- **Services**: Business logic, data persistence

### 2. Statelessness
- Services must be stateless for horizontal scaling
- State stored in: Database, Redis, or client

### 3. Multi-Tenancy
- All queries filtered by `tenant_id`
- Tenant context extracted from JWT (never request body)
- Cross-tenant access tests mandatory

### 4. Resilience
- Timeouts on all external calls
- Retry with exponential backoff
- Circuit breakers for external services
- Graceful degradation

### 5. Observability
- Correlation IDs through entire request chain
- Structured logging with tenant context
- Metrics on critical paths
- Distributed tracing

## 📋 Architecture Decision Records (ADRs)

### ADR-001: Three-Tier Architecture
**Context:** Need for clear separation, scalability, and language optimization
**Decision:** React UI → FastAPI BFF → C++ gRPC Services
**Consequences:**
- ✅ Clear boundaries, independent scaling
- ❌ Increased complexity, network hops

### ADR-002: C++ for Core Services
**Context:** Performance requirements (JWT < 2ms, login < 500ms)
**Decision:** Use C++ for business logic services
**Consequences:**
- ✅ 10x faster than Node.js for JWT validation
- ❌ Longer development time, smaller talent pool

### ADR-003: BFF Pattern
**Context:** Mobile and web clients have different needs
**Decision:** FastAPI as Backend for Frontend
**Consequences:**
- ✅ Client-specific optimizations, request aggregation
- ❌ Additional layer to maintain

### ADR-004: mTLS for Service-to-Service
**Context:** Need strong authentication between services
**Decision:** Mutual TLS with certificate-based auth
**Consequences:**
- ✅ Zero-trust security, automatic encryption
- ❌ Certificate management complexity

### ADR-005: JWT + Redis Blacklist
**Context:** Need fast token validation with instant revocation
**Decision:** RS256 JWT with Redis blacklist
**Consequences:**
- ✅ < 2ms validation, instant logout
- ❌ Redis dependency for auth

## 🔍 Review Checklist

Before merging code, verify:

- [ ] No cross-layer violations
- [ ] Tenant isolation enforced
- [ ] No business logic in UI
- [ ] No database access in BFF
- [ ] gRPC calls parallelized where possible
- [ ] Proper error codes used
- [ ] Timeouts configured
- [ ] Logging includes correlation_id
- [ ] Tests cover multi-tenancy
- [ ] Documentation updated

## 📚 References

- SRS Document: `docs/srs-boilerplate-saas.md`
- Authentication Architecture: `docs/srs-boilerplate-recommendation-implementation.md`
- ADRs: `docs/architecture/decisions/`
