"""
@file        payment.py
@brief       Payment router with MockStripeClient integration
@copyright   (c) 2025 FtsCoDe GmbH. All rights reserved.
@author      Heinstein F.
@date        2025-11-15

@details
Handles subscriptions, payment methods, invoices, and pricing with mock Stripe.
"""

from fastapi import APIRouter, Depends, HTTPException, status, Header
from typing import Optional
from models.payment import (
    PriceListResponse, PriceResponse,
    CreateCustomerRequest, CustomerResponse,
    CreatePaymentMethodRequest, PaymentMethodResponse, PaymentMethodListResponse,
    CreateSubscriptionRequest, SubscriptionResponse, SubscriptionListResponse,
    UpdateSubscriptionRequest, CancelSubscriptionRequest,
    InvoiceResponse, InvoiceListResponse,
    PlanComparisonResponse, PlanDetails, PlanFeature, PlanTier,
    PriceMetadata, RecurringInfo,
    SubscriptionItemsResponse, SubscriptionItemResponse, SubscriptionItemPrice,
    InvoiceLinesResponse, InvoiceLineResponse, InvoiceLinePrice,
    PaymentMethodCardResponse,
)
from services.mock_stripe_client import get_mock_stripe_client, MockStripeClient
from dependencies.auth import get_current_user, UserContext

router = APIRouter()


# Price endpoints
@router.get("/prices", response_model=PriceListResponse)
async def list_prices(
    stripe: MockStripeClient = Depends(get_mock_stripe_client)
):
    """List all available pricing plans"""
    prices = stripe.list_prices()

    formatted_prices = []
    for price in prices:
        formatted_prices.append(PriceResponse(
            id=price["id"],
            product=price["product"],
            currency=price["currency"],
            unit_amount=price["unit_amount"],
            recurring=RecurringInfo(interval=price["recurring"]["interval"]),
            metadata=PriceMetadata(**price["metadata"])
        ))

    return PriceListResponse(data=formatted_prices)


@router.get("/plans/compare", response_model=PlanComparisonResponse)
async def compare_plans():
    """Get plan comparison data for UI"""
    plans = [
        PlanDetails(
            tier=PlanTier.FREE,
            name="Free",
            price_monthly_id="price_free",
            price_monthly=0,
            storage_gb=10,
            uploads_per_month="100",
            features=[
                PlanFeature(name="Storage", value="10 GB", included=True),
                PlanFeature(name="Uploads/month", value="100", included=True),
                PlanFeature(name="Email support", value="Community", included=True),
                PlanFeature(name="API access", value="Limited", included=True),
                PlanFeature(name="Custom domain", value="No", included=False),
            ],
            popular=False
        ),
        PlanDetails(
            tier=PlanTier.STARTER,
            name="Starter",
            price_monthly_id="price_starter_monthly",
            price_yearly_id="price_starter_yearly",
            price_monthly=990,
            price_yearly=9900,
            storage_gb=100,
            uploads_per_month="1,000",
            features=[
                PlanFeature(name="Storage", value="100 GB", included=True),
                PlanFeature(name="Uploads/month", value="1,000", included=True),
                PlanFeature(name="Email support", value="24/7", included=True),
                PlanFeature(name="API access", value="Full", included=True),
                PlanFeature(name="Custom domain", value="1 domain", included=True),
            ],
            popular=True
        ),
        PlanDetails(
            tier=PlanTier.PRO,
            name="Pro",
            price_monthly_id="price_pro_monthly",
            price_yearly_id="price_pro_yearly",
            price_monthly=2990,
            price_yearly=29900,
            storage_gb=500,
            uploads_per_month="10,000",
            features=[
                PlanFeature(name="Storage", value="500 GB", included=True),
                PlanFeature(name="Uploads/month", value="10,000", included=True),
                PlanFeature(name="Email support", value="Priority 24/7", included=True),
                PlanFeature(name="API access", value="Full + Webhooks", included=True),
                PlanFeature(name="Custom domain", value="Unlimited", included=True),
            ],
            popular=False
        ),
        PlanDetails(
            tier=PlanTier.ENTERPRISE,
            name="Enterprise",
            price_monthly_id="price_enterprise_monthly",
            price_monthly=9990,
            storage_gb=2000,
            uploads_per_month="Unlimited",
            features=[
                PlanFeature(name="Storage", value="2 TB", included=True),
                PlanFeature(name="Uploads/month", value="Unlimited", included=True),
                PlanFeature(name="Email support", value="Dedicated account manager", included=True),
                PlanFeature(name="API access", value="Full + Custom integrations", included=True),
                PlanFeature(name="Custom domain", value="Unlimited + SSO", included=True),
            ],
            popular=False
        ),
    ]

    return PlanComparisonResponse(plans=plans)


# Customer endpoints
@router.post("/customers", response_model=CustomerResponse)
async def create_customer(
    request: CreateCustomerRequest,
    user: UserContext = Depends(get_current_user),
    stripe: MockStripeClient = Depends(get_mock_stripe_client)
):
    """Create Stripe customer"""
    user_id, tenant_id, email = user.user_id, user.tenant_id, user.email

    customer = stripe.create_customer(
        email=request.email,
        name=request.name,
        metadata={"user_id": user_id, "tenant_id": tenant_id}
    )

    return CustomerResponse(**customer)


# Payment Method endpoints
@router.post("/payment-methods", response_model=PaymentMethodResponse)
async def create_payment_method(
    request: CreatePaymentMethodRequest,
    user: UserContext = Depends(get_current_user),
    stripe: MockStripeClient = Depends(get_mock_stripe_client)
):
    """Create payment method (card)"""
    user_id, tenant_id, email = user.user_id, user.tenant_id, user.email

    pm = stripe.create_payment_method(
        type=request.type,
        card={
            "brand": "visa",  # Mock: always visa
            "last4": request.card.number[-4:],
            "exp_month": request.card.exp_month,
            "exp_year": request.card.exp_year,
        }
    )

    return PaymentMethodResponse(
        id=pm["id"],
        type=pm["type"],
        card=PaymentMethodCardResponse(**pm["card"]) if pm.get("card") else None,
        created=pm["created"]
    )


@router.post("/payment-methods/{pm_id}/attach")
async def attach_payment_method(
    pm_id: str,
    customer_id: str,
    user: UserContext = Depends(get_current_user),
    stripe: MockStripeClient = Depends(get_mock_stripe_client)
):
    """Attach payment method to customer"""
    user_id, tenant_id, email = user.user_id, user.tenant_id, user.email

    try:
        stripe.attach_payment_method(pm_id, customer_id)
        return {"message": "Payment method attached successfully"}
    except ValueError as e:
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail=str(e))


@router.get("/customers/{customer_id}/payment-methods", response_model=PaymentMethodListResponse)
async def list_payment_methods(
    customer_id: str,
    user: UserContext = Depends(get_current_user),
    stripe: MockStripeClient = Depends(get_mock_stripe_client)
):
    """List customer's payment methods"""
    user_id, tenant_id, email = user.user_id, user.tenant_id, user.email

    pms = stripe.list_payment_methods(customer_id, type="card")

    formatted_pms = []
    for pm in pms:
        formatted_pms.append(PaymentMethodResponse(
            id=pm["id"],
            type=pm["type"],
            card=PaymentMethodCardResponse(**pm["card"]) if pm.get("card") else None,
            created=pm["created"]
        ))

    return PaymentMethodListResponse(data=formatted_pms)


# Subscription endpoints
@router.post("/subscriptions", response_model=SubscriptionResponse)
async def create_subscription(
    request: CreateSubscriptionRequest,
    customer_id: str,
    user: UserContext = Depends(get_current_user),
    stripe: MockStripeClient = Depends(get_mock_stripe_client)
):
    """Create subscription"""
    user_id, tenant_id, email = user.user_id, user.tenant_id, user.email

    try:
        sub = stripe.create_subscription(
            customer_id=customer_id,
            price_id=request.price_id,
            payment_method_id=request.payment_method_id,
            metadata={"user_id": user_id, "tenant_id": tenant_id}
        )

        # Format response
        items_data = []
        for item in sub["items"]["data"]:
            price = item["price"]
            items_data.append(SubscriptionItemResponse(
                id=item["id"],
                price=SubscriptionItemPrice(
                    id=price["id"],
                    product=price["product"],
                    currency=price["currency"],
                    unit_amount=price["unit_amount"],
                    recurring=RecurringInfo(interval=price["recurring"]["interval"]),
                    metadata=PriceMetadata(**price["metadata"])
                ),
                quantity=item["quantity"]
            ))

        return SubscriptionResponse(
            id=sub["id"],
            customer=sub["customer"],
            status=sub["status"],
            current_period_start=sub["current_period_start"],
            current_period_end=sub["current_period_end"],
            created=sub["created"],
            default_payment_method=sub.get("default_payment_method"),
            items=SubscriptionItemsResponse(data=items_data),
            metadata=sub.get("metadata", {})
        )

    except ValueError as e:
        raise HTTPException(status_code=status.HTTP_400_BAD_REQUEST, detail=str(e))


@router.get("/customers/{customer_id}/subscriptions", response_model=SubscriptionListResponse)
async def list_subscriptions(
    customer_id: str,
    user: UserContext = Depends(get_current_user),
    stripe: MockStripeClient = Depends(get_mock_stripe_client)
):
    """List customer subscriptions"""
    user_id, tenant_id, email = user.user_id, user.tenant_id, user.email

    subs = stripe.list_subscriptions(customer_id=customer_id)

    formatted_subs = []
    for sub in subs:
        items_data = []
        for item in sub["items"]["data"]:
            price = item["price"]
            items_data.append(SubscriptionItemResponse(
                id=item["id"],
                price=SubscriptionItemPrice(
                    id=price["id"],
                    product=price["product"],
                    currency=price["currency"],
                    unit_amount=price["unit_amount"],
                    recurring=RecurringInfo(interval=price["recurring"]["interval"]),
                    metadata=PriceMetadata(**price["metadata"])
                ),
                quantity=item["quantity"]
            ))

        formatted_subs.append(SubscriptionResponse(
            id=sub["id"],
            customer=sub["customer"],
            status=sub["status"],
            current_period_start=sub["current_period_start"],
            current_period_end=sub["current_period_end"],
            created=sub["created"],
            cancel_at_period_end=sub.get("cancel_at_period_end", False),
            canceled_at=sub.get("canceled_at"),
            default_payment_method=sub.get("default_payment_method"),
            items=SubscriptionItemsResponse(data=items_data),
            metadata=sub.get("metadata", {})
        ))

    return SubscriptionListResponse(data=formatted_subs)


@router.patch("/subscriptions/{subscription_id}", response_model=SubscriptionResponse)
async def update_subscription(
    subscription_id: str,
    request: UpdateSubscriptionRequest,
    user: UserContext = Depends(get_current_user),
    stripe: MockStripeClient = Depends(get_mock_stripe_client)
):
    """Update subscription (change plan)"""
    user_id, tenant_id, email = user.user_id, user.tenant_id, user.email

    try:
        sub = stripe.update_subscription(subscription_id, price_id=request.price_id)

        items_data = []
        for item in sub["items"]["data"]:
            price = item["price"]
            items_data.append(SubscriptionItemResponse(
                id=item["id"],
                price=SubscriptionItemPrice(
                    id=price["id"],
                    product=price["product"],
                    currency=price["currency"],
                    unit_amount=price["unit_amount"],
                    recurring=RecurringInfo(interval=price["recurring"]["interval"]),
                    metadata=PriceMetadata(**price["metadata"])
                ),
                quantity=item["quantity"]
            ))

        return SubscriptionResponse(
            id=sub["id"],
            customer=sub["customer"],
            status=sub["status"],
            current_period_start=sub["current_period_start"],
            current_period_end=sub["current_period_end"],
            created=sub["created"],
            cancel_at_period_end=sub.get("cancel_at_period_end", False),
            canceled_at=sub.get("canceled_at"),
            default_payment_method=sub.get("default_payment_method"),
            items=SubscriptionItemsResponse(data=items_data),
            metadata=sub.get("metadata", {})
        )

    except ValueError as e:
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail=str(e))


@router.post("/subscriptions/{subscription_id}/cancel", response_model=SubscriptionResponse)
async def cancel_subscription(
    subscription_id: str,
    request: CancelSubscriptionRequest,
    user: UserContext = Depends(get_current_user),
    stripe: MockStripeClient = Depends(get_mock_stripe_client)
):
    """Cancel subscription"""
    user_id, tenant_id, email = user.user_id, user.tenant_id, user.email

    try:
        sub = stripe.cancel_subscription(subscription_id, at_period_end=request.at_period_end)

        items_data = []
        for item in sub["items"]["data"]:
            price = item["price"]
            items_data.append(SubscriptionItemResponse(
                id=item["id"],
                price=SubscriptionItemPrice(
                    id=price["id"],
                    product=price["product"],
                    currency=price["currency"],
                    unit_amount=price["unit_amount"],
                    recurring=RecurringInfo(interval=price["recurring"]["interval"]),
                    metadata=PriceMetadata(**price["metadata"])
                ),
                quantity=item["quantity"]
            ))

        return SubscriptionResponse(
            id=sub["id"],
            customer=sub["customer"],
            status=sub["status"],
            current_period_start=sub["current_period_start"],
            current_period_end=sub["current_period_end"],
            created=sub["created"],
            cancel_at_period_end=sub.get("cancel_at_period_end", False),
            canceled_at=sub.get("canceled_at"),
            default_payment_method=sub.get("default_payment_method"),
            items=SubscriptionItemsResponse(data=items_data),
            metadata=sub.get("metadata", {})
        )

    except ValueError as e:
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail=str(e))


# Invoice endpoints
@router.get("/customers/{customer_id}/invoices", response_model=InvoiceListResponse)
async def list_invoices(
    customer_id: str,
    user: UserContext = Depends(get_current_user),
    stripe: MockStripeClient = Depends(get_mock_stripe_client)
):
    """List customer invoices"""
    user_id, tenant_id, email = user.user_id, user.tenant_id, user.email

    invoices = stripe.list_invoices(customer_id=customer_id)

    formatted_invoices = []
    for inv in invoices:
        lines_data = []
        for line in inv["lines"]["data"]:
            price = line["price"]
            lines_data.append(InvoiceLineResponse(
                id=line["id"],
                amount=line["amount"],
                currency=line["currency"],
                description=line["description"],
                price=InvoiceLinePrice(
                    id=price["id"],
                    product=price["product"],
                    currency=price["currency"],
                    unit_amount=price["unit_amount"],
                    recurring=RecurringInfo(interval=price["recurring"]["interval"]),
                    metadata=PriceMetadata(**price["metadata"])
                ),
                quantity=line["quantity"]
            ))

        formatted_invoices.append(InvoiceResponse(
            id=inv["id"],
            customer=inv["customer"],
            subscription=inv.get("subscription"),
            status=inv["status"],
            amount_due=inv["amount_due"],
            amount_paid=inv["amount_paid"],
            currency=inv["currency"],
            created=inv["created"],
            period_start=inv["period_start"],
            period_end=inv["period_end"],
            lines=InvoiceLinesResponse(data=lines_data)
        ))

    return InvoiceListResponse(data=formatted_invoices)
