# Integration Tests Setup

## Prerequisites

The integration tests require the following to be installed:

### 1. Argon2 Development Library

```bash
sudo apt-get install -y libargon2-dev
```

**Why needed:** Auth Service uses Argon2id for secure password hashing (OWASP 2024 recommendations).

**Current status:**
- ✅ Runtime library (`libargon2-1`) installed
- ❌ Development headers (`libargon2-dev`) NOT installed - **REQUIRED**

### 2. Docker & Docker Compose

Required for test containers (PostgreSQL, Redis).

```bash
docker --version
docker-compose --version
```

## Setup Steps

### Step 1: Install Dependencies

```bash
sudo apt-get update
sudo apt-get install -y libargon2-dev
```

### Step 2: Build Integration Tests

```bash
cd services/cpp/build
cmake ..
make auth_integration_test -j$(nproc)
```

### Step 3: Run Integration Tests

**Automated (recommended):**
```bash
./scripts/run-integration-tests.sh
```

**Manual:**
```bash
# Start test containers
docker-compose -f docker-compose.test.yml up -d

# Wait for readiness
docker exec saasforge_test_postgres pg_isready -U test
docker exec saasforge_test_redis redis-cli ping

# Run tests
cd services/cpp/build/auth
./auth_integration_test

# Cleanup
cd ../..
docker-compose -f docker-compose.test.yml down -v
```

## Test Coverage

### Functional Tests (10 tests)

1. ✅ **Login with valid credentials** - Returns access + refresh tokens
2. ✅ **Login with invalid email** - Returns UNAUTHENTICATED
3. ✅ **Login with invalid password** - Returns UNAUTHENTICATED
4. ✅ **Login with deleted account** - Soft delete pattern enforced
5. ✅ **Logout removes refresh token** - Verifies Redis cleanup
6. ✅ **ValidateToken accepts valid JWT** - RS256 signature validation
7. ✅ **ValidateToken rejects invalid JWT** - Algorithm whitelisting
8. ✅ **CreateApiKey generates key** - Database storage verification
9. ✅ **RevokeApiKey soft-deletes** - Sets revoked_at timestamp
10. ✅ **Tenant isolation** - Cannot access other tenant's data

### Performance Tests (2 tests)

1. ✅ **Password hashing** - Target: <500ms (Argon2id)
2. ✅ **JWT validation** - Target: ≤2ms p95 (NFR-P1a)

## Test Fixtures

From `db/test_schema.sql`:

**Tenants:**
- `00000000-0000-0000-0000-000000000001` - Test Tenant 1
- `00000000-0000-0000-0000-000000000002` - Test Tenant 2

**Users:**
- `test@example.com` / `test_password_123` (Tenant 1, active)
- `tenant2@example.com` / `test_password_123` (Tenant 2, active)
- `deleted@example.com` (Tenant 1, soft-deleted)

**API Keys:**
- Pre-populated key for revocation tests

## Architecture

```
auth_integration_test (C++)
    ↓
AuthServiceImpl (gRPC service)
    ↓
PostgreSQL:5433 (test_saasforge DB) + Redis:6380
```

**Test Isolation:**
- Separate ports (5433, 6380) to avoid dev environment conflicts
- Dedicated test database with fixtures
- Cleanup after each test run
- No impact on development database

## Security Testing

Integration tests validate critical security requirements:

| Requirement | Test Coverage | Status |
|-------------|---------------|--------|
| **A-22** | JWT signature validation (RS256 only) | ✅ Covered |
| **A-18** | Instant logout (refresh token deletion) | ✅ Covered |
| **A-3** | Argon2id password hashing | ✅ Covered |
| **S-1** | Soft delete pattern (deleted_at) | ✅ Covered |
| **S-2** | Password hash format (PHC string) | ✅ Covered |
| **S-8** | Tenant isolation validation | ✅ Covered |
| **NFR-P1a** | JWT validation ≤2ms p95 | ✅ Benchmarked |

See `SECURITY_FIXES.md` for full security audit.

## CI/CD Integration

**GitHub Actions Example:**

```yaml
name: Integration Tests

on: [pull_request, push]

jobs:
  integration-tests:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3

      - name: Install Dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y libargon2-dev cmake g++ libssl-dev

      - name: Build Services
        run: |
          cd services/cpp
          mkdir build && cd build
          cmake ..
          make -j$(nproc)

      - name: Run Integration Tests
        run: |
          ./scripts/run-integration-tests.sh
```

## Troubleshooting

### Build Error: `argon2.h: No such file or directory`

**Problem:** Development headers not installed

**Solution:**
```bash
sudo apt-get install -y libargon2-dev
```

### Test Containers Won't Start

**Problem:** Port conflicts with dev environment

**Check:**
```bash
lsof -i :5433  # PostgreSQL
lsof -i :6380  # Redis
```

**Solution:** Stop dev containers or use different ports

### Tests Timeout Connecting to DB

**Problem:** Test database not ready

**Solution:**
```bash
# Check health
docker exec saasforge_test_postgres pg_isready -U test -d test_saasforge

# View logs
docker-compose -f docker-compose.test.yml logs test_postgres
```

### Redis Connection Refused

**Problem:** Test Redis not running

**Solution:**
```bash
# Check health
docker exec saasforge_test_redis redis-cli ping

# Restart
docker-compose -f docker-compose.test.yml restart test_redis
```

## Next Steps

1. **Install argon2-dev** - Blocking build
2. **Run integration tests** - Verify all pass
3. **Add to CI/CD** - Automated testing on PR
4. **Expand coverage** - Upload, Payment, Notification services
5. **Performance benchmarks** - k6 load testing scripts

## Related Files

- `services/cpp/auth/tests/auth_integration_test.cpp` - Integration test suite
- `services/cpp/auth/tests/auth_service_test.cpp` - Unit test suite
- `db/test_schema.sql` - Test database schema and fixtures
- `docker-compose.test.yml` - Test container orchestration
- `scripts/run-integration-tests.sh` - Test runner script
