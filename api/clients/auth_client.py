"""gRPC client wrapper for Auth Service."""

import grpc
import os
from typing import Optional, Dict, List
import sys

# Add generated proto path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'generated'))

from auth_pb2 import (
    LoginRequest, LoginResponse,
    LogoutRequest, LogoutResponse,
    RefreshTokenRequest, RefreshTokenResponse,
    ValidateTokenRequest, ValidateTokenResponse,
    CreateApiKeyRequest, CreateApiKeyResponse,
    RevokeApiKeyRequest, RevokeApiKeyResponse
)
from auth_pb2_grpc import AuthServiceStub


class AuthClient:
    """Client for Auth Service gRPC calls."""

    def __init__(self, host: str = None, port: int = None):
        self.host = host or os.getenv("AUTH_SERVICE_HOST", "localhost")
        self.port = port or int(os.getenv("AUTH_SERVICE_PORT", "50051"))
        self.address = f"{self.host}:{self.port}"

        # Create channel (insecure for now, add mTLS later)
        self.channel = grpc.insecure_channel(self.address)
        self.stub = AuthServiceStub(self.channel)

    def login(self, email: str, password: str, totp_code: Optional[str] = None) -> LoginResponse:
        """Authenticate user with email/password."""
        request = LoginRequest(
            email=email,
            password=password,
            totp_code=totp_code or ""
        )
        return self.stub.Login(request)

    def logout(self, refresh_token: str) -> LogoutResponse:
        """Logout user by invalidating refresh token."""
        request = LogoutRequest(refresh_token=refresh_token)
        return self.stub.Logout(request)

    def refresh_token(self, refresh_token: str) -> RefreshTokenResponse:
        """Refresh access token using refresh token."""
        request = RefreshTokenRequest(refresh_token=refresh_token)
        return self.stub.RefreshToken(request)

    def validate_token(self, access_token: str) -> ValidateTokenResponse:
        """Validate JWT access token."""
        request = ValidateTokenRequest(access_token=access_token)
        return self.stub.ValidateToken(request)

    def create_api_key(
        self,
        name: str,
        scopes: List[str],
        metadata: List[tuple]
    ) -> CreateApiKeyResponse:
        """Create API key with tenant context in metadata."""
        request = CreateApiKeyRequest(name=name, scopes=scopes)
        return self.stub.CreateApiKey(request, metadata=metadata)

    def revoke_api_key(
        self,
        key_id: str,
        metadata: List[tuple]
    ) -> RevokeApiKeyResponse:
        """Revoke API key."""
        request = RevokeApiKeyRequest(key_id=key_id)
        return self.stub.RevokeApiKey(request, metadata=metadata)

    def close(self):
        """Close gRPC channel."""
        if self.channel:
            self.channel.close()

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()
