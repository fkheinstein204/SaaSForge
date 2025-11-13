# SaaSForge - Quick Start Guide

Get SaaSForge up and running locally in minutes.

## Prerequisites

### System Requirements
- **OS**: Ubuntu 20.04+ / Debian 11+ (or similar Linux distribution)
- **RAM**: 4GB minimum, 8GB recommended
- **Disk**: 10GB free space

### Required Software

```bash
# Update package lists
sudo apt-get update

# Install core dependencies
sudo apt-get install -y \
    build-essential \
    cmake \
    git \
    curl \
    pkg-config \
    libssl-dev \
    libpq-dev \
    libpqxx-dev \
    libargon2-dev \
    libhiredis-dev \
    libre2-dev \
    protobuf-compiler \
    libprotobuf-dev \
    grpc++ \
    libgrpc++-dev \
    protobuf-compiler-grpc \
    libgtest-dev \
    libgmock-dev

# Install Docker
curl -fsSL https://get.docker.com -o get-docker.sh
sudo sh get-docker.sh
sudo usermod -aG docker $USER

# Install Docker Compose
sudo apt-get install -y docker-compose

# Install Node.js (for UI)
curl -fsSL https://deb.nodesource.com/setup_20.x | sudo -E bash -
sudo apt-get install -y nodejs

# Install Python (for API gateway)
sudo apt-get install -y python3 python3-pip python3-venv
```

**Important**: After installing Docker, log out and log back in for group permissions to take effect.

---

## Quick Start (5 Minutes)

### 1. Clone the Repository

```bash
git clone <repository-url>
cd SaaSForge
```

### 2. Install C++ Dependencies

```bash
./scripts/install-cpp-deps.sh
```

This installs:
- jwt-cpp (header-only JWT library)
- redis-plus-plus (Redis C++ client)

### 3. Generate Protobuf/gRPC Code

```bash
cd /path/to/SaaSForge
mkdir -p services/cpp/generated
protoc \
  --proto_path=proto \
  --cpp_out=services/cpp/generated \
  --grpc_out=services/cpp/generated \
  --plugin=protoc-gen-grpc=$(which grpc_cpp_plugin) \
  proto/*.proto
```

### 4. Build C++ Services

```bash
cd services/cpp
rm -rf build
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

**Built binaries**:
- `auth/auth_service` - Authentication & Authorization
- `upload/upload_service` - File Upload Management
- `payment/payment_service` - Subscription & Payment Processing
- `notification/notification_service` - Email/Webhook Notifications
- `auth/auth_integration_test` - Integration tests

### 5. Install Python API Dependencies

```bash
cd api
python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt
```

### 6. Install UI Dependencies

```bash
cd ui
npm install
```

---

## Running the Application

### Option A: Docker Compose (Recommended)

**Start all services**:

```bash
cd /path/to/SaaSForge
docker-compose up -d
```

This starts:
- PostgreSQL (port 5432)
- Redis (port 6379)
- Auth Service (gRPC port 50051)
- Upload Service (gRPC port 50052)
- Payment Service (gRPC port 50053)
- Notification Service (gRPC port 50054)
- API Gateway (HTTP port 8000)
- UI (HTTP port 5173)

**View logs**:
```bash
docker-compose logs -f
```

**Stop services**:
```bash
docker-compose down
```

**Clean everything (including volumes)**:
```bash
docker-compose down -v
```

### Option B: Manual Local Development

**1. Start infrastructure services**:

```bash
docker-compose up -d postgres redis
```

**2. Run database migrations**:

```bash
cd db
psql -h localhost -U saasforge -d saasforge -f schema.sql
# Password: saasforge_dev_password
```

**3. Start C++ microservices**:

```bash
# Terminal 1 - Auth Service
cd services/cpp/build
./auth/auth_service

# Terminal 2 - Upload Service
./upload/upload_service

# Terminal 3 - Payment Service
./payment/payment_service

# Terminal 4 - Notification Service
./notification/notification_service
```

**4. Start API Gateway**:

```bash
# Terminal 5
cd api
source venv/bin/activate
uvicorn main:app --reload --port 8000
```

**5. Start UI**:

```bash
# Terminal 6
cd ui
npm run dev
```

---

## Implemented Features ✨

### Authentication Service (Auth Service)

**Complete 2FA/TOTP Implementation:**
- ✅ TOTP secret generation (RFC 6238 compliant)
- ✅ QR code provisioning for authenticator apps
- ✅ Backup code generation (10 codes per enrollment)
- ✅ Login with TOTP or backup code validation
- ✅ 30-second time window validation (±1 window tolerance)

**OAuth 2.0/OIDC Infrastructure:**
- ✅ Google, GitHub, Microsoft provider support (mock implementation)
- ✅ CSRF protection with state parameter validation
- ✅ Account linking via verified email
- ✅ New user creation on first OAuth login
- ✅ Token exchange and session management

**API Key Management:**
- ✅ API key generation with Argon2id hashing
- ✅ Scope validation with wildcard support (`read:*`, `write:*`)
- ✅ Deny-by-default security model
- ✅ Expiry and revocation support

**Rate Limiting:**
- ✅ Redis-backed rate limiting
- ✅ Failed login lockout (20 attempts → 15-min lock)
- ✅ OTP rate limiting (3 requests per minute)
- ✅ Configurable limits per endpoint

**Security Features:**
- ✅ RS256 JWT signing (4096-bit RSA keys)
- ✅ Token blacklisting (instant logout)
- ✅ Refresh token rotation (prevents reuse attacks)
- ✅ Refresh token reuse detection (security alert)
- ✅ Password hashing with Argon2id (OWASP 2024 recommended)

### Payment Service (Payment Service)

**Subscription Management:**
- ✅ Subscription creation with trial support (7/14/30 days)
- ✅ State machine: TRIALING → ACTIVE → PAST_DUE → UNPAID → CANCELED
- ✅ Plan updates with MRR calculation
- ✅ Cancel immediately or at period end
- ✅ Three pricing tiers: Free ($0), Pro ($29), Enterprise ($99)

**Payment Retry Logic:**
- ✅ Fixed retry schedule (Day 1, Day 3, Day 7)
- ✅ Automatic state transitions on payment failure
- ✅ Email notifications before each retry
- ✅ Transition to UNPAID after 3 failures

**Mock Stripe Integration:**
- ✅ Customer management
- ✅ Payment method tokenization
- ✅ Invoice generation with line items
- ✅ Payment processing (80% success rate for testing)
- ✅ Realistic Stripe ID generation

### Shared Utilities (Common Library)

**TOTP Helper:**
- 270 lines of RFC 6238-compliant TOTP implementation
- HMAC-SHA1 computation with OpenSSL
- Base32 encoding/decoding
- Backup code generation with SHA-256 hashing
- Cryptographically secure randomness (RAND_bytes)

**Mock Stripe Client:**
- 342 lines of payment infrastructure
- In-memory subscription state management
- Payment retry scheduling
- Invoice lifecycle management
- Customer and payment method operations

**Other Utilities:**
- JWT validation with Redis blacklist
- Password hashing with Argon2id
- Database connection pooling
- Tenant context isolation
- mTLS credentials management

---

## Access the Application

| Service | URL | Description |
|---------|-----|-------------|
| **UI** | http://localhost:5173 | React frontend |
| **API Gateway** | http://localhost:8000 | REST API |
| **API Docs** | http://localhost:8000/docs | Swagger UI |
| **PostgreSQL** | localhost:5432 | Database |
| **Redis** | localhost:6379 | Cache/Sessions |

### Default Credentials

Database:
- **User**: `saasforge`
- **Password**: `saasforge_dev_password`
- **Database**: `saasforge`

---

## Running Tests

### C++ Unit Tests (Fast) ✅

**All unit tests** (recommended):
```bash
cd services/cpp/build

# Run all unit tests
./common/totp_helper_test           # TOTP/2FA tests (22 tests)
./common/mock_stripe_client_test    # Payment system tests (31 tests)
./auth/api_key_scope_test           # API key scope validation tests (34 tests)
./common/common_test                # Core utilities tests
```

**Individual test suites**:

**1. TOTP Helper Tests** (2FA & Authentication):
```bash
cd services/cpp/build
./common/totp_helper_test

# Test specific cases
./common/totp_helper_test --gtest_filter=TotpHelperTest.GenerateSecretProduces*

# Enable performance benchmarks (disabled by default)
./common/totp_helper_test --gtest_also_run_disabled_tests
```

Tests covered:
- ✅ TOTP secret generation (Base32, 160-bit entropy)
- ✅ QR code URL generation (otpauth:// format)
- ✅ TOTP code validation (30-second time windows)
- ✅ Backup code generation and verification
- ✅ SHA-256 hashing security

**2. Mock Stripe Client Tests** (Payment Processing):
```bash
cd services/cpp/build
./common/mock_stripe_client_test

# Test specific functionality
./common/mock_stripe_client_test --gtest_filter=*Subscription*
./common/mock_stripe_client_test --gtest_filter=*PaymentRetry*
```

Tests covered:
- ✅ Customer management
- ✅ Subscription lifecycle (TRIALING → ACTIVE → PAST_DUE → UNPAID)
- ✅ Payment retry logic (Day 1, 3, 7)
- ✅ Invoice operations
- ✅ Payment methods (cards, tokens)

**3. API Key Scope Validation Tests** (Authorization & Security):
```bash
cd services/cpp/build
./auth/api_key_scope_test

# Test specific functionality
./auth/api_key_scope_test --gtest_filter=*Wildcard*
./auth/api_key_scope_test --gtest_filter=*Security*
```

Tests covered:
- ✅ Exact scope matching
- ✅ Wildcard scope matching (e.g., "read:*" matches "read:upload")
- ✅ Deny-by-default security (Requirement A-14)
- ✅ Privilege escalation prevention
- ✅ Cross-tenant scope denial
- ✅ Real-world scenarios (read-only, service accounts, integrations)

**Test Results Summary:**
```
Total Tests: 87
✅ PASSED: 87 (100%)
❌ FAILED: 0
⏸️  DISABLED: 3 (performance benchmarks)
Execution Time: < 5ms
```

### C++ Integration Tests (Requires Test DB)

```bash
# Start test containers
docker-compose -f docker-compose.test.yml up -d

# Wait for services
sleep 5

# Run integration tests
cd services/cpp/build
./auth/auth_integration_test

# Cleanup
docker-compose -f docker-compose.test.yml down -v
```

### Python API Tests

```bash
cd api
source venv/bin/activate
pytest -v
```

### End-to-End Tests

```bash
cd tests/e2e
npm install
npx playwright test
```

---

## Development Workflow

### 1. Make Changes to Proto Files

```bash
# Edit proto/*.proto files
nano proto/auth.proto

# Regenerate code
protoc \
  --proto_path=proto \
  --cpp_out=services/cpp/generated \
  --grpc_out=services/cpp/generated \
  --plugin=protoc-gen-grpc=$(which grpc_cpp_plugin) \
  proto/*.proto
```

### 2. Rebuild C++ Services

```bash
cd services/cpp/build
make -j$(nproc)
```

### 3. Restart Services

**Docker Compose**:
```bash
docker-compose restart auth_service
```

**Manual**:
```bash
# Stop the service (Ctrl+C in its terminal)
# Start it again
./auth/auth_service
```

### 4. Hot Reload (Python API & UI)

The API Gateway and UI support hot reload automatically:
- **API**: Changes to `api/` files trigger automatic reload
- **UI**: Changes to `ui/src/` files trigger automatic rebuild

---

## Common Tasks

### View Database

```bash
docker exec -it saasforge_postgres psql -U saasforge -d saasforge

# Inside psql
\dt              # List tables
\d users         # Describe users table
SELECT * FROM users LIMIT 10;
```

### View Redis Data

```bash
docker exec -it saasforge_redis redis-cli

# Inside redis-cli
KEYS *           # List all keys
GET session:abc  # Get session data
```

### Check Service Health

```bash
# API Gateway
curl http://localhost:8000/health

# Check if gRPC services are running
docker-compose ps
```

### Rebuild Everything

```bash
# Clean build
cd services/cpp
rm -rf build
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# Rebuild containers
docker-compose build
docker-compose up -d
```

---

## Troubleshooting

### Port Already in Use

```bash
# Find process using port
sudo lsof -i :5432
sudo lsof -i :8000

# Kill process
sudo kill -9 <PID>
```

### Docker Build Fails

```bash
# Clean Docker cache
docker system prune -a --volumes

# Rebuild from scratch
docker-compose build --no-cache
```

### CMake Configuration Fails

**Missing jwt-cpp**:
```bash
./scripts/install-cpp-deps.sh
```

**Missing redis++**:
```bash
sudo apt-get install -y libhiredis-dev
./scripts/install-cpp-deps.sh
```

**Missing protobuf/gRPC**:
```bash
sudo apt-get install -y \
    protobuf-compiler \
    libprotobuf-dev \
    libgrpc++-dev \
    protobuf-compiler-grpc
```

### Database Connection Fails

```bash
# Check if PostgreSQL is running
docker-compose ps postgres

# View logs
docker-compose logs postgres

# Restart PostgreSQL
docker-compose restart postgres
```

### C++ Service Crashes

```bash
# Run with debugging
cd services/cpp/build
gdb ./auth/auth_service
(gdb) run
# When it crashes:
(gdb) backtrace
```

---

## Project Structure

```
SaaSForge/
├── api/                    # Python FastAPI gateway
│   ├── main.py            # API entry point
│   ├── routers/           # REST endpoints
│   └── clients/           # gRPC client wrappers
├── services/cpp/          # C++ microservices
│   ├── auth/              # Authentication service
│   ├── upload/            # File upload service
│   ├── payment/           # Payment service
│   ├── notification/      # Notification service
│   └── common/            # Shared utilities
├── ui/                    # React frontend
│   └── src/               # UI source code
├── proto/                 # gRPC protocol definitions
├── db/                    # Database schemas
├── docker/                # Dockerfiles
├── k8s/                   # Kubernetes manifests
└── scripts/               # Utility scripts
```

---

## Usage Examples

### Example 1: Enroll in 2FA

**gRPC Call (C++):**
```cpp
#include "auth.grpc.pb.h"
#include "common/totp_helper.h"

// Client-side: Generate secret
std::string secret = saasforge::common::TotpHelper::GenerateSecret();
std::string qr_url = saasforge::common::TotpHelper::GenerateQrCodeUrl(
    secret,
    "user@example.com",
    "SaaSForge"
);

// Show QR code to user
DisplayQRCode(qr_url);

// Server-side: Store secret in database (encrypted in production)
// User scans QR code and enters verification code

// Verify TOTP code
bool valid = saasforge::common::TotpHelper::ValidateCode(secret, "123456");
```

**Expected QR URL:**
```
otpauth://totp/SaaSForge:user@example.com?secret=JBSWY3DPEHPK3PXP&issuer=SaaSForge&algorithm=SHA1&digits=6&period=30
```

### Example 2: Create Subscription with Trial

**C++ (MockStripeClient):**
```cpp
#include "common/mock_stripe_client.h"

saasforge::common::MockStripeClient stripe("sk_test_key");

// Create customer
auto customer = stripe.CreateCustomer("user@example.com", "tenant_123");

// Create subscription with 14-day trial
auto subscription = stripe.CreateSubscription(
    customer.id,
    "pro",      // plan_id
    14          // trial_days
);

// subscription.status == SubscriptionStatus::TRIALING
// subscription.amount == 29.0
// subscription.trial_end == current_time + 14 days
```

### Example 3: Handle Payment Failure Retry

**C++ (MockStripeClient):**
```cpp
// Payment fails
stripe.RecordPaymentFailure(subscription_id);

// Check if should retry
if (stripe.ShouldRetryPayment(subscription_id)) {
    stripe.SchedulePaymentRetry(subscription_id, retry_count);
}

// After 3 failures
auto sub = stripe.GetSubscription(subscription_id);
// sub->status == SubscriptionStatus::UNPAID
// sub->retry_count == 3
```

### Example 4: Validate API Key with Scopes

**gRPC Call (Auth Service):**
```protobuf
message ValidateApiKeyRequest {
  api_key: "sk_live_1234567890abcdef"
  requested_scope: "read:upload"
}

// Response
message ValidateApiKeyResponse {
  valid: true
  user_id: "usr_abc123"
  tenant_id: "tenant_xyz"
  scopes: ["read:*", "write:upload"]
  message: "API key valid"
}
```

**Scope Matching Logic:**
- `read:upload` matches `read:*` (wildcard) ✅
- `read:upload` matches `read:upload` (exact) ✅
- `read:upload` DOES NOT match `write:*` ❌
- Unknown scope → **DENIED** (deny-by-default)

### Example 5: Generate and Verify Backup Codes

**C++ (TOTP Helper):**
```cpp
#include "common/totp_helper.h"

// Generate 10 backup codes
auto codes = saasforge::common::TotpHelper::GenerateBackupCodes(10);
// codes = ["1234-5678", "9012-3456", ...]

// Hash for storage (SHA-256)
for (const auto& code : codes) {
    std::string hash = saasforge::common::TotpHelper::HashBackupCode(code);
    // Store hash in database
    db->execute("INSERT INTO backup_codes (user_id, code_hash) VALUES (?, ?)",
                user_id, hash);
}

// Verify backup code during login
bool valid = saasforge::common::TotpHelper::VerifyBackupCode(
    user_input_code,
    stored_hash
);

if (valid) {
    // Mark code as used (one-time use only)
    db->execute("UPDATE backup_codes SET used_at = NOW() WHERE code_hash = ?",
                stored_hash);
}
```

---

## Test Coverage Report 📊

For detailed test results and coverage analysis, see:
- **[Test Results](docs/TEST_RESULTS_2025-11-13.md)** - Comprehensive test report
- **[Implementation Session](docs/IMPLEMENTATION_SESSION_2025-11-13.md)** - Development notes

**Quick Stats:**
- **Total Tests:** 53
- **Pass Rate:** 100% ✅
- **Coverage:** 85-90% on tested components
- **Execution Time:** < 5ms
- **Test-to-Code Ratio:** 1.67:1

---

## Next Steps

1. **Read the Documentation**:
   - [SRS Documentation](docs/srs-boilerplate-saas.md) - 128 functional requirements
   - [Authentication Guide](docs/srs-boilerplate-recommendation-implementation.md) - JWT + mTLS + OAuth
   - [Implementation Session](docs/IMPLEMENTATION_SESSION_2025-11-13.md) - Recent changes
   - [Test Results](docs/TEST_RESULTS_2025-11-13.md) - Unit test coverage
   - [CI/CD Setup](README_CICD.md)

2. **Explore the API**:
   - Visit http://localhost:8000/docs
   - Try the 2FA enrollment flow
   - Test subscription creation with trial
   - Experiment with API key scopes

3. **Run the Tests**:
   ```bash
   cd services/cpp/build
   ./common/totp_helper_test
   ./common/mock_stripe_client_test
   ```

4. **Customize**:
   - Add new proto definitions
   - Implement custom business logic
   - Modify UI components
   - Add new unit tests

5. **Deploy**:
   - Review [k8s/](k8s/) manifests
   - Set up production environment variables
   - Configure monitoring and logging
   - Replace mock implementations with real APIs

---

## Need Help?

- **Documentation**: See `docs/` directory
- **Issues**: Check existing issues or create a new one
- **Contributing**: See [CONTRIBUTING.md](CONTRIBUTING.md)

---

**Happy Coding! 🚀**
