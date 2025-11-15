#!/bin/bash

##
# @file        run_tests.sh
# @brief       Run integration tests for SaaSForge API
# @copyright   (c) 2025 FtsCoDe GmbH. All rights reserved.
# @author      Heinstein F.
# @date        2025-11-15
##

set -e

echo "========================================="
echo "SaaSForge API Integration Tests"
echo "========================================="
echo ""

# Colors for output
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

# Check if virtual environment is activated
if [ -z "$VIRTUAL_ENV" ]; then
    echo -e "${YELLOW}Warning: Virtual environment not activated${NC}"
    echo "Attempting to activate venv..."
    if [ -f "venv/bin/activate" ]; then
        source venv/bin/activate
    else
        echo -e "${RED}Error: venv not found. Please create virtual environment first.${NC}"
        exit 1
    fi
fi

# Install test dependencies if needed
echo "Checking test dependencies..."
pip install -q pytest pytest-cov httpx

echo ""
echo "========================================="
echo "Running Integration Tests"
echo "========================================="
echo ""

# Run tests based on command line argument
case "${1:-all}" in
    "auth")
        echo "Running authentication tests..."
        pytest tests/integration/test_auth_flow.py -v
        ;;
    "upload")
        echo "Running upload tests..."
        pytest tests/integration/test_upload_flow.py -v
        ;;
    "payment")
        echo "Running payment tests..."
        pytest tests/integration/test_payment_flow.py -v
        ;;
    "error")
        echo "Running error handling tests..."
        pytest tests/integration/test_error_handling.py -v
        ;;
    "integration")
        echo "Running all integration tests..."
        pytest tests/integration/ -v
        ;;
    "all")
        echo "Running all tests..."
        pytest tests/ -v
        ;;
    "coverage")
        echo "Running tests with coverage report..."
        pytest tests/integration/ -v --cov=. --cov-report=html --cov-report=term
        echo ""
        echo -e "${GREEN}Coverage report generated in htmlcov/index.html${NC}"
        ;;
    *)
        echo -e "${RED}Unknown test suite: $1${NC}"
        echo "Usage: ./run_tests.sh [auth|upload|payment|error|integration|all|coverage]"
        exit 1
        ;;
esac

# Check test results
if [ $? -eq 0 ]; then
    echo ""
    echo -e "${GREEN}=========================================${NC}"
    echo -e "${GREEN}All tests passed!${NC}"
    echo -e "${GREEN}=========================================${NC}"
else
    echo ""
    echo -e "${RED}=========================================${NC}"
    echo -e "${RED}Some tests failed!${NC}"
    echo -e "${RED}=========================================${NC}"
    exit 1
fi
