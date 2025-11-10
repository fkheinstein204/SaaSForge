"""
Payment models
"""
from pydantic import BaseModel, Field
from typing import Optional, List
from datetime import datetime
from enum import Enum


class SubscriptionStatus(str, Enum):
    ACTIVE = "active"
    PAST_DUE = "past_due"
    CANCELED = "canceled"
    UNPAID = "unpaid"
    TRIALING = "trialing"


class CreateSubscriptionRequest(BaseModel):
    plan_id: str
    payment_method_id: str
    quantity: int = 1
    trial_days: Optional[int] = None


class UpdateSubscriptionRequest(BaseModel):
    plan_id: Optional[str] = None
    quantity: Optional[int] = None


class SubscriptionResponse(BaseModel):
    id: str
    tenant_id: str
    plan_id: str
    status: SubscriptionStatus
    current_period_start: datetime
    current_period_end: datetime
    cancel_at: Optional[datetime]
    quantity: int
    mrr: float  # Monthly Recurring Revenue


class PaymentMethodRequest(BaseModel):
    stripe_payment_method_id: str  # Tokenized by Stripe.js


class PaymentMethodResponse(BaseModel):
    id: str
    type: str  # "card", "bank_account"
    last4: str
    brand: Optional[str] = None  # For cards
    exp_month: Optional[int] = None
    exp_year: Optional[int] = None


class InvoiceResponse(BaseModel):
    id: str
    tenant_id: str
    subscription_id: str
    amount_due: float
    amount_paid: float
    status: str
    due_date: datetime
    paid_at: Optional[datetime]
    pdf_url: Optional[str]


class UsageRecordRequest(BaseModel):
    subscription_id: str
    metric_name: str
    quantity: int = Field(..., gt=0)
    timestamp: datetime
