"""
Payment router
Handles subscriptions, invoices, and usage billing
"""
from fastapi import APIRouter, Depends, HTTPException, status, Header
from typing import Optional
from models.payment import (
    CreateSubscriptionRequest, SubscriptionResponse,
    UpdateSubscriptionRequest,
    PaymentMethodRequest, PaymentMethodResponse,
    InvoiceResponse,
    UsageRecordRequest
)

router = APIRouter()


@router.post("/subscriptions", response_model=SubscriptionResponse)
async def create_subscription(
    request: CreateSubscriptionRequest,
    idempotency_key: str = Header(..., alias="Idempotency-Key")
):
    """
    Create new subscription (REQUIRED: Idempotency-Key header)
    """
    # TODO: Check idempotency key in Redis
    # TODO: Call gRPC PaymentService.CreateSubscription
    # TODO: Integrate with Stripe
    # TODO: Store result in idempotency cache (24h TTL)
    raise HTTPException(
        status_code=status.HTTP_501_NOT_IMPLEMENTED,
        detail="Not implemented yet"
    )


@router.patch("/subscriptions/{subscription_id}", response_model=SubscriptionResponse)
async def update_subscription(
    subscription_id: str,
    request: UpdateSubscriptionRequest
):
    """
    Update subscription (change plan, quantity, etc.)
    """
    # TODO: Validate tenant ownership
    # TODO: Calculate prorated amounts
    # TODO: Update in Stripe + database
    raise HTTPException(
        status_code=status.HTTP_501_NOT_IMPLEMENTED,
        detail="Not implemented yet"
    )


@router.delete("/subscriptions/{subscription_id}")
async def cancel_subscription(subscription_id: str):
    """
    Cancel subscription (at period end or immediately)
    """
    # TODO: Validate tenant ownership
    # TODO: Cancel in Stripe
    # TODO: Update status to 'canceled'
    raise HTTPException(
        status_code=status.HTTP_501_NOT_IMPLEMENTED,
        detail="Not implemented yet"
    )


@router.post("/payment-methods", response_model=PaymentMethodResponse)
async def add_payment_method(request: PaymentMethodRequest):
    """
    Add payment method (tokenized via Stripe)
    """
    # TODO: Attach payment method to Stripe customer
    # TODO: Store reference in database (NOT card details)
    raise HTTPException(
        status_code=status.HTTP_501_NOT_IMPLEMENTED,
        detail="Not implemented yet"
    )


@router.get("/invoices/{invoice_id}", response_model=InvoiceResponse)
async def get_invoice(invoice_id: str):
    """
    Get invoice details
    """
    # TODO: Fetch from database
    # TODO: Validate tenant ownership
    raise HTTPException(
        status_code=status.HTTP_501_NOT_IMPLEMENTED,
        detail="Not implemented yet"
    )


@router.post("/usage")
async def record_usage(request: UsageRecordRequest):
    """
    Record usage for metered billing
    """
    # TODO: Store in database
    # TODO: Aggregate for billing period
    # TODO: Report to Stripe for invoice generation
    raise HTTPException(
        status_code=status.HTTP_501_NOT_IMPLEMENTED,
        detail="Not implemented yet"
    )
