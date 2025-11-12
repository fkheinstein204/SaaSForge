# Load Testing with k6

Comprehensive load tests for SaaSForge services using [k6](https://k6.io/).

## Prerequisites

**Install k6:**
```bash
# macOS
brew install k6

# Ubuntu/Debian
sudo gpg -k
sudo gpg --no-default-keyring --keyring /usr/share/keyrings/k6-archive-keyring.gpg --keyserver hkp://keyserver.ubuntu.com:80 --recv-keys C5AD17C747E3415A3642D57D77C6C491D6AC1D69
echo "deb [signed-by=/usr/share/keyrings/k6-archive-keyring.gpg] https://dl.k6.io/deb stable main" | sudo tee /etc/apt/sources.list.d/k6.list
sudo apt-get update
sudo apt-get install k6

# Docker
docker pull grafana/k6:latest
```

## Test Suites

### 1. Auth Service Load Test (`auth_load_test.js`)

**Scenarios:**
- **Ramp-up Test**: Gradual load increase (50 → 100 users over 16 minutes)
- **Spike Test**: 1000 concurrent logins for 1 minute (NFR-A4)
- **Soak Test**: Sustained 50 users for 30 minutes

**Endpoints Tested:**
- `POST /v1/auth/login` - NFR-P1: ≤500ms p95
- `GET /v1/auth/validate` - NFR-P1a: ≤2ms p95
- `POST /v1/auth/api-keys` - API key generation
- `POST /v1/auth/logout` - Refresh token cleanup

**Key Metrics:**
- Login success rate (>99%)
- JWT validation latency (p95, p99)
- Login duration trend
- Logout success rate

**Run:**
```bash
# Full test (47 minutes)
k6 run tests/load/auth_load_test.js

# Quick smoke test
k6 run --duration 1m --vus 10 tests/load/auth_load_test.js

# With custom API URL
k6 run --env API_URL=https://staging.saasforge.com tests/load/auth_load_test.js
```

### 2. Upload Service Load Test (`upload_load_test.js`)

**Scenarios:**
- **Mixed Load**: 20 users uploading various file sizes (1KB-10MB)
- **Large File Test**: 5 users uploading 100MB chunks (simulating 5GB NFR-B4)

**Endpoints Tested:**
- `POST /v1/upload/presign` - NFR-P2: ≤300ms p99
- `POST /v1/upload/complete` - ETag verification
- `GET /v1/upload/quota` - Quota enforcement (NFR-B7)

**Key Metrics:**
- Presign URL generation latency (p50, p95, p99)
- Upload success rate
- Quota enforcement counter
- Complete upload duration

**Run:**
```bash
# Full test (13 minutes)
k6 run tests/load/upload_load_test.js

# Large file focus
k6 run --scenario large_file_test tests/load/upload_load_test.js
```

### 3. Payment Service Load Test (`payment_load_test.js`)

**Scenarios:**
- **Subscription Lifecycle**: 10 users creating/updating/canceling subscriptions
- **Usage Metering**: 1000 requests/second for 2 minutes (NFR-C6: 10k req/s target)

**Endpoints Tested:**
- `POST /v1/payment/payment-methods` - Add payment method
- `POST /v1/payment/subscriptions` - NFR-P4: ≤3s p95
- `GET /v1/payment/subscriptions/{id}` - Subscription details
- `POST /v1/payment/subscriptions/{id}/cancel` - Cancel subscription
- `POST /v1/payment/usage` - Usage recording

**Key Metrics:**
- Payment processing duration (p95 target: 3s)
- Idempotency enforcement rate (Requirement E-124)
- Subscription creation success rate
- Usage metering throughput

**Run:**
```bash
# Full test (7 minutes)
k6 run tests/load/payment_load_test.js

# High-throughput metering only
k6 run --scenario usage_metering tests/load/payment_load_test.js
```

## Running All Tests

```bash
# Create results directory
mkdir -p tests/load/results

# Run all tests sequentially
for test in auth upload payment; do
  echo "Running ${test} load test..."
  k6 run tests/load/${test}_load_test.js | tee tests/load/results/${test}_output.txt
done
```

## Results Analysis

**JSON Summaries:**
- `tests/load/results/auth_load_test_summary.json`
- `tests/load/results/upload_load_test_summary.json`
- `tests/load/results/payment_load_test_summary.json`

**Parse results:**
```bash
# Extract p95 latencies
jq '.metrics.http_req_duration.values["p(95)"]' tests/load/results/auth_load_test_summary.json

# Check thresholds
jq '.thresholds | to_entries[] | select(.value.ok == false)' tests/load/results/*.json
```

## NFR Validation

| NFR | Requirement | Test | Target | Validation |
|-----|-------------|------|--------|------------|
| **NFR-P1** | Login latency | auth_load_test | ≤500ms p95 | `http_req_duration{endpoint:login}` |
| **NFR-P1a** | JWT validation | auth_load_test | ≤2ms p95 | `http_req_duration{endpoint:validate}` |
| **NFR-P2** | Presign URL | upload_load_test | ≤300ms p99 | `http_req_duration{endpoint:presign}` |
| **NFR-P4** | Payment processing | payment_load_test | ≤3s p95 | `payment_processing_duration` |
| **NFR-A4** | Concurrent logins | auth_load_test | 1000/min | Spike scenario |
| **NFR-B4** | Large file upload | upload_load_test | 5GB | Large file scenario |
| **NFR-C6** | Usage metering | payment_load_test | 10k req/s | Usage scenario |

## CI/CD Integration

**GitHub Actions Example:**
```yaml
name: Load Tests

on:
  schedule:
    - cron: '0 2 * * *'  # Nightly at 2 AM
  workflow_dispatch:

jobs:
  load-tests:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3

      - name: Install k6
        run: |
          sudo gpg -k
          sudo gpg --no-default-keyring --keyring /usr/share/keyrings/k6-archive-keyring.gpg --keyserver hkp://keyserver.ubuntu.com:80 --recv-keys C5AD17C747E3415A3642D57D77C6C491D6AC1D69
          echo "deb [signed-by=/usr/share/keyrings/k6-archive-keyring.gpg] https://dl.k6.io/deb stable main" | sudo tee /etc/apt/sources.list.d/k6.list
          sudo apt-get update
          sudo apt-get install k6

      - name: Run Auth Load Test
        run: k6 run --duration 5m tests/load/auth_load_test.js
        env:
          API_URL: ${{ secrets.STAGING_API_URL }}

      - name: Upload Results
        uses: actions/upload-artifact@v3
        with:
          name: load-test-results
          path: tests/load/results/
```

## Best Practices

### 1. Baseline Testing
Run tests against local/staging before production:
```bash
k6 run --env API_URL=http://localhost:8000 tests/load/auth_load_test.js
```

### 2. Gradual Rollout
Start with small load, increase incrementally:
```bash
# 10 users
k6 run --vus 10 --duration 1m tests/load/auth_load_test.js

# 50 users
k6 run --vus 50 --duration 5m tests/load/auth_load_test.js

# 100 users
k6 run --vus 100 --duration 10m tests/load/auth_load_test.js
```

### 3. Monitor System Resources
During load tests, monitor:
- CPU utilization (target: <80% peak)
- Memory usage (check for leaks)
- Database connections (pool saturation)
- Redis memory (maxmemory policy)
- Network I/O (bandwidth limits)
- Disk I/O (database write throughput)

### 4. Results Interpretation
- **p50 (median)**: Typical user experience
- **p95**: What 95% of users experience
- **p99**: Tail latency (worst case minus outliers)
- **max**: Absolute worst case (may include network issues)

### 5. Test Data Isolation
Use dedicated test users to avoid production data pollution:
```javascript
const TEST_USER = {
  email: 'loadtest@example.com',
  password: 'LoadTest123!',
};
```

## Troubleshooting

### High Error Rates
- Check service logs for exceptions
- Verify database connection pool size
- Check Redis connection limits
- Monitor network timeouts

### Slow Response Times
- Database query optimization (EXPLAIN ANALYZE)
- Add missing indexes
- Increase connection pool size
- Enable query caching

### Test Timeout
- Reduce VUs or duration
- Use `--no-connection-reuse` flag
- Increase test machine resources
- Split into smaller scenarios

### Quota Exceeded Errors (Expected)
Upload and payment tests intentionally hit quota limits to verify enforcement (Requirement B-45).

## Advanced Usage

### Custom Thresholds
```bash
k6 run --threshold http_req_duration=p(95)<100 tests/load/auth_load_test.js
```

### Distributed Load Testing
```bash
# Run from multiple machines
k6 run --out cloud tests/load/auth_load_test.js
```

### Real-time Metrics
```bash
# InfluxDB export
k6 run --out influxdb=http://localhost:8086/k6 tests/load/auth_load_test.js

# Grafana visualization
docker run -d -p 3000:3000 grafana/grafana
```

## Related Documentation

- [k6 Official Docs](https://k6.io/docs/)
- [SRS NFR Section](../../docs/srs-boilerplate-saas.md#6-non-functional-requirements)
- [Performance Targets](../../CLAUDE.md#performance-targets-p95)
- [Integration Tests](../../services/cpp/auth/tests/README.md)

## Test Maintenance

Review and update tests when:
- Adding new API endpoints
- Changing NFR targets
- Identifying new bottlenecks
- After major refactors
- Before production deployments
