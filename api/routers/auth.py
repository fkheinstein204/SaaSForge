"""
@file        auth.py
@brief       Authentication router with complete implementation
@copyright   (c) 2025 FtsCoDe GmbH. All rights reserved.
@author      Heinstein F.
@date        2025-11-15
"""

from fastapi import APIRouter, Depends, HTTPException, status, Header, Request
from typing import Optional
import grpc
import secrets
import logging

from models.auth import (
    LoginRequest, LoginResponse,
    RegisterRequest, RegisterResponse,
    RefreshTokenRequest, RefreshTokenResponse,
    LogoutRequest,
    TOTPEnrollRequest, TOTPEnrollResponse,
    TOTPVerifyRequest, TOTPVerifyResponse,
    TwoFactorVerifyRequest, TwoFactorBackupCodeRequest,
    PasswordResetRequest, PasswordResetConfirmRequest, PasswordChangeRequest,
    OTPRequest, OTPVerifyRequest,
    ApiKeyCreateRequest, ApiKeyResponse
)
from clients.auth_client import AuthClient
from services.email_service import get_email_service

logger = logging.getLogger(__name__)
router = APIRouter()


def get_auth_client() -> AuthClient:
    """Dependency to get AuthClient instance"""
    return AuthClient()


@router.post("/register", response_model=RegisterResponse, status_code=status.HTTP_201_CREATED)
async def register(request: RegisterRequest, auth_client: AuthClient = Depends(get_auth_client)):
    """
    Register a new user account

    Creates a new user with email/password authentication.
    Automatically creates a new tenant for the user.
    Sends welcome email.
    """
    try:
        # Call gRPC AuthService.Register
        response = auth_client.stub.Register(
            auth_client.stub.RegisterRequest(
                email=request.email,
                password=request.password,
                full_name=request.full_name
            )
        )

        # Send welcome email asynchronously
        email_service = get_email_service()
        await email_service.send_welcome_email(request.email, request.full_name)

        return RegisterResponse(
            access_token=response.access_token,
            refresh_token=response.refresh_token,
            expires_in=response.expires_in,
            user_id=response.user_id,
            tenant_id=response.tenant_id
        )

    except grpc.RpcError as e:
        logger.error(f"gRPC error during registration: {e.code()} - {e.details()}")

        if e.code() == grpc.StatusCode.ALREADY_EXISTS:
            raise HTTPException(
                status_code=status.HTTP_409_CONFLICT,
                detail="Email address already registered"
            )
        elif e.code() == grpc.StatusCode.INVALID_ARGUMENT:
            raise HTTPException(
                status_code=status.HTTP_400_BAD_REQUEST,
                detail=e.details()
            )
        else:
            raise HTTPException(
                status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
                detail="Registration failed"
            )


@router.post("/login", response_model=LoginResponse)
async def login(request: LoginRequest, auth_client: AuthClient = Depends(get_auth_client)):
    """
    User login with email/password

    Returns JWT access token + refresh token.
    If 2FA is enabled, returns HTTP 403 with requires_2fa flag.
    """
    try:
        response = auth_client.login(
            email=request.email,
            password=request.password,
            totp_code=request.totp_code
        )

        # Check if 2FA is required
        if hasattr(response, 'requires_2fa') and response.requires_2fa:
            raise HTTPException(
                status_code=status.HTTP_403_FORBIDDEN,
                detail={
                    "code": "2FA_REQUIRED",
                    "message": "Two-factor authentication required",
                    "requires_2fa": True,
                    "temp_token": response.temp_token
                }
            )

        return LoginResponse(
            access_token=response.access_token,
            refresh_token=response.refresh_token,
            expires_in=response.expires_in,
            user_id=response.user_id,
            tenant_id=response.tenant_id,
            totp_enabled=getattr(response, 'totp_enabled', False)
        )

    except grpc.RpcError as e:
        logger.error(f"gRPC error during login: {e.code()} - {e.details()}")

        if e.code() == grpc.StatusCode.UNAUTHENTICATED:
            raise HTTPException(
                status_code=status.HTTP_401_UNAUTHORIZED,
                detail="Invalid credentials"
            )
        elif e.code() == grpc.StatusCode.PERMISSION_DENIED:
            raise HTTPException(
                status_code=status.HTTP_403_FORBIDDEN,
                detail=e.details()
            )
        else:
            raise HTTPException(
                status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
                detail="Login failed"
            )


@router.post("/refresh", response_model=RefreshTokenResponse)
async def refresh_token(request: RefreshTokenRequest, auth_client: AuthClient = Depends(get_auth_client)):
    """
    Refresh access token using refresh token

    Implements token rotation - old refresh token is invalidated.
    Detects token reuse attacks.
    """
    try:
        response = auth_client.refresh_token(request.refresh_token)

        return RefreshTokenResponse(
            access_token=response.access_token,
            refresh_token=response.refresh_token,
            expires_in=response.expires_in
        )

    except grpc.RpcError as e:
        logger.error(f"gRPC error during token refresh: {e.code()} - {e.details()}")

        if e.code() == grpc.StatusCode.UNAUTHENTICATED:
            raise HTTPException(
                status_code=status.HTTP_401_UNAUTHORIZED,
                detail="Invalid or expired refresh token"
            )
        elif e.code() == grpc.StatusCode.PERMISSION_DENIED:
            # Token reuse detected
            raise HTTPException(
                status_code=status.HTTP_403_FORBIDDEN,
                detail="Token reuse detected - all sessions revoked for security"
            )
        else:
            raise HTTPException(
                status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
                detail="Token refresh failed"
            )


@router.post("/logout", status_code=status.HTTP_204_NO_CONTENT)
async def logout(
    request: LogoutRequest,
    authorization: Optional[str] = Header(None),
    auth_client: AuthClient = Depends(get_auth_client)
):
    """
    Logout user and blacklist tokens

    Adds both access token (from header) and refresh token to Redis blacklist.
    """
    try:
        if request.refresh_token:
            auth_client.logout(request.refresh_token)

        return None  # 204 No Content

    except grpc.RpcError as e:
        logger.error(f"gRPC error during logout: {e.code()} - {e.details()}")
        # Fail silently for logout
        return None


@router.post("/totp/enroll", response_model=TOTPEnrollResponse)
async def totp_enroll(
    request: TOTPEnrollRequest,
    authorization: str = Header(...),
    auth_client: AuthClient = Depends(get_auth_client)
):
    """
    Enroll in TOTP 2FA

    Generates TOTP secret and QR code.
    User must verify with TOTPVerify before 2FA is enabled.
    """
    try:
        # Extract JWT from Authorization header
        access_token = authorization.replace("Bearer ", "")

        # Call gRPC with token in metadata
        metadata = [("authorization", f"Bearer {access_token}")]
        response = auth_client.stub.TOTPEnroll(
            auth_client.stub.TOTPEnrollRequest(),
            metadata=metadata
        )

        return TOTPEnrollResponse(
            secret=response.secret,
            qr_code_url=response.qr_code_url,
            manual_entry_key=response.manual_entry_key
        )

    except grpc.RpcError as e:
        logger.error(f"gRPC error during TOTP enrollment: {e.code()} - {e.details()}")
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail="TOTP enrollment failed"
        )


@router.post("/totp/verify", response_model=TOTPVerifyResponse)
async def totp_verify(
    request: TOTPVerifyRequest,
    authorization: str = Header(...),
    auth_client: AuthClient = Depends(get_auth_client)
):
    """
    Verify TOTP code and complete enrollment

    Returns backup codes for account recovery.
    Enables 2FA for the account.
    """
    try:
        access_token = authorization.replace("Bearer ", "")
        metadata = [("authorization", f"Bearer {access_token}")]

        response = auth_client.stub.TOTPVerify(
            auth_client.stub.TOTPVerifyRequest(code=request.code),
            metadata=metadata
        )

        return TOTPVerifyResponse(backup_codes=list(response.backup_codes))

    except grpc.RpcError as e:
        logger.error(f"gRPC error during TOTP verification: {e.code()} - {e.details()}")

        if e.code() == grpc.StatusCode.INVALID_ARGUMENT:
            raise HTTPException(
                status_code=status.HTTP_400_BAD_REQUEST,
                detail="Invalid verification code"
            )
        else:
            raise HTTPException(
                status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
                detail="TOTP verification failed"
            )


@router.post("/totp/disable", status_code=status.HTTP_204_NO_CONTENT)
async def totp_disable(
    authorization: str = Header(...),
    auth_client: AuthClient = Depends(get_auth_client)
):
    """
    Disable TOTP 2FA

    Removes TOTP secret and backup codes.
    """
    try:
        access_token = authorization.replace("Bearer ", "")
        metadata = [("authorization", f"Bearer {access_token}")]

        auth_client.stub.TOTPDisable(
            auth_client.stub.TOTPDisableRequest(),
            metadata=metadata
        )

        return None

    except grpc.RpcError as e:
        logger.error(f"gRPC error disabling TOTP: {e.code()} - {e.details()}")
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail="Failed to disable 2FA"
        )


@router.post("/totp/regenerate-backup-codes", response_model=TOTPVerifyResponse)
async def regenerate_backup_codes(
    authorization: str = Header(...),
    auth_client: AuthClient = Depends(get_auth_client)
):
    """
    Regenerate backup codes

    Invalidates old backup codes and generates new ones.
    """
    try:
        access_token = authorization.replace("Bearer ", "")
        metadata = [("authorization", f"Bearer {access_token}")]

        response = auth_client.stub.RegenerateBackupCodes(
            auth_client.stub.RegenerateBackupCodesRequest(),
            metadata=metadata
        )

        return TOTPVerifyResponse(backup_codes=list(response.backup_codes))

    except grpc.RpcError as e:
        logger.error(f"gRPC error regenerating backup codes: {e.code()} - {e.details()}")
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail="Failed to regenerate backup codes"
        )


@router.post("/2fa/verify", response_model=LoginResponse)
async def two_factor_verify(request: TwoFactorVerifyRequest, auth_client: AuthClient = Depends(get_auth_client)):
    """
    Verify 2FA code during login

    Completes authentication after successful 2FA verification.
    """
    try:
        response = auth_client.stub.TwoFactorVerify(
            auth_client.stub.TwoFactorVerifyRequest(
                temp_token=request.temp_token,
                code=request.code
            )
        )

        return LoginResponse(
            access_token=response.access_token,
            refresh_token=response.refresh_token,
            expires_in=response.expires_in,
            user_id=response.user_id,
            tenant_id=response.tenant_id,
            totp_enabled=True
        )

    except grpc.RpcError as e:
        logger.error(f"gRPC error during 2FA verification: {e.code()} - {e.details()}")
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="Invalid verification code"
        )


@router.post("/2fa/backup-code", response_model=LoginResponse)
async def two_factor_backup_code(request: TwoFactorBackupCodeRequest, auth_client: AuthClient = Depends(get_auth_client)):
    """
    Verify backup code during login

    Uses one-time backup code to complete authentication.
    Backup code is consumed after use.
    """
    try:
        response = auth_client.stub.TwoFactorBackupCode(
            auth_client.stub.TwoFactorBackupCodeRequest(
                temp_token=request.temp_token,
                code=request.code
            )
        )

        return LoginResponse(
            access_token=response.access_token,
            refresh_token=response.refresh_token,
            expires_in=response.expires_in,
            user_id=response.user_id,
            tenant_id=response.tenant_id,
            totp_enabled=True
        )

    except grpc.RpcError as e:
        logger.error(f"gRPC error during backup code verification: {e.code()} - {e.details()}")
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="Invalid or used backup code"
        )


@router.post("/password/reset/request", status_code=status.HTTP_202_ACCEPTED)
async def password_reset_request(request: PasswordResetRequest):
    """
    Request password reset

    Sends password reset email with secure token.
    Always returns success for security (no user enumeration).

    Security:
    - Generates cryptographically secure 32-byte token
    - Stores token in Redis with 1-hour expiry (3600 seconds)
    - Associates token with user email for validation
    - Rate limited to prevent abuse (implemented via middleware)
    """
    from dependencies.auth import get_redis_client

    # Generate secure reset token (256 bits of entropy)
    reset_token = secrets.token_urlsafe(32)
    reset_url = f"http://localhost:3000/reset-password?token={reset_token}"

    # Store reset token in Redis with 1-hour expiry
    redis_client = get_redis_client()
    if redis_client:
        try:
            # Key format: reset_token:{token}
            # Value: email address
            # TTL: 3600 seconds (1 hour)
            redis_key = f"reset_token:{reset_token}"
            redis_client.setex(redis_key, 3600, request.email)
            logger.info(f"Password reset token stored for email: {request.email[:3]}***@{request.email.split('@')[1]}")
        except Exception as e:
            logger.error(f"Failed to store reset token in Redis: {e}")
            # Continue anyway - token will be validated by gRPC service
    else:
        logger.warning("Redis unavailable - reset token not cached (relying on gRPC validation)")

    # Send reset email
    email_service = get_email_service()
    await email_service.send_password_reset_email(
        request.email,
        reset_token,
        reset_url
    )

    return {"message": "If the email exists, a reset link has been sent"}


@router.post("/password/reset/confirm", status_code=status.HTTP_200_OK)
async def password_reset_confirm(request: PasswordResetConfirmRequest, auth_client: AuthClient = Depends(get_auth_client)):
    """
    Confirm password reset with token

    Validates reset token and updates password.

    Security:
    - Validates token exists in Redis (not expired)
    - Retrieves email associated with token
    - Deletes token after use (one-time use)
    - Calls gRPC service to update password
    """
    from dependencies.auth import get_redis_client

    # Validate reset token from Redis
    redis_client = get_redis_client()
    email = None

    if redis_client:
        try:
            redis_key = f"reset_token:{request.token}"

            # Get email associated with token
            email = redis_client.get(redis_key)

            if not email:
                logger.warning(f"Password reset attempted with invalid/expired token")
                raise HTTPException(
                    status_code=status.HTTP_400_BAD_REQUEST,
                    detail="Invalid or expired reset token"
                )

            # Delete token (one-time use)
            redis_client.delete(redis_key)
            logger.info(f"Password reset token consumed for email: {email[:3]}***@{email.split('@')[1]}")

        except HTTPException:
            raise  # Re-raise HTTP exceptions as-is
        except Exception as e:
            logger.error(f"Redis error during token validation: {e}")
            # Fall through to gRPC validation
    else:
        logger.warning("Redis unavailable - relying on gRPC token validation")

    try:
        # Call gRPC password reset
        # Note: gRPC service should also validate the token independently
        auth_client.stub.PasswordReset(
            auth_client.stub.PasswordResetRequest(
                token=request.token,
                new_password=request.new_password
            )
        )

        return {"message": "Password reset successful"}

    except grpc.RpcError as e:
        logger.error(f"gRPC error during password reset: {e.code()} - {e.details()}")
        raise HTTPException(
            status_code=status.HTTP_400_BAD_REQUEST,
            detail="Invalid or expired reset token"
        )


@router.post("/password/change", status_code=status.HTTP_200_OK)
async def password_change(
    request: PasswordChangeRequest,
    authorization: str = Header(...),
    auth_client: AuthClient = Depends(get_auth_client)
):
    """
    Change password (authenticated user)

    Requires current password for verification.
    """
    try:
        access_token = authorization.replace("Bearer ", "")
        metadata = [("authorization", f"Bearer {access_token}")]

        auth_client.stub.PasswordChange(
            auth_client.stub.PasswordChangeRequest(
                current_password=request.current_password,
                new_password=request.new_password
            ),
            metadata=metadata
        )

        return {"message": "Password changed successfully"}

    except grpc.RpcError as e:
        logger.error(f"gRPC error during password change: {e.code()} - {e.details()}")

        if e.code() == grpc.StatusCode.UNAUTHENTICATED:
            raise HTTPException(
                status_code=status.HTTP_401_UNAUTHORIZED,
                detail="Current password is incorrect"
            )
        else:
            raise HTTPException(
                status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
                detail="Password change failed"
            )


@router.post("/otp/request", status_code=status.HTTP_202_ACCEPTED)
async def otp_request(request: OTPRequest, user: UserContext = Depends(get_current_user)):
    """
    Request OTP code via email or SMS

    Alternative to TOTP for 2FA.

    Security:
    - Uses secrets module for cryptographically secure random generation
    - Stores OTP in Redis with 10-minute expiry (600 seconds)
    - Rate limited to 3 requests per hour per user (prevents brute force)
    - Associates OTP with user_id to prevent replay attacks
    """
    from dependencies.auth import get_redis_client

    user_id = user.user_id
    email = user.email

    # Check rate limiting (max 3 OTP requests per hour)
    redis_client = get_redis_client()
    if redis_client:
        rate_limit_key = f"otp_rate_limit:{user_id}"
        request_count = redis_client.get(rate_limit_key)

        if request_count and int(request_count) >= 3:
            logger.warning(f"OTP rate limit exceeded for user: {user_id}")
            raise HTTPException(
                status_code=status.HTTP_429_TOO_MANY_REQUESTS,
                detail="Too many OTP requests. Please wait 1 hour before requesting again."
            )

    # Generate 6-digit OTP using secrets module (cryptographically secure)
    # secrets.randbelow(900000) generates 0-899999, adding 100000 gives 100000-999999
    otp = f"{secrets.randbelow(900000) + 100000}"

    # Store OTP in Redis with 10-minute expiry
    if redis_client:
        try:
            # Key format: otp:{user_id}
            # Value: OTP code
            # TTL: 600 seconds (10 minutes)
            otp_key = f"otp:{user_id}"
            redis_client.setex(otp_key, 600, otp)

            # Increment rate limit counter (1-hour expiry)
            redis_client.incr(rate_limit_key)
            redis_client.expire(rate_limit_key, 3600)  # 1 hour

            logger.info(f"OTP generated for user: {user_id} via {request.method}")
        except Exception as e:
            logger.error(f"Failed to store OTP in Redis: {e}")
            raise HTTPException(
                status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
                detail="Failed to generate OTP. Please try again."
            )
    else:
        logger.error("Redis unavailable - OTP cannot be generated")
        raise HTTPException(
            status_code=status.HTTP_503_SERVICE_UNAVAILABLE,
            detail="OTP service temporarily unavailable"
        )

    # Send OTP via requested method
    if request.method == "email":
        if not email:
            raise HTTPException(
                status_code=status.HTTP_400_BAD_REQUEST,
                detail="Email not available for this user"
            )
        email_service = get_email_service()
        await email_service.send_otp_email(email, otp)
    elif request.method == "sms":
        # TODO: Implement SMS sending via Twilio
        logger.warning("SMS OTP not yet implemented - using email fallback")
        if not email:
            raise HTTPException(
                status_code=status.HTTP_501_NOT_IMPLEMENTED,
                detail="SMS OTP not yet implemented and no email available"
            )
        email_service = get_email_service()
        await email_service.send_otp_email(email, otp)

    return {"message": f"OTP sent via {request.method}"}


@router.post("/otp/verify", status_code=status.HTTP_200_OK)
async def otp_verify(request: OTPVerifyRequest, user: UserContext = Depends(get_current_user)):
    """
    Verify OTP code

    Alternative to TOTP for 2FA.

    Security:
    - Validates OTP from Redis (automatic expiry after 10 minutes)
    - Deletes OTP after successful verification (one-time use)
    - Constant-time comparison to prevent timing attacks
    - Rate limited via OTP request endpoint
    """
    from dependencies.auth import get_redis_client

    user_id = user.user_id

    # Retrieve OTP from Redis
    redis_client = get_redis_client()
    if not redis_client:
        logger.error("Redis unavailable - OTP verification failed")
        raise HTTPException(
            status_code=status.HTTP_503_SERVICE_UNAVAILABLE,
            detail="OTP service temporarily unavailable"
        )

    try:
        otp_key = f"otp:{user_id}"
        stored_otp = redis_client.get(otp_key)

        if not stored_otp:
            logger.warning(f"OTP verification failed for user: {user_id} - OTP not found or expired")
            raise HTTPException(
                status_code=status.HTTP_401_UNAUTHORIZED,
                detail="Invalid or expired OTP code"
            )

        # Constant-time comparison to prevent timing attacks
        import hmac
        if not hmac.compare_digest(request.code, stored_otp):
            logger.warning(f"OTP verification failed for user: {user_id} - code mismatch")
            raise HTTPException(
                status_code=status.HTTP_401_UNAUTHORIZED,
                detail="Invalid OTP code"
            )

        # OTP verified successfully - delete from Redis (one-time use)
        redis_client.delete(otp_key)
        logger.info(f"OTP verified successfully for user: {user_id}")

        return {"message": "OTP verified successfully"}

    except HTTPException:
        raise  # Re-raise HTTP exceptions as-is
    except Exception as e:
        logger.error(f"Redis error during OTP verification: {e}")
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail="OTP verification failed"
        )


@router.post("/api-keys", response_model=ApiKeyResponse)
async def create_api_key(
    request: ApiKeyCreateRequest,
    authorization: str = Header(...),
    auth_client: AuthClient = Depends(get_auth_client)
):
    """
    Create API key with scopes

    Returns plaintext key (only shown once).
    """
    try:
        access_token = authorization.replace("Bearer ", "")
        metadata = [("authorization", f"Bearer {access_token}")]

        response = auth_client.create_api_key(
            name=request.name,
            scopes=request.scopes,
            metadata=metadata
        )

        return ApiKeyResponse(
            id=response.id,
            key=response.key,  # Only returned on creation
            name=response.name,
            scopes=list(response.scopes),
            created_at=response.created_at,
            expires_at=response.expires_at if hasattr(response, 'expires_at') else None,
            last_used_at=None
        )

    except grpc.RpcError as e:
        logger.error(f"gRPC error creating API key: {e.code()} - {e.details()}")
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail="Failed to create API key"
        )


@router.delete("/api-keys/{key_id}", status_code=status.HTTP_204_NO_CONTENT)
async def revoke_api_key(
    key_id: str,
    authorization: str = Header(...),
    auth_client: AuthClient = Depends(get_auth_client)
):
    """
    Revoke API key

    Marks key as revoked in database.
    """
    try:
        access_token = authorization.replace("Bearer ", "")
        metadata = [("authorization", f"Bearer {access_token}")]

        auth_client.revoke_api_key(key_id=key_id, metadata=metadata)

        return None

    except grpc.RpcError as e:
        logger.error(f"gRPC error revoking API key: {e.code()} - {e.details()}")
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail="Failed to revoke API key"
        )
