"""
JWT validation middleware
Validates access tokens and extracts user context
"""
from fastapi import Request, HTTPException, status
from starlette.middleware.base import BaseHTTPMiddleware
from typing import Optional
import jwt
# import redis

# Public endpoints that don't require authentication
PUBLIC_PATHS = ["/health", "/v1/auth/login", "/v1/auth/refresh", "/docs", "/openapi.json"]


class JWTMiddleware(BaseHTTPMiddleware):
    def __init__(self, app):
        super().__init__(app)
        # TODO: Load public key from environment
        # TODO: Initialize Redis connection
        pass

    async def dispatch(self, request: Request, call_next):
        # Skip authentication for public paths
        if any(request.url.path.startswith(path) for path in PUBLIC_PATHS):
            return await call_next(request)

        # Extract Authorization header
        auth_header = request.headers.get("Authorization")
        if not auth_header or not auth_header.startswith("Bearer "):
            raise HTTPException(
                status_code=status.HTTP_401_UNAUTHORIZED,
                detail="Missing or invalid Authorization header",
                headers={"WWW-Authenticate": "Bearer"}
            )

        token = auth_header.split(" ")[1]

        try:
            # TODO: Validate JWT signature (RS256 only)
            # TODO: Check expiration, issuer, audience
            # TODO: Check Redis blacklist
            # payload = jwt.decode(token, public_key, algorithms=["RS256"])

            # Extract user context
            # request.state.user_id = payload["sub"]
            # request.state.tenant_id = payload["tenant_id"]
            # request.state.email = payload["email"]
            # request.state.roles = payload.get("roles", [])

            # For now, pass through (placeholder)
            pass

        except jwt.ExpiredSignatureError:
            raise HTTPException(
                status_code=status.HTTP_401_UNAUTHORIZED,
                detail="Token expired",
                headers={"WWW-Authenticate": "Bearer"}
            )
        except jwt.InvalidTokenError as e:
            raise HTTPException(
                status_code=status.HTTP_401_UNAUTHORIZED,
                detail=f"Invalid token: {str(e)}",
                headers={"WWW-Authenticate": "Bearer"}
            )

        response = await call_next(request)
        return response
