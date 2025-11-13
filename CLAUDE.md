# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.



# Developer: Heinstein F.

## Contact
- Email: fkheinstein@gmail.com
- GitHub: @fkheinstein204

## Code Style Preferences
- Author comments: Use "Heinstein" in file headers
- Commit messages: Sign as "Heinstein"
- Documentation: Attribute work to Heinstein F.

## File Headers Template
Use this format for new files:
```cpp
/**
 * @file        <filename>.cpp
 * @brief       <Short description of the file’s purpose>
 *
 * @copyright   (c) 2025 FtsCoDe GmbH. All rights reserved.
 * @author      Heinstein F.
 * @date        2025-11-02
 *
 * @details
 * Provide a concise description of what this file implements,
 * including context, responsibilities, or usage examples if needed.
 */
```


## Project Overview

**SaaSForge** is a production-ready, multi-tenant SaaS boilerplate designed to accelerate teams from prototype to launch. It provides enterprise-grade foundations for authentication, file processing, payments, notifications, and observability.

**Current Status:** Specification-complete (v0.3) with comprehensive architecture documentation. Implementation phase ready to begin.

**Scope:** 128 functional requirements across 5 subsystems:
- **A. Authentication (1-39):** JWT + mTLS + OAuth 2.0/OIDC
- **B. Upload (40-62):** Presigned URLs, transformations, security
- **C. Payment (63-88):** Stripe subscriptions, invoicing, metering
- **D. Notifications (89-109):** Multi-channel delivery, webhooks
- **E. Email & Transactions (110-128):** Queue management, distributed transactions

## Architecture

### Three-Tier System Design

```
React/TypeScript UI (port 3000)
    ↓ HTTPS/REST + WebSocket
FastAPI BFF (port 8000) - Rate limiting, JWT validation, presigned URLs
    ↓ gRPC + mTLS (port 50051+)
C++ gRPC Services - AuthService, UploadService, PaymentService, NotificationService
    ↓
PostgreSQL 15+ | Redis 7+ | S3/R2 Storage | AWS KMS
```

### Three-Tier Authentication Model

**Tier 1: User Authentication (JWT-based)**
- OAuth 2.0/OIDC (Google, GitHub, Microsoft) + email/password + 2FA (TOTP/OTP)
- **Access token:** RS256 signed, 15-minute expiry
- **Refresh token:** 30-day expiry with rotation on every use
- **Revocation:** Redis blacklist (`blacklist:{jti}`) for instant logout
- **Security:** Refresh token reuse detection → immediate session revocation

**Tier 2: BFF → gRPC Services**
- FastAPI validates JWT signature + checks Redis blacklist
- Validated token passed to C++ services via gRPC metadata
- C++ services perform additional JWT validation + extract user/tenant context

**Tier 3: Service-to-Service (mTLS)**
- Mutual TLS with certificate-based authentication (no tokens)
- Automatic cert rotation every 90 days via cert-manager
- TLS 1.3 preferred, TLS 1.2 minimum (TLS 1.0/1.1 disabled)
- Certificate revocation via CRL/OCSP (1-hour emergency revocation SLA)

### Critical Security Patterns

**JWT Token Validation (Requirement A-22):**
- Explicitly whitelist RS256 algorithm only
- Reject unsigned tokens (`alg: none`) to prevent algorithm confusion attacks
- Check: signature validity, expiration, audience, issuer, Redis blacklist status
- See: `docs/srs-boilerplate-recommendation-implementation.md:194-228` for C++ implementation

**API Key Scopes (Requirement A-14):**
- Validate scopes against requested resource actions
- Support wildcard scopes (e.g., `read:*`) with prefix matching
- Deny by default when scope is ambiguous or unmatched

**CSRF Protection (Requirement A-31):**
- Required for POST/PUT/PATCH/DELETE when using cookie-based auth
- Generate unique CSRF tokens per session
- Validate server-side before processing; reject with HTTP 403 if missing/invalid

**Upload Security (Requirements B-41, B-43, B-44):**
- Presigned URLs: 10-minute TTL, bound to exact Content-Length + Content-Type + x-amz-checksum-sha256
- Multipart uploads: Per-part SHA-256 checksums, validate before CompleteMultipartUpload
- Quota enforcement: Hard limit + 10% grace overage for paid plans, reset daily at midnight UTC

**Webhook URL Validation (Requirement D-102):**
- Reject private IP ranges (10.0.0.0/8, 172.16.0.0/12, 192.168.0.0/16, 127.0.0.0/8)
- Reject non-standard ports (except 80, 443, 8080, 8443)
- Follow maximum 2 redirects with re-validation to prevent SSRF attacks

**Idempotency Keys (Requirement E-124):**
- Scoped per tenant + user
- 24-hour window; same key may be reused after expiry
- Optional for most endpoints, **REQUIRED** for payment and subscription endpoints
- Ensures exactly-once semantics for state-changing operations

## Development Workflow

### Build Commands (when implemented)

**C++ gRPC Services:**
```bash
cd services/cpp
mkdir build && cd build
cmake .. && make -j$(nproc)
./auth_service &         # Port 50051
./upload_service &       # Port 50052
./payment_service &      # Port 50053
./notification_service & # Port 50054
ctest --output-on-failure  # Run C++ unit tests
```

**FastAPI BFF:**
```bash
cd api
python -m venv venv
source venv/bin/activate  # On Windows: venv\Scripts\activate
pip install -r requirements.txt
uvicorn main:app --reload --port 8000
pytest tests/ -v --cov  # Run Python tests with coverage
```

**React UI:**
```bash
cd ui
npm install
npm run dev              # Vite dev server at localhost:3000
npm run build            # Production build
npm run test:unit        # Jest unit tests
npm run test:e2e         # Cypress E2E tests
```

**Infrastructure:**
```bash
docker-compose up -d postgres redis  # Start dependencies
./scripts/migrate.sh                 # Run database migrations
docker-compose up -d                 # Start all services
```

### Testing Strategy

| Test Type | Framework | Command | Coverage |
|-----------|-----------|---------|----------|
| **Unit** | gtest (C++), pytest (Python), Jest (TS) | `ctest`, `pytest`, `npm test` | Policy evaluation, crypto, calculations |
| **Integration** | pytest + testcontainers | `pytest tests/integration/` | S3 presign, gRPC→DB, Stripe API |
| **E2E** | Playwright, Cypress | `npm run test:e2e` | Browser flows, multi-step auth, checkout |
| **Load** | k6, Locust | `k6 run tests/load/api_load_test.js` | Login surge (1000 req/s), 5GB upload |
| **Security** | Semgrep (SAST), OWASP ZAP (DAST) | In CI/CD pipeline | Dependency scanning, vulnerability detection |

## Technology Stack

### Critical Libraries & Usage

**JWT (C++):**
- **Library:** `jwt-cpp` (header-only, v0.7.0+)
- **Installation:** `vcpkg install jwt-cpp` or CMake FetchContent
- **Algorithm:** RS256 with 4096-bit RSA keys (never use HS256)
- **CMake:** `target_link_libraries(auth_service PRIVATE jwt-cpp::jwt-cpp)`
- **Example:** See `docs/srs-boilerplate-recommendation-implementation.md:440-467`

**Redis (C++):**
- **Library:** `redis-plus-plus`
- **Installation:** `vcpkg install redis-plus-plus`
- **Usage:** Token blacklist, rate limiting, session storage
- **Example:** `redis.setex("blacklist:token_xyz", 900, R"({"reason":"logout"})")`

**gRPC (C++):**
- **Version:** 1.59.0+
- **mTLS:** Built-in SSL credentials with client cert verification
- **Installation:** `vcpkg install grpc` or build from source
- **Server:** `grpc::SslServerCredentials` with `GRPC_SSL_REQUEST_AND_REQUIRE_CLIENT_CERTIFICATE_AND_VERIFY`
- **Client:** `grpc::SslCredentials` with CA cert, client cert, and private key

**Database (C++):**
- **Library:** `libpqxx` for PostgreSQL
- **Isolation Levels:** `READ COMMITTED` (default), `SERIALIZABLE` (financial operations)

**FastAPI (Python):**
- **Framework:** FastAPI with Pydantic v2 for validation
- **ORM:** SQLAlchemy 2.0
- **Redis:** `redis-py` with async support
- **JWT:** `PyJWT` with `RS256` algorithm

**React (TypeScript):**
- **Framework:** React 18 with TypeScript
- **State:** Zustand (lightweight store)
- **Data Fetching:** React Query (TanStack Query)
- **Styling:** TailwindCSS
- **Build:** Vite

## Performance Targets (p95)

| Operation | Target | Notes |
|-----------|--------|-------|
| JWT validation | ≤ 2ms | Signature + blacklist check (see NFR-P1a) |
| mTLS handshake (full) | ≤ 15ms | Initial connection setup (see NFR-P1b) |
| mTLS resume | ≤ 3ms | With session tickets |
| Login (no 2FA) | ≤ 500ms | Credential validation (see NFR-P1) |
| Login (with TOTP) | ≤ 1.2s | Including 2FA step-up |
| Presign request | ≤ 300ms | 99% at scale (see NFR-P2) |
| Payment processing | ≤ 3s | Including Stripe round-trip (see NFR-P4) |
| Critical email delivery | ≤ 30s | OTP, password reset (see NFR-P6) |

## Retry Policies

**Email/SMS/Push Notifications (Requirement D-98):**
- Max 3 retries with exponential backoff: 1s, 5s, 30s
- For fast failure on transient errors

**Webhooks (Requirement D-104):**
- Max 5 retries with exponential backoff: 1s, 5s, 30s, 5min, 30min
- Extended retries for external system tolerance
- Disable webhook after 10 consecutive failures

**Failed Payments (Requirement C-71):**
- Fixed schedule: Day 1, Day 3, Day 7 after failure
- Email notification before each retry
- After 3 failed retries, transition subscription to 'unpaid' status

**Email Bounce Handling (Requirement E-113):**
- Hard bounce: Permanent failure, suppress future sends immediately
- Soft bounce: Temporary failure, retry up to 3 times over 72 hours
- Alert when bounce rate exceeds 5% for any email type

## Documentation Guide

### Primary Documents

**1. Software Requirements Specification (SRS)**
- **File:** `docs/srs-boilerplate-saas.md` (1,496 lines)
- **Version:** 0.3
- **Coverage:** 128 functional requirements + 24 NFRs

**Key Sections:**
- **Section 1.4:** Authentication architecture overview (JWT + mTLS + OAuth)
- **Section 3:** API contracts with examples (presign, create subscription, send notification)
- **Section 5:** Functional requirements by subsystem (A: 1-39, B: 40-62, C: 63-88, D: 89-109, E: 110-128)
- **Section 6:** Non-functional requirements (performance, availability, security, compliance)
- **Section 8:** Authorization model (RBAC + ABAC with examples)
- **Section 9:** Error codes (34 total: AUTH_001-008, UP_001-006, PAY_001-008, NOTIF_001-006, EMAIL_001-004)
- **Section 11:** Monitoring metrics (33 total with thresholds and alert conditions)
- **Section 12:** Acceptance criteria (39 test scenarios across all subsystems)
- **Section 13:** Data protection & retention (GDPR compliance, 7-year retention for financial data)

**2. Authentication Architecture**
- **File:** `docs/srs-boilerplate-recommendation-implementation.md` (850 lines)
- **Purpose:** Detailed JWT + mTLS implementation guide

**Key Sections:**
- **Lines 69-101:** JWT token structure (access + refresh tokens)
- **Lines 136-169:** FastAPI JWT validation code example
- **Lines 173-228:** C++ gRPC JWT validation interceptor
- **Lines 249-302:** mTLS server/client configuration with code examples
- **Lines 411-467:** jwt-cpp library integration and usage
- **Lines 534-559:** Performance benchmarks (JWT: 1.6ms p50, mTLS: 0ms after handshake)
- **Lines 562-606:** 6-week implementation roadmap
- **Lines 609-660:** Security best practices and checklists

**3. README**
- **File:** `README.md`
- Quick start guide, architecture diagram, tech stack overview
- Brand identity: Forge Blue (#0A2540), Ember Orange (#FF6B35), Steel Gray (#556B7D)
- Feature highlights and roadmap (Q1-Q3 2026)

## Implementation Notes

### Requirement Numbering Scheme

Requirements are numbered sequentially across subsystems:
- **A. Authentication & Authorization:** Requirements 1-39
- **B. Upload & File Processing:** Requirements 40-62
- **C. Payment & Subscriptions:** Requirements 63-88
- **D. Notifications & Communication:** Requirements 89-109
- **E. Email & Transaction Management:** Requirements 110-128

**Total:** 128 functional requirements

### Data Model (14 Core Entities)

**Identity & Access:**
- User, Session, Role, ApiKey

**Content Management:**
- UploadObject, Quota

**Billing:**
- Subscription, PaymentMethod, Invoice, UsageRecord

**Notifications & Communication:**
- Notification, NotificationPreference, Webhook, EmailTemplate

### Multi-Tenancy Pattern

Every resource carries `tenantId` for strict isolation:
```sql
-- All tables follow this pattern
CREATE TABLE example (
    id UUID PRIMARY KEY,
    tenant_id UUID NOT NULL REFERENCES tenants(id),
    -- ... other columns
    CONSTRAINT tenant_isolation CHECK (tenant_id IS NOT NULL)
);

-- All queries MUST include tenant_id
SELECT * FROM example WHERE tenant_id = $1 AND id = $2;
```

**Authorization:** Enforce in middleware + gRPC interceptors. Never trust client-provided tenant_id.

### Compliance Requirements

**GDPR (Section 13.1):**
- Data portability: Export API for user data in JSON format
- Right to erasure: Anonymize (not delete) for audit trail preservation
- Retention: 90 days (hot), 1 year (cold) for operational events; 7 years for financial data

**PCI-DSS (NFR-S6):**
- No card data storage (tokenize via Stripe)
- Log only last 4 digits of card numbers
- Separate payment processing from general application logs

**WCAG 2.1 AA (NFR-C6):**
- Keyboard navigation for all interactive elements
- ARIA labels for screen readers
- Color contrast ratio ≥ 4.5:1 for normal text, ≥ 3:1 for large text

### Monitoring Metrics (Section 11.1)

**Critical Alerts:**
- `auth_refresh_token_reuse_detected_total` > 0 → Immediate investigation (potential token theft)
- `redis_up` == 0 → Critical: Token revocation disabled
- `cert_expiry_timestamp_seconds` - time() < 604800 (7 days) → Warning: Certificate expiring soon
- `payment_processing_errors_total` rate > 5% → Page on-call engineer

**Key Metrics:**
```
# Authentication
auth_jwt_validation_duration_seconds (p50, p95, p99)
auth_jwt_validation_errors_total{error_type="expired|invalid|revoked"}
auth_mtls_handshake_duration_seconds

# Upload
upload_presign_requests_total
upload_validation_failures_total{reason="virus|checksum|quota"}
upload_transform_duration_seconds{profile_id}

# Payment
payment_subscription_mrr_usd (Monthly Recurring Revenue)
payment_processing_duration_seconds
payment_fraud_detections_total

# Notifications
notification_delivery_rate{channel="email|sms|push|webhook"}
notification_delivery_latency_seconds
webhook_retry_exhausted_total
```

### API Error Response Format

```json
{
  "code": "AUTH_003",
  "message": "Invalid credentials",
  "correlationId": "550e8400-e29b-41d4-a716-446655440000",
  "details": {
    "field": "email",
    "reason": "Email not found"
  }
}
```

All error codes documented in SRS Section 9.

### Cost Optimization Notes (Section 20)

- **S3 Lifecycle:** Move to IA after 30 days, Glacier after 90 days
- **PostgreSQL:** Connection pooling (min 2, max 20 per service)
- **Redis:** Eviction policy `allkeys-lru`, max memory 2GB
- **Compute:** Autoscaling based on queue depth + CPU (50-90% target)

### Breaking Changes Ledger (Section 21)

When introducing breaking changes:
1. Document in CHANGELOG.md with migration guide
2. Provide 60-day deprecation notice for API changes
3. Support old + new versions for 1 release cycle
4. Emit deprecation warnings in logs 30 days before removal

## Key Decisions & Rationale

**Why C++ for core services?**
- Performance: JWT validation 1.6ms p50 (10x faster than Node.js)
- Type safety: Compile-time checks for critical auth logic
- mTLS: Native gRPC support with excellent performance

**Why Redis blacklist instead of database?**
- Latency: < 1ms vs 10-20ms for DB lookup
- Availability: Cached data survives DB failover
- TTL: Automatic cleanup when tokens expire

**Why RS256 instead of HS256 for JWT?**
- Asymmetric: Public key can be distributed to all services
- Key rotation: No need to update all services when rotating
- Security: Prevents algorithm confusion attacks (RFC 8725)

**Why saga pattern instead of 2PC?**
- Availability: No distributed locks or coordination
- Scalability: Each service commits independently
- Compensation: Explicit rollback logic for failure scenarios

## Common Development Patterns

### Adding a New gRPC Service

1. Define `.proto` file in `/proto/`
2. Generate C++ stubs: `protoc --cpp_out=. --grpc_out=. service.proto`
3. Implement service class with mTLS + JWT interceptors
4. Add to `docker-compose.yml` with port mapping
5. Update Kubernetes manifests in `/k8s/`
6. Add metrics and tracing instrumentation
7. Write unit tests with gRPC test fixtures

### Adding a New API Endpoint

1. Define Pydantic models for request/response validation
2. Add route handler in FastAPI with authentication dependency
3. Validate tenant_id from JWT context
4. Call gRPC service with JWT in metadata
5. Handle errors with standard error format (Section 9)
6. Document in OpenAPI (auto-generated at `/api/docs`)
7. Add acceptance criteria to SRS Section 12

### Implementing Idempotency

```python
@app.post("/v1/payments/charge")
async def charge_payment(
    request: ChargeRequest,
    idempotency_key: str = Header(..., alias="Idempotency-Key"),
    user: User = Depends(get_current_user)
):
    cache_key = f"idempotency:{user.tenant_id}:{user.id}:{idempotency_key}"

    # Check if already processed
    cached = await redis.get(cache_key)
    if cached:
        return JSONResponse(content=json.loads(cached), status_code=200)

    # Process payment
    result = await process_payment(request)

    # Cache result for 24 hours
    await redis.setex(cache_key, 86400, json.dumps(result))

    return result
```

### Handling Multi-Tenancy

```cpp
// gRPC interceptor to extract tenant context
grpc::Status ValidateTenantAccess(grpc::ServerContext* context,
                                   const std::string& requested_resource_id) {
    // Extract tenant_id from validated JWT
    auto tenant_id = context->client_metadata().find("x-tenant-id");

    // Load resource and verify tenant ownership
    auto resource = db_->LoadResource(requested_resource_id);
    if (resource.tenant_id != std::string(tenant_id->second.data())) {
        return grpc::Status(grpc::PERMISSION_DENIED,
                           "Resource not accessible by this tenant");
    }

    return grpc::Status::OK;
}
```

## Next Steps for Implementation

1. **Week 1-2:** Set up project structure (services/, api/, ui/)
2. **Week 3-4:** Implement JWT foundation (jwt-cpp integration, Redis blacklist)
3. **Week 5-6:** Implement mTLS for gRPC services
4. **Week 7:** OAuth 2.0/OIDC integration (Google, GitHub, Microsoft)
5. **Week 8:** Testing & hardening (unit, integration, E2E, security audit)



---

## AI Tool Usage Policy

### Philosophy

Code and commits are authored by **Heinstein F.** with AI assistance as a development tool - similar to using an IDE, compiler, linter, or Stack Overflow. The tool assists but does not claim authorship.

### In Code & Commits - INCLUDE

- **Human author attribution**: `@author Heinstein F.`
- **Technical commit messages** focused on "what" and "why"

### In Code & Commits - EXCLUDE

- **Marketing footers**: "Generated with [Claude Code]"
- **AI co-authorship**: "Co-Authored-By: Claude"
- **Tool branding**: 🤖 emoji or promotional links
- **"Created by AI"** or "Generated by AI" comments
- Any implication that AI is the author

### In Documentation - APPROPRIATE

- References to Claude Code as a tool (README.md, CLAUDE.md)
- Transparency about using AI-assisted development
- Technical documentation of AI features in the product


Refer to authentication architecture document (lines 562-606) for detailed roadmap.
- # add ssh key ssh-add ~/.ssh/id_rsa_git-work before