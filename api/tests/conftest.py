"""
@file        conftest.py
@brief       Pytest configuration and shared fixtures with RS256 JWT support
@copyright   (c) 2025 FtsCoDe GmbH. All rights reserved.
@author      Heinstein F.
@date        2025-11-15

@details
SECURITY FIX: Updated to use RS256 (asymmetric) instead of HS256 (symmetric)
to match production JWT configuration. Also added Redis and DB mock fixtures.
"""

import pytest
import os
from fastapi.testclient import TestClient
from datetime import datetime, timedelta
import jwt
from cryptography.hazmat.primitives.asymmetric import rsa
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.backends import default_backend
from unittest.mock import Mock, patch
import asyncpg

# Generate test RSA keypair for RS256 JWT signing
# This is done once when tests start to avoid performance overhead
_test_private_key = rsa.generate_private_key(
    public_exponent=65537,
    key_size=2048,
    backend=default_backend()
)
_test_public_key = _test_private_key.public_key()

# Serialize keys to PEM format
TEST_PRIVATE_KEY_PEM = _test_private_key.private_bytes(
    encoding=serialization.Encoding.PEM,
    format=serialization.PrivateFormat.PKCS8,
    encryption_algorithm=serialization.NoEncryption()
).decode('utf-8')

TEST_PUBLIC_KEY_PEM = _test_public_key.public_bytes(
    encoding=serialization.Encoding.PEM,
    format=serialization.PublicFormat.SubjectPublicKeyInfo
).decode('utf-8')

# Set test environment variables
os.environ["ENVIRONMENT"] = "test"
os.environ["JWT_ALGORITHM"] = "RS256"  # SECURITY FIX: Use RS256 in tests
os.environ["JWT_PUBLIC_KEY"] = TEST_PUBLIC_KEY_PEM
os.environ["JWT_PRIVATE_KEY"] = TEST_PRIVATE_KEY_PEM
os.environ["JWT_ACCESS_TOKEN_EXPIRE_MINUTES"] = "15"
os.environ["JWT_REFRESH_TOKEN_EXPIRE_DAYS"] = "30"
os.environ["JWT_ISSUER"] = "saasforge-test"
os.environ["JWT_AUDIENCE"] = "saasforge-api-test"
os.environ["S3_ENDPOINT_URL"] = "http://localhost:4566"
os.environ["S3_BUCKET"] = "saasforge-uploads-test"
os.environ["AWS_ACCESS_KEY_ID"] = "test"
os.environ["AWS_SECRET_ACCESS_KEY"] = "test"
os.environ["AWS_DEFAULT_REGION"] = "us-east-1"
os.environ["REDIS_HOST"] = "localhost"
os.environ["REDIS_PORT"] = "6379"
os.environ["REDIS_DB"] = "1"  # Use DB 1 for tests (DB 0 for dev)


@pytest.fixture
def client():
    """FastAPI test client"""
    from main import app
    return TestClient(app)


@pytest.fixture
def test_user_data():
    """Test user data"""
    return {
        "email": "test@example.com",
        "password": "TestPassword123!",
        "name": "Test User",
        "user_id": "user_test123",
        "tenant_id": "tenant_test123",
    }


@pytest.fixture
def auth_headers(test_user_data):
    """
    Generate JWT auth headers for testing with RS256 signature.

    SECURITY FIX: Now uses RS256 (asymmetric) instead of HS256 (symmetric)
    to match production configuration and test signature verification.
    """
    payload = {
        "sub": test_user_data["user_id"],
        "user_id": test_user_data["user_id"],
        "tenant_id": test_user_data["tenant_id"],
        "email": test_user_data["email"],
        "iss": os.getenv("JWT_ISSUER"),
        "aud": os.getenv("JWT_AUDIENCE"),
        "exp": datetime.utcnow() + timedelta(minutes=15),
        "iat": datetime.utcnow(),
        "jti": "test-jwt-id-12345",  # JWT ID for blacklist testing
    }

    # Sign with RS256 using test private key
    token = jwt.encode(
        payload,
        TEST_PRIVATE_KEY_PEM,
        algorithm="RS256"
    )

    return {"Authorization": f"Bearer {token}"}


@pytest.fixture
def mock_stripe_client():
    """Get mock Stripe client instance"""
    from services.mock_stripe_client import get_mock_stripe_client
    return get_mock_stripe_client()


@pytest.fixture
def test_customer(mock_stripe_client, test_user_data):
    """Create a test Stripe customer"""
    customer = mock_stripe_client.create_customer(
        email=test_user_data["email"],
        name=test_user_data["name"],
        metadata={
            "user_id": test_user_data["user_id"],
            "tenant_id": test_user_data["tenant_id"],
        }
    )
    return customer


@pytest.fixture
def test_payment_method(mock_stripe_client):
    """Create a test payment method"""
    pm = mock_stripe_client.create_payment_method(
        type="card",
        card={
            "brand": "visa",
            "last4": "4242",
            "exp_month": 12,
            "exp_year": 2025,
        }
    )
    return pm


@pytest.fixture(autouse=True)
def reset_mock_stripe():
    """Reset mock Stripe state before each test"""
    from services.mock_stripe_client import MockStripeClient, _mock_stripe_client
    global _mock_stripe_client
    _mock_stripe_client = None
    yield
    _mock_stripe_client = None


@pytest.fixture
def sample_file_data():
    """Sample file data for upload tests"""
    return {
        "filename": "test-file.txt",
        "content": b"This is test file content",
        "content_type": "text/plain",
    }


@pytest.fixture
def mock_redis():
    """
    Mock Redis client for testing blacklist, OTP, and password reset flows.

    SECURITY FIX: Enables testing of:
    - JWT blacklist (Fix #1, #2)
    - OTP generation and validation (Fix #5)
    - Password reset tokens (Fix #3)
    """
    redis_mock = Mock()

    # In-memory storage for test isolation
    _redis_store = {}

    def mock_get(key):
        """Get value from mock store, respecting TTL"""
        if key in _redis_store:
            return _redis_store[key].encode() if isinstance(_redis_store[key], str) else _redis_store[key]
        return None

    def mock_setex(key, ttl, value):
        """Set value with TTL (TTL not enforced in tests)"""
        _redis_store[key] = value
        return True

    def mock_exists(key):
        """Check if key exists"""
        return 1 if key in _redis_store else 0

    def mock_delete(*keys):
        """Delete one or more keys"""
        count = 0
        for key in keys:
            if key in _redis_store:
                del _redis_store[key]
                count += 1
        return count

    def mock_incr(key):
        """Increment counter"""
        current = int(_redis_store.get(key, 0))
        _redis_store[key] = current + 1
        return _redis_store[key]

    def mock_expire(key, ttl):
        """Set expiry (no-op in tests)"""
        return True

    # Bind mock methods
    redis_mock.get = Mock(side_effect=mock_get)
    redis_mock.setex = Mock(side_effect=mock_setex)
    redis_mock.exists = Mock(side_effect=mock_exists)
    redis_mock.delete = Mock(side_effect=mock_delete)
    redis_mock.incr = Mock(side_effect=mock_incr)
    redis_mock.expire = Mock(side_effect=mock_expire)

    # Reset store after each test
    yield redis_mock
    _redis_store.clear()


@pytest.fixture
async def test_db():
    """
    Test database fixture with automatic cleanup.

    SECURITY FIX: Provides isolated test database for:
    - User authentication tests
    - Multi-tenancy isolation tests
    - Permission and RBAC tests
    """
    import asyncpg

    # Test database connection
    DATABASE_URL = os.getenv(
        "TEST_DATABASE_URL",
        "postgresql://saasforge:devpass123@localhost:5432/saasforge_test"
    )

    pool = await asyncpg.create_pool(DATABASE_URL, min_size=2, max_size=10)

    try:
        yield pool
    finally:
        # Cleanup: Delete all test data
        async with pool.acquire() as conn:
            # Disable foreign key checks temporarily
            await conn.execute("SET session_replication_role = 'replica';")

            # Truncate test tables
            tables = [
                "sessions", "api_keys", "upload_objects", "quotas",
                "subscriptions", "payment_methods", "invoices", "usage_records",
                "notifications", "notification_preferences", "webhooks",
                "email_templates", "users", "tenants"
            ]

            for table in tables:
                try:
                    await conn.execute(f"TRUNCATE TABLE {table} CASCADE;")
                except Exception as e:
                    # Table might not exist yet
                    pass

            # Re-enable foreign key checks
            await conn.execute("SET session_replication_role = 'origin';")

        await pool.close()
