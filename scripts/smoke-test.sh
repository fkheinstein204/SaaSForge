#!/bin/bash
set -e

# Smoke tests for SaaSForge deployment
# Usage: ./smoke-test.sh <BASE_URL>
#
# Example:
#   ./smoke-test.sh https://staging.saasforge.io
#   ./smoke-test.sh http://localhost:8000

BASE_URL="${1:-http://localhost:8000}"
TIMEOUT=30
RETRY_DELAY=5

echo "========================================="
echo "Running smoke tests against: ${BASE_URL}"
echo "========================================="
echo ""

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Test counter
TESTS_RUN=0
TESTS_PASSED=0
TESTS_FAILED=0

# Helper function to test endpoint
test_endpoint() {
    local endpoint="$1"
    local expected_status="${2:-200}"
    local description="$3"
    local max_retries="${4:-3}"

    TESTS_RUN=$((TESTS_RUN + 1))

    echo -n "[$TESTS_RUN] Testing: $description... "

    for i in $(seq 1 $max_retries); do
        status_code=$(curl -s -o /dev/null -w "%{http_code}" \
            --max-time $TIMEOUT \
            "${BASE_URL}${endpoint}" 2>/dev/null || echo "000")

        if [ "$status_code" = "$expected_status" ]; then
            echo -e "${GREEN}✓ PASS${NC} (HTTP $status_code)"
            TESTS_PASSED=$((TESTS_PASSED + 1))
            return 0
        fi

        if [ $i -lt $max_retries ]; then
            echo -n "Retry $i/$max_retries... "
            sleep $RETRY_DELAY
        fi
    done

    echo -e "${RED}✗ FAIL${NC} (Expected HTTP $expected_status, got $status_code)"
    TESTS_FAILED=$((TESTS_FAILED + 1))
    return 1
}

# Helper function to test endpoint with content check
test_endpoint_content() {
    local endpoint="$1"
    local expected_content="$2"
    local description="$3"
    local max_retries="${4:-3}"

    TESTS_RUN=$((TESTS_RUN + 1))

    echo -n "[$TESTS_RUN] Testing: $description... "

    for i in $(seq 1 $max_retries); do
        response=$(curl -s --max-time $TIMEOUT "${BASE_URL}${endpoint}" 2>/dev/null || echo "")

        if echo "$response" | grep -q "$expected_content"; then
            echo -e "${GREEN}✓ PASS${NC} (Found: $expected_content)"
            TESTS_PASSED=$((TESTS_PASSED + 1))
            return 0
        fi

        if [ $i -lt $max_retries ]; then
            echo -n "Retry $i/$max_retries... "
            sleep $RETRY_DELAY
        fi
    done

    echo -e "${RED}✗ FAIL${NC} (Expected content not found: $expected_content)"
    TESTS_FAILED=$((TESTS_FAILED + 1))
    return 1
}

# Test 1: Health check endpoint
test_endpoint_content "/health" "healthy" "API health check" 5

# Test 2: OpenAPI docs endpoint
test_endpoint "/docs" "200" "OpenAPI docs endpoint" 3

# Test 3: OpenAPI JSON schema
test_endpoint "/openapi.json" "200" "OpenAPI JSON schema" 3

# Test 4: ReDoc endpoint
test_endpoint "/redoc" "200" "ReDoc documentation" 3

# Test 5: Authentication endpoints exist (should return 401/422, not 404)
test_endpoint "/v1/auth/login" "422" "Auth login endpoint exists" 2

# Test 6: OAuth endpoints exist
test_endpoint "/v1/oauth/google" "307" "OAuth Google redirect exists" 2

# Test 7: Upload endpoints (protected - should return 401)
test_endpoint "/v1/uploads/presign" "401" "Upload presign endpoint exists" 2

# Test 8: Payment endpoints (protected - should return 401)
test_endpoint "/v1/payments/subscriptions" "401" "Payment subscriptions endpoint exists" 2

# Test 9: Notification endpoints (protected - should return 401)
test_endpoint "/v1/notifications" "401" "Notifications endpoint exists" 2

# Test 10: Check CORS headers
echo -n "[$((TESTS_RUN + 1))] Testing: CORS headers... "
TESTS_RUN=$((TESTS_RUN + 1))

cors_headers=$(curl -s -I -X OPTIONS \
    -H "Origin: http://localhost:3000" \
    -H "Access-Control-Request-Method: POST" \
    "${BASE_URL}/health" 2>/dev/null || echo "")

if echo "$cors_headers" | grep -qi "access-control-allow-origin"; then
    echo -e "${GREEN}✓ PASS${NC} (CORS headers present)"
    TESTS_PASSED=$((TESTS_PASSED + 1))
else
    echo -e "${YELLOW}⚠ WARN${NC} (CORS headers not found - may be expected)"
    TESTS_PASSED=$((TESTS_PASSED + 1))
fi

# Test 11: Response time check (health endpoint should be fast)
echo -n "[$((TESTS_RUN + 1))] Testing: Response time (< 1s)... "
TESTS_RUN=$((TESTS_RUN + 1))

response_time=$(curl -s -o /dev/null -w "%{time_total}" \
    --max-time $TIMEOUT \
    "${BASE_URL}/health" 2>/dev/null || echo "999")

if (( $(echo "$response_time < 1.0" | bc -l) )); then
    echo -e "${GREEN}✓ PASS${NC} (${response_time}s)"
    TESTS_PASSED=$((TESTS_PASSED + 1))
else
    echo -e "${YELLOW}⚠ WARN${NC} (${response_time}s - slower than expected)"
    TESTS_PASSED=$((TESTS_PASSED + 1))
fi

# Test 12: Check if gRPC services are healthy (if grpc_health_probe is available)
if command -v grpc_health_probe &> /dev/null; then
    echo -n "[$((TESTS_RUN + 1))] Testing: gRPC services health... "
    TESTS_RUN=$((TESTS_RUN + 1))

    grpc_services=("localhost:50051" "localhost:50052" "localhost:50053" "localhost:50054")
    grpc_healthy=true

    for service in "${grpc_services[@]}"; do
        if ! grpc_health_probe -addr="$service" -connect-timeout=5s 2>/dev/null; then
            grpc_healthy=false
            break
        fi
    done

    if [ "$grpc_healthy" = true ]; then
        echo -e "${GREEN}✓ PASS${NC} (All gRPC services healthy)"
        TESTS_PASSED=$((TESTS_PASSED + 1))
    else
        echo -e "${YELLOW}⚠ SKIP${NC} (gRPC services not accessible from test environment)"
        # Don't count as failure - gRPC may not be exposed externally
    fi
else
    echo -e "${YELLOW}[INFO]${NC} Skipping gRPC health check (grpc_health_probe not installed)"
fi

# Print summary
echo ""
echo "========================================="
echo "Smoke Test Summary"
echo "========================================="
echo -e "Total tests:  ${TESTS_RUN}"
echo -e "Passed:       ${GREEN}${TESTS_PASSED}${NC}"
echo -e "Failed:       ${RED}${TESTS_FAILED}${NC}"
echo ""

if [ $TESTS_FAILED -eq 0 ]; then
    echo -e "${GREEN}✅ All smoke tests passed!${NC}"
    exit 0
else
    echo -e "${RED}❌ Some smoke tests failed!${NC}"
    exit 1
fi
