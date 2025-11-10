"""
Notification models
"""
from pydantic import BaseModel, EmailStr, HttpUrl, Field
from typing import Optional, Dict, List
from datetime import datetime
from enum import Enum


class NotificationChannel(str, Enum):
    EMAIL = "email"
    SMS = "sms"
    PUSH = "push"
    WEBHOOK = "webhook"


class NotificationStatus(str, Enum):
    PENDING = "pending"
    SENT = "sent"
    DELIVERED = "delivered"
    FAILED = "failed"
    BOUNCED = "bounced"


class SendEmailRequest(BaseModel):
    to: EmailStr
    subject: str = Field(..., min_length=1, max_length=200)
    body_html: str
    body_text: Optional[str] = None
    template_id: Optional[str] = None
    template_vars: Optional[Dict[str, str]] = None


class SendSMSRequest(BaseModel):
    to: str = Field(..., pattern=r'^\+[1-9]\d{1,14}$')  # E.164 format
    message: str = Field(..., min_length=1, max_length=1600)


class SendPushRequest(BaseModel):
    user_id: str
    title: str = Field(..., max_length=100)
    body: str = Field(..., max_length=500)
    data: Optional[Dict[str, str]] = None


class NotificationResponse(BaseModel):
    id: str
    channel: NotificationChannel
    status: NotificationStatus
    created_at: datetime
    sent_at: Optional[datetime]
    delivered_at: Optional[datetime]
    retry_count: int


class WebhookRequest(BaseModel):
    url: HttpUrl
    events: List[str] = Field(..., min_items=1)
    secret: Optional[str] = None  # For signature verification


class WebhookResponse(BaseModel):
    id: str
    url: str
    events: List[str]
    status: str
    created_at: datetime
    last_triggered_at: Optional[datetime]
    failure_count: int


class PreferencesRequest(BaseModel):
    email_enabled: bool = True
    sms_enabled: bool = True
    push_enabled: bool = True
    marketing_emails: bool = False


class PreferencesResponse(BaseModel):
    user_id: str
    email_enabled: bool
    sms_enabled: bool
    push_enabled: bool
    marketing_emails: bool
    updated_at: datetime
