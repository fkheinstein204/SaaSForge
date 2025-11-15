"""
@file        email.py
@brief       Email management and queue status endpoints
@copyright   (c) 2025 FtsCoDe GmbH. All rights reserved.
@author      Heinstein F.
@date        2025-11-15
"""

from fastapi import APIRouter, HTTPException, status, Depends
from pydantic import BaseModel, EmailStr
from typing import Dict, Optional
from services.email_service import get_email_service, EmailPriority
from workers.email_worker import EmailQueueWorker

router = APIRouter()


class SendEmailRequest(BaseModel):
    """Request model for sending custom emails"""
    to_email: EmailStr
    subject: str
    html_content: str
    plain_content: Optional[str] = None
    priority: EmailPriority = EmailPriority.NORMAL


class EmailQueueStats(BaseModel):
    """Email queue statistics"""
    critical: int
    high: int
    normal: int
    low: int
    dead_letter: int
    total: int


@router.post("/send", status_code=status.HTTP_202_ACCEPTED)
async def send_email(request: SendEmailRequest):
    """
    Send a custom email (admin endpoint - should be protected)

    This endpoint allows administrators to send custom emails.
    In production, this should require admin authentication.
    """
    email_service = get_email_service()

    success = await email_service.send_email(
        to_email=request.to_email,
        subject=request.subject,
        html_content=request.html_content,
        plain_content=request.plain_content,
        priority=request.priority,
    )

    if not success:
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail="Failed to send email"
        )

    return {
        "message": "Email queued successfully",
        "priority": request.priority.value
    }


@router.get("/queue/stats", response_model=EmailQueueStats)
async def get_queue_stats():
    """
    Get email queue statistics

    Returns the number of emails in each priority queue and dead letter queue.
    """
    worker = EmailQueueWorker()
    stats = await worker.get_queue_stats()

    total = sum(stats.values())

    return EmailQueueStats(
        critical=stats.get("critical", 0),
        high=stats.get("high", 0),
        normal=stats.get("normal", 0),
        low=stats.get("low", 0),
        dead_letter=stats.get("dead_letter", 0),
        total=total
    )


@router.post("/test/otp")
async def send_test_otp(email: EmailStr):
    """
    Send a test OTP email (development/testing only)

    This endpoint should be disabled in production or protected with authentication.
    """
    email_service = get_email_service()

    # Generate test OTP
    import random
    otp = f"{random.randint(100000, 999999)}"

    success = await email_service.send_otp_email(email, otp)

    if not success:
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail="Failed to send OTP email"
        )

    return {
        "message": "Test OTP email sent",
        "otp": otp,  # Only for testing - never return OTP in production!
        "email": email
    }


@router.post("/test/password-reset")
async def send_test_password_reset(email: EmailStr):
    """
    Send a test password reset email (development/testing only)

    This endpoint should be disabled in production or protected with authentication.
    """
    email_service = get_email_service()

    # Generate test reset token
    import secrets
    reset_token = secrets.token_urlsafe(32)
    reset_url = f"http://localhost:3000/reset-password?token={reset_token}"

    success = await email_service.send_password_reset_email(email, reset_token, reset_url)

    if not success:
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail="Failed to send password reset email"
        )

    return {
        "message": "Test password reset email sent",
        "reset_token": reset_token,  # Only for testing!
        "reset_url": reset_url,
        "email": email
    }


@router.post("/test/welcome")
async def send_test_welcome(email: EmailStr, name: str = "User"):
    """
    Send a test welcome email (development/testing only)

    This endpoint should be disabled in production or protected with authentication.
    """
    email_service = get_email_service()

    success = await email_service.send_welcome_email(email, name)

    if not success:
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail="Failed to send welcome email"
        )

    return {
        "message": "Test welcome email sent",
        "email": email,
        "name": name
    }
