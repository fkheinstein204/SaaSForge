"""
JWT validation middleware
Validates access tokens and extracts user context
"""
from fastapi import Request, HTTPException, status
from starlette.middleware.base import BaseHTTPMiddleware
from typing import Optional
import jwt
import redis
import os
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.backends import default_backend

# Public endpoints that don't require authentication
PUBLIC_PATHS = [
    "/health",
    "/v1/auth/login",
    "/v1/auth/refresh",
    "/v1/auth/register",
    "/v1/oauth",  # OAuth callback endpoints
    "/docs",
    "/openapi.json",
    "/redoc"
]


class JWTMiddleware(BaseHTTPMiddleware):
    def __init__(self, app):
        super().__init__(app)

        # Load RS256 public key from environment
        jwt_public_key_path = os.getenv("JWT_PUBLIC_KEY_PATH", "certs/jwt_rsa.pub")
        try:
            with open(jwt_public_key_path, "rb") as f:
                self.public_key = serialization.load_pem_public_key(
                    f.read(),
                    backend=default_backend()
                )
        except FileNotFoundError:
            # Fallback to environment variable
            jwt_public_key_pem = os.getenv("JWT_PUBLIC_KEY", "")
            if jwt_public_key_pem:
                self.public_key = serialization.load_pem_public_key(
                    jwt_public_key_pem.encode(),
                    backend=default_backend()
                )
            else:
                raise RuntimeError("JWT_PUBLIC_KEY_PATH or JWT_PUBLIC_KEY must be set")

        # Initialize Redis connection for blacklist checking
        redis_url = os.getenv("REDIS_URL", "redis://localhost:6379")
        self.redis_client = redis.from_url(redis_url, decode_responses=True)

        # JWT validation parameters
        self.issuer = os.getenv("JWT_ISSUER", "saasforge")
        self.audience = os.getenv("JWT_AUDIENCE", "saasforge-api")

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
            # Validate JWT signature with RS256 only (security: prevent algorithm confusion)
            payload = jwt.decode(
                token,
                self.public_key,
                algorithms=["RS256"],  # Explicitly whitelist RS256 only
                issuer=self.issuer,
                audience=self.audience,
                options={
                    "verify_signature": True,
                    "verify_exp": True,
                    "verify_iat": True,
                    "verify_iss": True,
                    "verify_aud": True,
                }
            )

            # Check Redis blacklist for revoked tokens
            jti = payload.get("jti")
            if jti:
                blacklisted = self.redis_client.exists(f"blacklist:{jti}")
                if blacklisted:
                    raise HTTPException(
                        status_code=status.HTTP_401_UNAUTHORIZED,
                        detail="Token has been revoked",
                        headers={"WWW-Authenticate": "Bearer"}
                    )

            # Extract user context and attach to request state
            request.state.user_id = payload.get("sub")
            request.state.tenant_id = payload.get("tenant_id")
            request.state.email = payload.get("email")
            request.state.roles = payload.get("roles", [])
            request.state.jti = jti  # Store for potential logout

            # Validate required claims
            if not request.state.user_id or not request.state.tenant_id:
                raise HTTPException(
                    status_code=status.HTTP_401_UNAUTHORIZED,
                    detail="Token missing required claims (sub, tenant_id)",
                    headers={"WWW-Authenticate": "Bearer"}
                )

        except jwt.ExpiredSignatureError:
            raise HTTPException(
                status_code=status.HTTP_401_UNAUTHORIZED,
                detail="Token expired",
                headers={"WWW-Authenticate": "Bearer"}
            )
        except jwt.InvalidIssuerError:
            raise HTTPException(
                status_code=status.HTTP_401_UNAUTHORIZED,
                detail="Invalid token issuer",
                headers={"WWW-Authenticate": "Bearer"}
            )
        except jwt.InvalidAudienceError:
            raise HTTPException(
                status_code=status.HTTP_401_UNAUTHORIZED,
                detail="Invalid token audience",
                headers={"WWW-Authenticate": "Bearer"}
            )
        except jwt.InvalidSignatureError:
            raise HTTPException(
                status_code=status.HTTP_401_UNAUTHORIZED,
                detail="Invalid token signature",
                headers={"WWW-Authenticate": "Bearer"}
            )
        except jwt.InvalidAlgorithmError:
            # Critical: Prevents algorithm confusion attacks (alg: none)
            raise HTTPException(
                status_code=status.HTTP_401_UNAUTHORIZED,
                detail="Invalid token algorithm (only RS256 allowed)",
                headers={"WWW-Authenticate": "Bearer"}
            )
        except jwt.InvalidTokenError as e:
            raise HTTPException(
                status_code=status.HTTP_401_UNAUTHORIZED,
                detail=f"Invalid token: {str(e)}",
                headers={"WWW-Authenticate": "Bearer"}
            )
        except redis.RedisError as e:
            # SECURITY FIX: Fail closed on Redis errors in production
            # Redis blacklist is critical for token revocation (logout, password reset)
            environment = os.getenv("ENVIRONMENT", "development")

            if environment == "production":
                # Fail closed - deny request if blacklist unavailable
                raise HTTPException(
                    status_code=status.HTTP_503_SERVICE_UNAVAILABLE,
                    detail="Authentication service temporarily unavailable. Please try again.",
                    headers={"WWW-Authenticate": "Bearer"}
                )
            else:
                # Development: Fail open with warning
                print(f"WARN: Redis blacklist check failed in {environment}: {e}")
                # Continue with request (token validated, but blacklist not checked)

        response = await call_next(request)
        return response
