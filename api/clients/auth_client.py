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

        # Load mTLS certificates for secure service-to-service communication
        ca_cert_path = os.getenv("GRPC_CA_CERT_PATH", "certs/ca.crt")
        client_cert_path = os.getenv("GRPC_CLIENT_CERT_PATH", "certs/client.crt")
        client_key_path = os.getenv("GRPC_CLIENT_KEY_PATH", "certs/client.key")

        try:
            with open(ca_cert_path, "rb") as f:
                ca_cert = f.read()
            with open(client_cert_path, "rb") as f:
                client_cert = f.read()
            with open(client_key_path, "rb") as f:
                client_key = f.read()

            # Create SSL credentials with client certificate for mTLS
            credentials = grpc.ssl_channel_credentials(
                root_certificates=ca_cert,
                private_key=client_key,
                certificate_chain=client_cert
            )

            # Create secure channel with mTLS
            self.channel = grpc.secure_channel(self.address, credentials)
        except FileNotFoundError as e:
            # Fallback to insecure channel for local development only
            print(f"WARNING: mTLS certs not found ({e}), using insecure channel")
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
