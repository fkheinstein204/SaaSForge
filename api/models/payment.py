"""
@file        payment.py
@brief       Payment and subscription Pydantic models
@copyright   (c) 2025 FtsCoDe GmbH. All rights reserved.
@author      Heinstein F.
@date        2025-11-15
"""

from pydantic import BaseModel, Field, EmailStr
from typing import Optional, List, Dict, Any
from datetime import datetime
from enum import Enum


class SubscriptionStatus(str, Enum):
    """Subscription status"""
    ACTIVE = "active"
    PAST_DUE = "past_due"
    CANCELED = "canceled"
    UNPAID = "unpaid"
    TRIALING = "trialing"


class BillingInterval(str, Enum):
    """Billing interval"""
    MONTH = "month"
    YEAR = "year"


class PlanTier(str, Enum):
    """Plan tiers"""
    FREE = "free"
    STARTER = "starter"
    PRO = "pro"
    ENTERPRISE = "enterprise"


# Price models
class PriceMetadata(BaseModel):
    """Price metadata"""
    name: str
    storage_gb: str
    uploads_per_month: str


class RecurringInfo(BaseModel):
    """Recurring billing information"""
    interval: str


class PriceResponse(BaseModel):
    """Price response"""
    id: str
    product: str
    currency: str
    unit_amount: int  # in cents
    recurring: RecurringInfo
    metadata: PriceMetadata


class PriceListResponse(BaseModel):
    """List of available prices"""
    data: List[PriceResponse]


# Customer models
class CreateCustomerRequest(BaseModel):
    """Create Stripe customer request"""
    email: EmailStr
    name: str


class CustomerResponse(BaseModel):
    """Customer response"""
    id: str
    email: str
    name: str
    created: int
    metadata: Dict[str, Any] = {}


# Payment Method models
class CardDetails(BaseModel):
    """Card details for creating payment method"""
    number: str = Field(..., min_length=13, max_length=19)
    exp_month: int = Field(..., ge=1, le=12)
    exp_year: int = Field(..., ge=2025)
    cvc: str = Field(..., min_length=3, max_length=4)


class CreatePaymentMethodRequest(BaseModel):
    """Create payment method request"""
    type: str = "card"
    card: CardDetails


class PaymentMethodCardResponse(BaseModel):
    """Payment method card details"""
    brand: str
    last4: str
    exp_month: int
    exp_year: int


class PaymentMethodResponse(BaseModel):
    """Payment method response"""
    id: str
    type: str
    card: Optional[PaymentMethodCardResponse] = None
    created: int


class PaymentMethodListResponse(BaseModel):
    """List of payment methods"""
    data: List[PaymentMethodResponse]


# Subscription models
class CreateSubscriptionRequest(BaseModel):
    """Create subscription request"""
    price_id: str
    payment_method_id: Optional[str] = None


class SubscriptionItemPrice(BaseModel):
    """Subscription item price"""
    id: str
    product: str
    currency: str
    unit_amount: int
    recurring: RecurringInfo
    metadata: PriceMetadata


class SubscriptionItemResponse(BaseModel):
    """Subscription item"""
    id: str
    price: SubscriptionItemPrice
    quantity: int


class SubscriptionItemsResponse(BaseModel):
    """Subscription items wrapper"""
    object: str = "list"
    data: List[SubscriptionItemResponse]


class SubscriptionResponse(BaseModel):
    """Subscription response"""
    id: str
    customer: str
    status: SubscriptionStatus
    current_period_start: int
    current_period_end: int
    created: int
    cancel_at_period_end: Optional[bool] = False
    canceled_at: Optional[int] = None
    default_payment_method: Optional[str] = None
    items: SubscriptionItemsResponse
    metadata: Dict[str, Any] = {}


class SubscriptionListResponse(BaseModel):
    """List of subscriptions"""
    data: List[SubscriptionResponse]


class UpdateSubscriptionRequest(BaseModel):
    """Update subscription request"""
    price_id: Optional[str] = None


class CancelSubscriptionRequest(BaseModel):
    """Cancel subscription request"""
    at_period_end: bool = False


# Invoice models
class InvoiceLinePrice(BaseModel):
    """Invoice line item price"""
    id: str
    product: str
    currency: str
    unit_amount: int
    recurring: RecurringInfo
    metadata: PriceMetadata


class InvoiceLineResponse(BaseModel):
    """Invoice line item"""
    id: str
    amount: int
    currency: str
    description: str
    price: InvoiceLinePrice
    quantity: int


class InvoiceLinesResponse(BaseModel):
    """Invoice lines wrapper"""
    object: str = "list"
    data: List[InvoiceLineResponse]


class InvoiceResponse(BaseModel):
    """Invoice response"""
    id: str
    customer: str
    subscription: Optional[str] = None
    status: str
    amount_due: int
    amount_paid: int
    currency: str
    created: int
    period_start: int
    period_end: int
    lines: InvoiceLinesResponse


class InvoiceListResponse(BaseModel):
    """List of invoices"""
    data: List[InvoiceResponse]


# Plan comparison
class PlanFeature(BaseModel):
    """Plan feature"""
    name: str
    value: str
    included: bool


class PlanDetails(BaseModel):
    """Plan details for comparison"""
    tier: PlanTier
    name: str
    price_monthly_id: str
    price_yearly_id: Optional[str] = None
    price_monthly: int  # in cents
    price_yearly: Optional[int] = None  # in cents
    storage_gb: int
    uploads_per_month: str
    features: List[PlanFeature]
    popular: bool = False


class PlanComparisonResponse(BaseModel):
    """Plan comparison response"""
    plans: List[PlanDetails]


# Legacy models for backward compatibility
class PaymentMethodRequest(BaseModel):
    """Legacy payment method request"""
    stripe_payment_method_id: str


class UsageRecordRequest(BaseModel):
    """Usage record request"""
    subscription_id: str
    metric_name: str
    quantity: int = Field(..., gt=0)
    timestamp: datetime
