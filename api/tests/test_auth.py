"""
Tests for authentication endpoints
"""
import pytest
from fastapi.testclient import TestClient
from main import app

client = TestClient(app)


def test_health_check():
    """Health check should work without authentication"""
    response = client.get("/health")
    assert response.status_code == 200
    assert response.json()["status"] == "healthy"


def test_login_requires_credentials():
    """Login should require email and password"""
    response = client.post("/v1/auth/login", json={})
    assert response.status_code == 422  # Validation error


def test_login_not_implemented():
    """Login endpoint should return 501 (not implemented)"""
    response = client.post("/v1/auth/login", json={
        "email": "test@example.com",
        "password": "password123"
    })
    assert response.status_code == 501


# TODO: Add tests for:
# - Successful login
# - Failed login (wrong password)
# - Token refresh
# - Token rotation
# - Logout and token blacklist
# - API key creation/revocation
# - 2FA flow
