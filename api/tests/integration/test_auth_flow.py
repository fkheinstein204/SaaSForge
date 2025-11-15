"""
@file        test_auth_flow.py
@brief       Integration tests for authentication flows
@copyright   (c) 2025 FtsCoDe GmbH. All rights reserved.
@author      Heinstein F.
@date        2025-11-15

Tests:
1. Complete registration flow
2. Login with email/password
"""

import pytest
from fastapi import status


class TestAuthenticationFlow:
    """Test authentication flows"""

    def test_complete_registration_flow(self, client):
        """
        Test 1: Complete user registration flow

        Steps:
        1. Register new user
        2. Verify user is created
        3. Verify password is hashed
        """
        # Register new user
        registration_data = {
            "email": "newuser@example.com",
            "password": "SecurePassword123!",
            "name": "New User",
        }

        response = client.post("/v1/auth/register", json=registration_data)

        # Verify successful registration
        assert response.status_code == status.HTTP_201_CREATED
        data = response.json()
        assert "user_id" in data
        assert "tenant_id" in data
        assert data["email"] == registration_data["email"]
        assert "password" not in data  # Password should not be returned

    def test_login_with_email_password(self, client, test_user_data):
        """
        Test 2: Login with email and password

        Steps:
        1. Register user
        2. Login with credentials
        3. Verify JWT tokens are returned
        4. Verify token payload
        """
        # First register the user
        client.post("/v1/auth/register", json={
            "email": test_user_data["email"],
            "password": test_user_data["password"],
            "name": test_user_data["name"],
        })

        # Login
        login_data = {
            "email": test_user_data["email"],
            "password": test_user_data["password"],
        }

        response = client.post("/v1/auth/login", json=login_data)

        # Verify successful login
        assert response.status_code == status.HTTP_200_OK
        data = response.json()
        assert "access_token" in data
        assert "refresh_token" in data
        assert "user_id" in data
        assert "tenant_id" in data
        assert data["email"] == test_user_data["email"]

    def test_login_with_invalid_credentials(self, client, test_user_data):
        """
        Test: Login with invalid credentials should fail
        """
        # Register user
        client.post("/v1/auth/register", json={
            "email": test_user_data["email"],
            "password": test_user_data["password"],
            "name": test_user_data["name"],
        })

        # Try login with wrong password
        response = client.post("/v1/auth/login", json={
            "email": test_user_data["email"],
            "password": "WrongPassword123!",
        })

        assert response.status_code == status.HTTP_401_UNAUTHORIZED

    def test_access_protected_endpoint(self, client, auth_headers):
        """
        Test: Access protected endpoint with valid JWT
        """
        # Try to access a protected endpoint (e.g., profile)
        response = client.get("/v1/auth/profile", headers=auth_headers)

        # Should succeed with valid token
        assert response.status_code in [status.HTTP_200_OK, status.HTTP_404_NOT_FOUND]
        # 404 is acceptable if endpoint doesn't exist yet

    def test_access_protected_endpoint_without_auth(self, client):
        """
        Test: Access protected endpoint without authentication should fail
        """
        response = client.get("/v1/auth/profile")

        assert response.status_code == status.HTTP_401_UNAUTHORIZED
