"""
Integration test: Full authentication flow
Tests login, token refresh, and logout across API + gRPC services
"""
import pytest
import requests
import time

BASE_URL = "http://localhost:8000"


class TestAuthFlow:
    def test_full_auth_flow(self):
        """Test complete authentication flow"""
        # TODO: Implement after services are running
        # 1. Login with valid credentials
        # 2. Receive access + refresh tokens
        # 3. Make authenticated request with access token
        # 4. Refresh access token
        # 5. Logout and verify token is blacklisted
        pass

    def test_token_expiration(self):
        """Test that expired tokens are rejected"""
        # TODO: Implement
        pass

    def test_refresh_token_rotation(self):
        """Test that refresh tokens are rotated on use"""
        # TODO: Implement
        pass

    def test_token_reuse_detection(self):
        """Test that reused refresh tokens trigger revocation"""
        # TODO: Implement
        pass
