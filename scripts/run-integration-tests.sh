#!/bin/bash
set -e

# Run Integration Tests for SaaSForge
# This script starts test containers, runs integration tests, and cleans up

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

echo "🧪 SaaSForge Integration Test Runner"
echo "===================================="

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Step 1: Start test containers
echo -e "\n${YELLOW}1. Starting test containers (PostgreSQL, Redis)...${NC}"
cd "$PROJECT_ROOT"
docker-compose -f docker-compose.test.yml up -d

# Wait for services to be healthy
echo "⏳ Waiting for test database to be ready..."
timeout=30
counter=0
until docker exec saasforge_test_postgres pg_isready -U test -d test_saasforge > /dev/null 2>&1; do
    counter=$((counter + 1))
    if [ $counter -gt $timeout ]; then
        echo -e "${RED}❌ Test database failed to start${NC}"
        docker-compose -f docker-compose.test.yml logs test_postgres
        docker-compose -f docker-compose.test.yml down
        exit 1
    fi
    sleep 1
done
echo -e "${GREEN}✓ Test database ready${NC}"

echo "⏳ Waiting for test Redis to be ready..."
counter=0
until docker exec saasforge_test_redis redis-cli ping > /dev/null 2>&1; do
    counter=$((counter + 1))
    if [ $counter -gt $timeout ]; then
        echo -e "${RED}❌ Test Redis failed to start${NC}"
        docker-compose -f docker-compose.test.yml logs test_redis
        docker-compose -f docker-compose.test.yml down
        exit 1
    fi
    sleep 1
done
echo -e "${GREEN}✓ Test Redis ready${NC}"

# Step 2: Build integration tests
echo -e "\n${YELLOW}2. Building integration tests...${NC}"
cd "$PROJECT_ROOT/services/cpp/build"
cmake .. -DCMAKE_BUILD_TYPE=Debug
make auth_integration_test -j$(nproc)

if [ $? -ne 0 ]; then
    echo -e "${RED}❌ Build failed${NC}"
    docker-compose -f docker-compose.test.yml down
    exit 1
fi
echo -e "${GREEN}✓ Build successful${NC}"

# Step 3: Run integration tests
echo -e "\n${YELLOW}3. Running integration tests...${NC}"
cd "$PROJECT_ROOT/services/cpp/build/auth"

# Run with verbose output
./auth_integration_test --gtest_color=yes

TEST_EXIT_CODE=$?

# Step 4: Cleanup
echo -e "\n${YELLOW}4. Cleaning up test containers...${NC}"
cd "$PROJECT_ROOT"
docker-compose -f docker-compose.test.yml down -v

# Report results
echo ""
echo "===================================="
if [ $TEST_EXIT_CODE -eq 0 ]; then
    echo -e "${GREEN}✅ All integration tests passed!${NC}"
else
    echo -e "${RED}❌ Some integration tests failed${NC}"
    exit $TEST_EXIT_CODE
fi

exit 0
