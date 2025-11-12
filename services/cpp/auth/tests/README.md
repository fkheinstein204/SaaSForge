# Auth Service Tests

## Test Types

### Unit Tests (`auth_service_test.cpp`)

Fast, isolated tests that use mocks for external dependencies. Run without requiring PostgreSQL or Redis.

**Run:**
```bash
cd services/cpp/build
make auth_service_test
./auth/auth_service_test
```

**Coverage:**
- Service initialization
- Token generation logic
- Password hashing
- Mock-based scenarios

### Integration Tests (`auth_integration_test.cpp`)

Full-stack tests using real PostgreSQL and Redis instances. Verify end-to-end functionality with actual database and cache.

**Run:**
```bash
# Automated (recommended)
./scripts/run-integration-tests.sh

# Manual
docker-compose -f docker-compose.test.yml up -d
cd services/cpp/build
make auth_integration_test
./auth/auth_integration_test
docker-compose -f docker-compose.test.yml down
```

**Coverage:**
- ✅ Login with valid credentials returns tokens
- ✅ Login with invalid email/password fails
- ✅ Login with deleted account fails
- ✅ Logout removes refresh token from Redis
- ✅ ValidateToken accepts valid JWT
- ✅ ValidateToken rejects invalid JWT
- ✅ CreateApiKey generates and stores keys
- ✅ RevokeApiKey soft-deletes keys
- ✅ Password hashing performance (<500ms)
- ✅ JWT validation performance (≤2ms p95, NFR-P1a)

## Test Environment

Integration tests use isolated test containers:
- **PostgreSQL**: `localhost:5433` (test_saasforge database)
- **Redis**: `localhost:6380`

Test data fixtures from `db/test_schema.sql`:
- Test Tenant 1: `00000000-0000-0000-0000-000000000001`
- Test User: `test@example.com` / `test_password_123`
- Pre-populated API key for revocation tests

## Performance Benchmarks

Integration tests include performance validation:

**Password Hashing (Argon2id):**
- Target: <500ms (OWASP recommendation: 200-500ms)
- Parameters: 64MB memory, 3 iterations, 4 threads

**JWT Validation (RS256):**
- Target: ≤2ms p95 (NFR-P1a)
- Includes signature verification + Redis blacklist check

## Adding New Tests

**Unit Test Pattern:**
```cpp
TEST_F(AuthServiceTest, YourTestName) {
    // Arrange - setup mocks and data
    // Act - call service method
    // Assert - verify results
    EXPECT_TRUE(condition);
}
```

**Integration Test Pattern:**
```cpp
TEST_F(AuthIntegrationTest, YourTestName) {
    // Arrange - use real DB/Redis connections from fixture
    // Act - call service method with real gRPC context
    // Assert - verify in database/Redis
    EXPECT_TRUE(condition);
}
```

## Continuous Integration

CI pipeline should run:
1. Unit tests (fast, always run)
2. Integration tests (slower, run on PR + merge)
3. Performance validation (ensure NFRs met)

**GitHub Actions Example:**
```yaml
- name: Run Integration Tests
  run: |
    ./scripts/run-integration-tests.sh
```

## Troubleshooting

**Test containers won't start:**
```bash
# Check port conflicts
lsof -i :5433  # PostgreSQL
lsof -i :6380  # Redis

# View logs
docker-compose -f docker-compose.test.yml logs
```

**Tests fail to connect:**
```bash
# Verify test containers are healthy
docker ps
docker exec saasforge_test_postgres pg_isready -U test
docker exec saasforge_test_redis redis-cli ping
```

**Cleanup stuck containers:**
```bash
docker-compose -f docker-compose.test.yml down -v
```

## Security Testing

Integration tests verify critical security requirements:
- ✅ **A-22**: JWT signature validation, algorithm whitelisting (RS256 only)
- ✅ **A-18**: Instant logout via refresh token deletion
- ✅ **S-2**: Argon2id password hashing (OWASP 2024)
- ✅ **S-1**: Soft delete pattern (deleted users cannot login)
- ✅ **S-8**: Tenant isolation (tests use tenant-scoped fixtures)

See `SECURITY_FIXES.md` for full security audit.
