# SaaSForge Review Agents

Seven specialized agents to review and validate code changes against project requirements.

## 🔐 1. Security Review Agent
**Focus:** JWT, mTLS, multi-tenancy isolation, OWASP compliance

**Run:** `python .agents/security/review.py <path>`

**Checks:**
- JWT algorithm whitelisting (RS256 only)
- Token blacklist verification
- Tenant isolation in queries
- SSRF protection in webhooks
- Input validation and sanitization
- Secrets in code

## 🏗️ 2. Architecture Compliance Agent
**Focus:** SRS requirements adherence, design patterns

**Run:** `python .agents/architecture/review.py <path>`

**Checks:**
- Requirement traceability (A-1 through E-128)
- gRPC service contracts
- Multi-tenancy patterns
- Error code standards
- Idempotency key usage

## ⚡ 3. Performance Review Agent
**Focus:** NFR targets (JWT < 2ms, login < 500ms, etc.)

**Run:** `python .agents/performance/review.py <path>`

**Checks:**
- Database query optimization
- N+1 query detection
- Connection pooling
- Caching strategies
- Rate limiting implementation

## 📋 4. API Contract Agent
**Focus:** Proto/OpenAPI compliance, breaking changes

**Run:** `python .agents/api-contract/review.py <proto-file>`

**Checks:**
- Proto backward compatibility
- REST API versioning
- Required field changes
- Response format consistency
- Error response structure

## 🗄️ 5. Database Schema Agent
**Focus:** Migrations, indexes, multi-tenancy

**Run:** `python .agents/database/review.py <migration-file>`

**Checks:**
- tenant_id presence in all tables
- Missing indexes
- Migration reversibility
- Foreign key constraints
- Data retention compliance

## 🧪 6. Testing Coverage Agent
**Focus:** Critical paths, security tests, edge cases

**Run:** `python .agents/testing/review.py <test-path>`

**Checks:**
- Authentication flow coverage
- Multi-tenancy test isolation
- Error handling tests
- Edge case coverage
- Integration test completeness

## 🏗️ 7. Software Architect Agent
**Focus:** Three-tier architecture, layer separation, scalability patterns

**Run:** `python .agents/architect/review.py <path>`

**Checks:**
- UI → BFF → Service boundaries
- Business logic placement
- Service statelessness
- Communication patterns (sync vs async)
- Resilience patterns (retry, circuit breaker)
- Observability (logging, metrics, tracing)

## Usage

### Manual Review
```bash
# Run all agents on a file
python .agents/run_all.py path/to/file.py

# Run specific agent
python .agents/security/review.py services/cpp/auth/
```

### CI Integration
Agents automatically run on PRs via `.github/workflows/agents.yml`

### Configuration
Edit `.agents/config.yaml` to customize checks and thresholds.
