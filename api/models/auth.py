"""
Authentication models
"""
from pydantic import BaseModel, EmailStr, Field
from typing import Optional, List
from datetime import datetime


class LoginRequest(BaseModel):
    email: EmailStr
    password: str
    totp_code: Optional[str] = None  # For 2FA


class LoginResponse(BaseModel):
    access_token: str
    refresh_token: str
    token_type: str = "Bearer"
    expires_in: int  # seconds
    user_id: str
    tenant_id: str
    totp_enabled: bool = False


class RefreshTokenRequest(BaseModel):
    refresh_token: str


class RefreshTokenResponse(BaseModel):
    access_token: str
    refresh_token: str
    token_type: str = "Bearer"
    expires_in: int


class LogoutRequest(BaseModel):
    refresh_token: Optional[str] = None


class ApiKeyCreateRequest(BaseModel):
    name: str = Field(..., min_length=1, max_length=100)
    scopes: List[str] = Field(..., min_items=1)
    expires_at: Optional[datetime] = None


class ApiKeyResponse(BaseModel):
    id: str
    key: Optional[str] = None  # Only returned on creation
    name: str
    scopes: List[str]
    created_at: datetime
    expires_at: Optional[datetime]
    last_used_at: Optional[datetime]


class RegisterRequest(BaseModel):
    email: EmailStr
    password: str = Field(..., min_length=12)
    full_name: str = Field(..., min_length=1, max_length=255)


class RegisterResponse(BaseModel):
    access_token: str
    refresh_token: str
    token_type: str = "Bearer"
    expires_in: int
    user_id: str
    tenant_id: str


class TOTPEnrollRequest(BaseModel):
    """Request to enroll in TOTP 2FA"""
    pass


class TOTPEnrollResponse(BaseModel):
    """Response with TOTP secret and QR code"""
    secret: str
    qr_code_url: str
    manual_entry_key: str


class TOTPVerifyRequest(BaseModel):
    """Verify TOTP code during enrollment"""
    code: str = Field(..., min_length=6, max_length=6)


class TOTPVerifyResponse(BaseModel):
    """Response after successful TOTP verification"""
    backup_codes: List[str]


class TwoFactorVerifyRequest(BaseModel):
    """Verify 2FA code during login"""
    temp_token: str
    code: str = Field(..., min_length=6, max_length=6)


class TwoFactorBackupCodeRequest(BaseModel):
    """Verify backup code during login"""
    temp_token: str
    code: str = Field(..., min_length=8, max_length=8)


class PasswordResetRequest(BaseModel):
    """Request password reset"""
    email: EmailStr


class PasswordResetConfirmRequest(BaseModel):
    """Confirm password reset with token"""
    token: str
    new_password: str = Field(..., min_length=12)


class PasswordChangeRequest(BaseModel):
    """Change password (authenticated)"""
    current_password: str
    new_password: str = Field(..., min_length=12)


class OTPRequest(BaseModel):
    """Request OTP code"""
    method: str = Field(..., regex="^(email|sms)$")


class OTPVerifyRequest(BaseModel):
    """Verify OTP code"""
    code: str = Field(..., min_length=6, max_length=6)
