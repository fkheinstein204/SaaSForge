"""
@file        email_service.py
@brief       Email service using SendGrid with queue management
@copyright   (c) 2025 FtsCoDe GmbH. All rights reserved.
@author      Heinstein F.
@date        2025-11-15
"""

import os
import logging
from typing import Optional, Dict, Any
from enum import Enum
from datetime import datetime, timedelta
import asyncio
from sendgrid import SendGridAPIClient
from sendgrid.helpers.mail import Mail, Email, To, Content
from redis import Redis

logger = logging.getLogger(__name__)


class EmailPriority(str, Enum):
    """Email priority levels with SLA targets"""
    CRITICAL = "critical"  # 30s SLA - OTP, password reset
    HIGH = "high"          # 5min SLA - transactional emails
    NORMAL = "normal"      # 30min SLA - notifications
    LOW = "low"            # best effort - marketing


class EmailService:
    """
    Email service with SendGrid integration and Redis-backed queue

    Features:
    - Priority-based email queue (critical, high, normal, low)
    - Retry logic with exponential backoff
    - Template support
    - Bounce tracking
    - Rate limiting
    """

    def __init__(self):
        self.api_key = os.getenv("SENDGRID_API_KEY")
        if not self.api_key:
            logger.warning("SENDGRID_API_KEY not set - email sending will fail")

        self.client = SendGridAPIClient(self.api_key) if self.api_key else None
        self.from_email = os.getenv("SENDGRID_FROM_EMAIL", "noreply@saasforge.com")
        self.from_name = os.getenv("SENDGRID_FROM_NAME", "SaaSForge")

        # Redis for email queue
        redis_host = os.getenv("REDIS_HOST", "localhost")
        redis_port = int(os.getenv("REDIS_PORT", "6379"))
        self.redis = Redis(host=redis_host, port=redis_port, decode_responses=True)

        # Queue configuration
        self.queue_keys = {
            EmailPriority.CRITICAL: "email:queue:critical",
            EmailPriority.HIGH: "email:queue:high",
            EmailPriority.NORMAL: "email:queue:normal",
            EmailPriority.LOW: "email:queue:low",
        }

        # Rate limiting (SendGrid free tier: 100/day)
        self.rate_limit_key = "email:rate_limit"
        self.daily_limit = int(os.getenv("SENDGRID_DAILY_LIMIT", "100"))

    async def send_email(
        self,
        to_email: str,
        subject: str,
        html_content: str,
        plain_content: Optional[str] = None,
        priority: EmailPriority = EmailPriority.NORMAL,
        template_id: Optional[str] = None,
        template_data: Optional[Dict[str, Any]] = None,
    ) -> bool:
        """
        Send email immediately or queue based on priority

        Args:
            to_email: Recipient email address
            subject: Email subject
            html_content: HTML email body
            plain_content: Plain text fallback (optional)
            priority: Email priority level
            template_id: SendGrid template ID (optional)
            template_data: Template variables (optional)

        Returns:
            True if sent/queued successfully, False otherwise
        """

        # Check rate limit
        if not self._check_rate_limit():
            logger.error(f"Daily email limit ({self.daily_limit}) reached")
            return False

        # For critical priority, send immediately
        if priority == EmailPriority.CRITICAL:
            return await self._send_now(
                to_email, subject, html_content, plain_content,
                template_id, template_data
            )

        # For other priorities, queue the email
        return await self._queue_email(
            to_email, subject, html_content, plain_content,
            priority, template_id, template_data
        )

    async def _send_now(
        self,
        to_email: str,
        subject: str,
        html_content: str,
        plain_content: Optional[str] = None,
        template_id: Optional[str] = None,
        template_data: Optional[Dict[str, Any]] = None,
    ) -> bool:
        """Send email immediately via SendGrid"""

        if not self.client:
            logger.error("SendGrid client not initialized - check SENDGRID_API_KEY")
            return False

        try:
            message = Mail(
                from_email=Email(self.from_email, self.from_name),
                to_emails=To(to_email),
                subject=subject,
                html_content=Content("text/html", html_content),
            )

            if plain_content:
                message.add_content(Content("text/plain", plain_content))

            # Use template if provided
            if template_id:
                message.template_id = template_id
                if template_data:
                    message.dynamic_template_data = template_data

            response = self.client.send(message)

            if response.status_code in (200, 201, 202):
                logger.info(f"Email sent successfully to {to_email} (status: {response.status_code})")
                self._increment_rate_limit()
                return True
            else:
                logger.error(f"Failed to send email: {response.status_code} - {response.body}")
                return False

        except Exception as e:
            logger.error(f"Exception sending email to {to_email}: {str(e)}")
            return False

    async def _queue_email(
        self,
        to_email: str,
        subject: str,
        html_content: str,
        plain_content: Optional[str] = None,
        priority: EmailPriority = EmailPriority.NORMAL,
        template_id: Optional[str] = None,
        template_data: Optional[Dict[str, Any]] = None,
    ) -> bool:
        """Add email to Redis queue for background processing"""

        import json

        email_data = {
            "to_email": to_email,
            "subject": subject,
            "html_content": html_content,
            "plain_content": plain_content,
            "template_id": template_id,
            "template_data": template_data,
            "queued_at": datetime.utcnow().isoformat(),
            "retry_count": 0,
        }

        queue_key = self.queue_keys[priority]

        try:
            # Add to Redis list (LPUSH for FIFO with RPOP)
            self.redis.lpush(queue_key, json.dumps(email_data))
            logger.info(f"Email queued ({priority}) to {to_email}: {subject}")
            return True
        except Exception as e:
            logger.error(f"Failed to queue email: {str(e)}")
            return False

    def _check_rate_limit(self) -> bool:
        """Check if we're within daily sending limit"""
        try:
            count = self.redis.get(self.rate_limit_key)
            if count and int(count) >= self.daily_limit:
                return False
            return True
        except Exception as e:
            logger.error(f"Rate limit check failed: {str(e)}")
            # Fail open - allow sending if Redis is down
            return True

    def _increment_rate_limit(self):
        """Increment daily send counter"""
        try:
            # Increment counter with 24-hour expiry
            pipe = self.redis.pipeline()
            pipe.incr(self.rate_limit_key)
            pipe.expire(self.rate_limit_key, 86400)  # 24 hours
            pipe.execute()
        except Exception as e:
            logger.error(f"Failed to increment rate limit: {str(e)}")

    async def send_otp_email(self, to_email: str, otp_code: str) -> bool:
        """Send OTP verification email (critical priority)"""
        subject = "Your SaaSForge Verification Code"
        html_content = f"""
        <html>
        <body style="font-family: Arial, sans-serif; max-width: 600px; margin: 0 auto;">
            <div style="background: linear-gradient(135deg, #0A2540, #1e4d7b); padding: 30px; text-align: center;">
                <h1 style="color: white; margin: 0;">SaaSForge</h1>
            </div>
            <div style="padding: 40px 30px;">
                <h2 style="color: #0A2540;">Your Verification Code</h2>
                <p style="color: #556B7D; font-size: 16px;">
                    Use the following code to complete your verification:
                </p>
                <div style="background: #F7F9FA; border-left: 4px solid #FF6B35; padding: 20px; margin: 20px 0;">
                    <h1 style="color: #0A2540; margin: 0; font-size: 36px; letter-spacing: 5px; font-family: monospace;">
                        {otp_code}
                    </h1>
                </div>
                <p style="color: #556B7D; font-size: 14px;">
                    This code will expire in <strong>10 minutes</strong>.
                </p>
                <p style="color: #556B7D; font-size: 14px;">
                    If you didn't request this code, please ignore this email.
                </p>
            </div>
            <div style="background: #F7F9FA; padding: 20px; text-align: center; color: #556B7D; font-size: 12px;">
                <p>© 2025 SaaSForge. All rights reserved.</p>
            </div>
        </body>
        </html>
        """

        plain_content = f"""
        Your SaaSForge Verification Code

        Use the following code to complete your verification: {otp_code}

        This code will expire in 10 minutes.

        If you didn't request this code, please ignore this email.

        © 2025 SaaSForge. All rights reserved.
        """

        return await self.send_email(
            to_email=to_email,
            subject=subject,
            html_content=html_content,
            plain_content=plain_content,
            priority=EmailPriority.CRITICAL
        )

    async def send_password_reset_email(self, to_email: str, reset_token: str, reset_url: str) -> bool:
        """Send password reset email (critical priority)"""
        subject = "Reset Your SaaSForge Password"
        html_content = f"""
        <html>
        <body style="font-family: Arial, sans-serif; max-width: 600px; margin: 0 auto;">
            <div style="background: linear-gradient(135deg, #0A2540, #1e4d7b); padding: 30px; text-align: center;">
                <h1 style="color: white; margin: 0;">SaaSForge</h1>
            </div>
            <div style="padding: 40px 30px;">
                <h2 style="color: #0A2540;">Reset Your Password</h2>
                <p style="color: #556B7D; font-size: 16px;">
                    We received a request to reset your password. Click the button below to choose a new password:
                </p>
                <div style="text-align: center; margin: 30px 0;">
                    <a href="{reset_url}" style="background: #FF6B35; color: white; padding: 15px 40px; text-decoration: none; border-radius: 5px; display: inline-block; font-weight: bold;">
                        Reset Password
                    </a>
                </div>
                <p style="color: #556B7D; font-size: 14px;">
                    This link will expire in <strong>1 hour</strong>.
                </p>
                <p style="color: #556B7D; font-size: 14px;">
                    If you didn't request a password reset, please ignore this email or contact support if you have concerns.
                </p>
                <p style="color: #556B7D; font-size: 12px; margin-top: 30px;">
                    If the button doesn't work, copy and paste this link into your browser:<br>
                    <a href="{reset_url}" style="color: #FF6B35;">{reset_url}</a>
                </p>
            </div>
            <div style="background: #F7F9FA; padding: 20px; text-align: center; color: #556B7D; font-size: 12px;">
                <p>© 2025 SaaSForge. All rights reserved.</p>
            </div>
        </body>
        </html>
        """

        plain_content = f"""
        Reset Your SaaSForge Password

        We received a request to reset your password. Visit this link to choose a new password:

        {reset_url}

        This link will expire in 1 hour.

        If you didn't request a password reset, please ignore this email or contact support if you have concerns.

        © 2025 SaaSForge. All rights reserved.
        """

        return await self.send_email(
            to_email=to_email,
            subject=subject,
            html_content=html_content,
            plain_content=plain_content,
            priority=EmailPriority.CRITICAL
        )

    async def send_welcome_email(self, to_email: str, user_name: str) -> bool:
        """Send welcome email to new users (normal priority)"""
        subject = "Welcome to SaaSForge!"
        html_content = f"""
        <html>
        <body style="font-family: Arial, sans-serif; max-width: 600px; margin: 0 auto;">
            <div style="background: linear-gradient(135deg, #0A2540, #1e4d7b); padding: 30px; text-align: center;">
                <h1 style="color: white; margin: 0;">Welcome to SaaSForge!</h1>
            </div>
            <div style="padding: 40px 30px;">
                <h2 style="color: #0A2540;">Hi {user_name},</h2>
                <p style="color: #556B7D; font-size: 16px;">
                    Thank you for joining SaaSForge! We're excited to have you on board.
                </p>
                <p style="color: #556B7D; font-size: 16px;">
                    Here's what you can do next:
                </p>
                <ul style="color: #556B7D; font-size: 16px;">
                    <li>Enable two-factor authentication for extra security</li>
                    <li>Upload your first files</li>
                    <li>Explore our subscription plans</li>
                    <li>Set up webhooks for notifications</li>
                </ul>
                <div style="text-align: center; margin: 30px 0;">
                    <a href="https://saasforge.com/dashboard" style="background: #FF6B35; color: white; padding: 15px 40px; text-decoration: none; border-radius: 5px; display: inline-block; font-weight: bold;">
                        Get Started
                    </a>
                </div>
                <p style="color: #556B7D; font-size: 14px;">
                    If you have any questions, feel free to reach out to our support team.
                </p>
            </div>
            <div style="background: #F7F9FA; padding: 20px; text-align: center; color: #556B7D; font-size: 12px;">
                <p>© 2025 SaaSForge. All rights reserved.</p>
            </div>
        </body>
        </html>
        """

        plain_content = f"""
        Welcome to SaaSForge!

        Hi {user_name},

        Thank you for joining SaaSForge! We're excited to have you on board.

        Here's what you can do next:
        - Enable two-factor authentication for extra security
        - Upload your first files
        - Explore our subscription plans
        - Set up webhooks for notifications

        Visit your dashboard: https://saasforge.com/dashboard

        If you have any questions, feel free to reach out to our support team.

        © 2025 SaaSForge. All rights reserved.
        """

        return await self.send_email(
            to_email=to_email,
            subject=subject,
            html_content=html_content,
            plain_content=plain_content,
            priority=EmailPriority.NORMAL
        )


# Singleton instance
_email_service = None

def get_email_service() -> EmailService:
    """Get or create EmailService singleton"""
    global _email_service
    if _email_service is None:
        _email_service = EmailService()
    return _email_service
