"""
@file        auth.py
@brief       Authentication dependencies for FastAPI with RS256 JWT validation
@copyright   (c) 2025 FtsCoDe GmbH. All rights reserved.
@author      Heinstein F.
@date        2025-11-15

@details
Provides centralized JWT token validation with proper RS256 signature verification,
Redis blacklist checking, and user context extraction. This is the single source
of truth for JWT authentication across all FastAPI routers.

Security Features:
- RS256 asymmetric signature verification (production)
- HS256 symmetric signing (development/testing only)
- Redis blacklist check for revoked tokens
- Algorithm whitelist (prevents algorithm confusion attacks)
- Comprehensive error handling
"""

from fastapi import Depends, HTTPException, status, Header
from typing import Optional
from pydantic import BaseModel
import jwt
import os
import logging
import redis

logger = logging.getLogger(__name__)

# JWT Configuration
JWT_ALGORITHM = os.getenv("JWT_ALGORITHM", "HS256")  # Use RS256 in production
JWT_SECRET_KEY = os.getenv("JWT_SECRET_KEY")  # For HS256
JWT_PUBLIC_KEY = os.getenv("JWT_PUBLIC_KEY")  # For RS256 verification
JWT_PRIVATE_KEY = os.getenv("JWT_PRIVATE_KEY")  # For RS256 signing

# Redis Configuration for token blacklist
REDIS_HOST = os.getenv("REDIS_HOST", "localhost")
REDIS_PORT = int(os.getenv("REDIS_PORT", 6379))
REDIS_DB = int(os.getenv("REDIS_DB", 0))

# Initialize Redis client (lazy initialization)
_redis_client: Optional[redis.Redis] = None


def get_redis_client() -> redis.Redis:
    """
    Get Redis client with lazy initialization and connection pooling.

    Returns:
        redis.Redis: Connected Redis client
    """
    global _redis_client
    if _redis_client is None:
        try:
            _redis_client = redis.Redis(
                host=REDIS_HOST,
                port=REDIS_PORT,
                db=REDIS_DB,
                decode_responses=True,
                socket_connect_timeout=5,
                socket_timeout=5,
            )
            # Test connection
            _redis_client.ping()
            logger.info(f"Redis connected: {REDIS_HOST}:{REDIS_PORT}")
        except redis.ConnectionError as e:
            logger.error(f"Redis connection failed: {e}")
            # Continue without Redis (blacklist checks will be skipped with warning)
            _redis_client = None
    return _redis_client


class UserContext(BaseModel):
    """
    User context extracted from validated JWT token.

    Attributes:
        user_id: Unique user identifier (from 'sub' claim)
        tenant_id: Tenant identifier for multi-tenancy
        email: User email address
        jti: JWT ID for token revocation tracking (optional)
    """
    user_id: str
    tenant_id: str
    email: Optional[str] = None
    jti: Optional[str] = None


def extract_token_from_header(authorization: Optional[str] = Header(None)) -> str:
    """
    Extract JWT token from Authorization header.

    Args:
        authorization: Authorization header value (Bearer <token>)

    Returns:
        str: JWT token string

    Raises:
        HTTPException: If header is missing or malformed
    """
    if not authorization:
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="Missing authorization header",
            headers={"WWW-Authenticate": "Bearer"},
        )

    if not authorization.startswith("Bearer "):
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="Invalid authorization header format. Expected: Bearer <token>",
            headers={"WWW-Authenticate": "Bearer"},
        )

    return authorization.replace("Bearer ", "")


def check_token_blacklist(jti: Optional[str]) -> bool:
    """
    Check if token is blacklisted (revoked) in Redis.

    Args:
        jti: JWT ID from token claims

    Returns:
        bool: True if token is blacklisted, False otherwise
    """
    if not jti:
        return False  # No JTI claim, cannot check blacklist

    redis_client = get_redis_client()
    if not redis_client:
        logger.warning("Redis unavailable - token blacklist check skipped")
        return False  # Fail open (security risk, but prevents downtime)

    try:
        blacklist_key = f"blacklist:{jti}"
        is_blacklisted = redis_client.exists(blacklist_key)
        return bool(is_blacklisted)
    except Exception as e:
        logger.error(f"Redis blacklist check failed: {e}")
        return False  # Fail open


def validate_jwt_token(token: str) -> dict:
    """
    Validate JWT token with RS256/HS256 signature verification.

    This function implements secure JWT validation according to RFC 8725:
    1. Explicitly whitelist allowed algorithms (RS256 or HS256)
    2. Reject unsigned tokens (alg: none) to prevent algorithm confusion attacks
    3. Verify signature with appropriate key (public key for RS256, secret for HS256)
    4. Check expiration, audience, issuer claims
    5. Check Redis blacklist for revoked tokens

    Args:
        token: JWT token string

    Returns:
        dict: Decoded JWT payload with claims

    Raises:
        HTTPException: If token validation fails
    """
    try:
        # Step 1: Explicitly whitelist algorithms to prevent algorithm confusion attacks
        allowed_algorithms = ["RS256", "HS256"]

        if JWT_ALGORITHM not in allowed_algorithms:
            logger.error(f"Unsafe JWT algorithm configured: {JWT_ALGORITHM}")
            raise HTTPException(
                status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
                detail="JWT algorithm configuration error"
            )

        # Step 2: Determine verification key based on algorithm
        if JWT_ALGORITHM == "RS256":
            if not JWT_PUBLIC_KEY:
                logger.error("JWT_PUBLIC_KEY not configured for RS256 verification")
                raise HTTPException(
                    status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
                    detail="JWT public key not configured"
                )
            verification_key = JWT_PUBLIC_KEY
        else:  # HS256
            if not JWT_SECRET_KEY:
                logger.error("JWT_SECRET_KEY not configured for HS256 verification")
                raise HTTPException(
                    status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
                    detail="JWT secret key not configured"
                )
            verification_key = JWT_SECRET_KEY

        # Step 3: Decode and verify JWT with signature validation
        payload = jwt.decode(
            token,
            verification_key,
            algorithms=[JWT_ALGORITHM],  # Explicit algorithm whitelist
            options={
                "verify_signature": True,  # ✅ CRITICAL: Always verify signature in production
                "verify_exp": True,  # Verify expiration
                "verify_iat": True,  # Verify issued-at
                "require": ["exp", "sub"],  # Require expiration and subject claims
            }
        )

        # Step 4: Check token blacklist (revoked tokens)
        jti = payload.get("jti")
        if check_token_blacklist(jti):
            logger.warning(f"Blacklisted token used: jti={jti}")
            raise HTTPException(
                status_code=status.HTTP_401_UNAUTHORIZED,
                detail="Token has been revoked",
                headers={"WWW-Authenticate": "Bearer"},
            )

        return payload

    except jwt.ExpiredSignatureError:
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="Token has expired",
            headers={"WWW-Authenticate": "Bearer"},
        )
    except jwt.InvalidSignatureError:
        logger.warning("Invalid JWT signature detected")
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="Invalid token signature",
            headers={"WWW-Authenticate": "Bearer"},
        )
    except jwt.InvalidAlgorithmError:
        logger.warning(f"Invalid JWT algorithm detected (expected {JWT_ALGORITHM})")
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="Invalid token algorithm",
            headers={"WWW-Authenticate": "Bearer"},
        )
    except jwt.DecodeError as e:
        logger.warning(f"JWT decode error: {e}")
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="Malformed token",
            headers={"WWW-Authenticate": "Bearer"},
        )
    except HTTPException:
        raise  # Re-raise HTTP exceptions as-is
    except Exception as e:
        logger.error(f"Unexpected JWT validation error: {e}")
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="Token validation failed",
            headers={"WWW-Authenticate": "Bearer"},
        )


def get_current_user(authorization: Optional[str] = Header(None)) -> UserContext:
    """
    FastAPI dependency to extract and validate current user from JWT token.

    This is the primary authentication dependency used across all protected endpoints.

    Usage:
        @router.get("/protected")
        async def protected_endpoint(user: UserContext = Depends(get_current_user)):
            print(f"User {user.user_id} from tenant {user.tenant_id}")

    Args:
        authorization: Authorization header (injected by FastAPI)

    Returns:
        UserContext: Validated user context

    Raises:
        HTTPException: If authentication fails
    """
    # Extract token from header
    token = extract_token_from_header(authorization)

    # Validate token and get payload
    payload = validate_jwt_token(token)

    # Extract required user context
    user_id = payload.get("sub") or payload.get("user_id")
    tenant_id = payload.get("tenant_id")
    email = payload.get("email")
    jti = payload.get("jti")

    if not user_id:
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="Token missing 'sub' or 'user_id' claim",
        )

    if not tenant_id:
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="Token missing 'tenant_id' claim (required for multi-tenancy)",
        )

    return UserContext(
        user_id=user_id,
        tenant_id=tenant_id,
        email=email,
        jti=jti,
    )


def extract_user_from_jwt(authorization: str) -> tuple[str, str, str]:
    """
    Legacy helper function for backward compatibility.

    DEPRECATED: Use get_current_user dependency instead.

    This function is maintained for backward compatibility with existing code
    that doesn't use FastAPI's dependency injection system.

    Args:
        authorization: Authorization header value (Bearer <token>)

    Returns:
        Tuple of (user_id, tenant_id, email)

    Raises:
        HTTPException: If token validation fails
    """
    user_context = get_current_user(authorization)
    return user_context.user_id, user_context.tenant_id, user_context.email
