"""
@file        mock_stripe_client.py
@brief       Mock Stripe client for development and testing
@copyright   (c) 2025 FtsCoDe GmbH. All rights reserved.
@author      Heinstein F.
@date        2025-11-15

@details
Simulates Stripe API behavior without actual API calls or charges.
Useful for development, testing, and demo environments.
"""

import uuid
from datetime import datetime, timedelta
from typing import Dict, List, Optional, Any
import json


class MockStripeClient:
    """Mock Stripe API client"""

    def __init__(self):
        # In-memory storage
        self.customers: Dict[str, Dict] = {}
        self.subscriptions: Dict[str, Dict] = {}
        self.payment_methods: Dict[str, Dict] = {}
        self.invoices: Dict[str, Dict] = {}
        self.prices: Dict[str, Dict] = self._initialize_prices()

    def _initialize_prices(self) -> Dict[str, Dict]:
        """Initialize standard pricing tiers"""
        return {
            "price_free": {
                "id": "price_free",
                "product": "prod_free",
                "currency": "usd",
                "unit_amount": 0,
                "recurring": {"interval": "month"},
                "metadata": {
                    "name": "Free",
                    "storage_gb": "10",
                    "uploads_per_month": "100",
                },
            },
            "price_starter_monthly": {
                "id": "price_starter_monthly",
                "product": "prod_starter",
                "currency": "usd",
                "unit_amount": 990,  # $9.90/month
                "recurring": {"interval": "month"},
                "metadata": {
                    "name": "Starter",
                    "storage_gb": "100",
                    "uploads_per_month": "1000",
                },
            },
            "price_starter_yearly": {
                "id": "price_starter_yearly",
                "product": "prod_starter",
                "currency": "usd",
                "unit_amount": 9900,  # $99/year (2 months free)
                "recurring": {"interval": "year"},
                "metadata": {
                    "name": "Starter",
                    "storage_gb": "100",
                    "uploads_per_month": "1000",
                },
            },
            "price_pro_monthly": {
                "id": "price_pro_monthly",
                "product": "prod_pro",
                "currency": "usd",
                "unit_amount": 2990,  # $29.90/month
                "recurring": {"interval": "month"},
                "metadata": {
                    "name": "Pro",
                    "storage_gb": "500",
                    "uploads_per_month": "10000",
                },
            },
            "price_pro_yearly": {
                "id": "price_pro_yearly",
                "product": "prod_pro",
                "currency": "usd",
                "unit_amount": 29900,  # $299/year (2 months free)
                "recurring": {"interval": "year"},
                "metadata": {
                    "name": "Pro",
                    "storage_gb": "500",
                    "uploads_per_month": "10000",
                },
            },
            "price_enterprise_monthly": {
                "id": "price_enterprise_monthly",
                "product": "prod_enterprise",
                "currency": "usd",
                "unit_amount": 9990,  # $99.90/month
                "recurring": {"interval": "month"},
                "metadata": {
                    "name": "Enterprise",
                    "storage_gb": "2000",
                    "uploads_per_month": "unlimited",
                },
            },
        }

    # Customer methods
    def create_customer(
        self, email: str, name: str, metadata: Optional[Dict] = None
    ) -> Dict:
        """Create a mock Stripe customer"""
        customer_id = f"cus_{uuid.uuid4().hex[:14]}"
        customer = {
            "id": customer_id,
            "object": "customer",
            "email": email,
            "name": name,
            "created": int(datetime.utcnow().timestamp()),
            "metadata": metadata or {},
        }
        self.customers[customer_id] = customer
        return customer

    def retrieve_customer(self, customer_id: str) -> Optional[Dict]:
        """Retrieve a customer by ID"""
        return self.customers.get(customer_id)

    def update_customer(
        self, customer_id: str, **kwargs
    ) -> Dict:
        """Update a customer"""
        if customer_id not in self.customers:
            raise ValueError(f"Customer {customer_id} not found")

        customer = self.customers[customer_id]
        customer.update(kwargs)
        return customer

    # Payment Method methods
    def create_payment_method(
        self, type: str, card: Optional[Dict] = None
    ) -> Dict:
        """Create a mock payment method"""
        pm_id = f"pm_{uuid.uuid4().hex[:14]}"

        if type == "card":
            card_data = card or {}
            payment_method = {
                "id": pm_id,
                "object": "payment_method",
                "type": "card",
                "card": {
                    "brand": card_data.get("brand", "visa"),
                    "last4": card_data.get("last4", "4242"),
                    "exp_month": card_data.get("exp_month", 12),
                    "exp_year": card_data.get("exp_year", 2025),
                },
                "created": int(datetime.utcnow().timestamp()),
            }
        else:
            payment_method = {
                "id": pm_id,
                "object": "payment_method",
                "type": type,
                "created": int(datetime.utcnow().timestamp()),
            }

        self.payment_methods[pm_id] = payment_method
        return payment_method

    def attach_payment_method(
        self, payment_method_id: str, customer_id: str
    ) -> Dict:
        """Attach payment method to customer"""
        if payment_method_id not in self.payment_methods:
            raise ValueError(f"Payment method {payment_method_id} not found")
        if customer_id not in self.customers:
            raise ValueError(f"Customer {customer_id} not found")

        pm = self.payment_methods[payment_method_id]
        pm["customer"] = customer_id
        return pm

    def list_payment_methods(
        self, customer_id: str, type: str = "card"
    ) -> List[Dict]:
        """List customer's payment methods"""
        return [
            pm
            for pm in self.payment_methods.values()
            if pm.get("customer") == customer_id and pm["type"] == type
        ]

    # Subscription methods
    def create_subscription(
        self,
        customer_id: str,
        price_id: str,
        payment_method_id: Optional[str] = None,
        metadata: Optional[Dict] = None,
    ) -> Dict:
        """Create a mock subscription"""
        if customer_id not in self.customers:
            raise ValueError(f"Customer {customer_id} not found")
        if price_id not in self.prices:
            raise ValueError(f"Price {price_id} not found")

        sub_id = f"sub_{uuid.uuid4().hex[:14]}"
        now = datetime.utcnow()
        price = self.prices[price_id]

        # Calculate period end based on interval
        if price["recurring"]["interval"] == "month":
            period_end = now + timedelta(days=30)
        elif price["recurring"]["interval"] == "year":
            period_end = now + timedelta(days=365)
        else:
            period_end = now + timedelta(days=30)

        subscription = {
            "id": sub_id,
            "object": "subscription",
            "customer": customer_id,
            "status": "active",
            "current_period_start": int(now.timestamp()),
            "current_period_end": int(period_end.timestamp()),
            "created": int(now.timestamp()),
            "default_payment_method": payment_method_id,
            "items": {
                "object": "list",
                "data": [
                    {
                        "id": f"si_{uuid.uuid4().hex[:14]}",
                        "object": "subscription_item",
                        "price": price,
                        "quantity": 1,
                    }
                ],
            },
            "metadata": metadata or {},
        }

        self.subscriptions[sub_id] = subscription

        # Create initial invoice
        self._create_invoice_for_subscription(sub_id)

        return subscription

    def retrieve_subscription(self, subscription_id: str) -> Optional[Dict]:
        """Retrieve a subscription by ID"""
        return self.subscriptions.get(subscription_id)

    def update_subscription(
        self, subscription_id: str, **kwargs
    ) -> Dict:
        """Update a subscription"""
        if subscription_id not in self.subscriptions:
            raise ValueError(f"Subscription {subscription_id} not found")

        subscription = self.subscriptions[subscription_id]

        # Handle price change
        if "price_id" in kwargs:
            new_price_id = kwargs.pop("price_id")
            if new_price_id not in self.prices:
                raise ValueError(f"Price {new_price_id} not found")

            new_price = self.prices[new_price_id]
            subscription["items"]["data"][0]["price"] = new_price

            # Create invoice for upgrade/downgrade
            self._create_invoice_for_subscription(subscription_id)

        subscription.update(kwargs)
        return subscription

    def cancel_subscription(
        self, subscription_id: str, at_period_end: bool = False
    ) -> Dict:
        """Cancel a subscription"""
        if subscription_id not in self.subscriptions:
            raise ValueError(f"Subscription {subscription_id} not found")

        subscription = self.subscriptions[subscription_id]

        if at_period_end:
            subscription["cancel_at_period_end"] = True
            subscription["cancel_at"] = subscription["current_period_end"]
        else:
            subscription["status"] = "canceled"
            subscription["canceled_at"] = int(datetime.utcnow().timestamp())

        return subscription

    def list_subscriptions(
        self, customer_id: Optional[str] = None, status: Optional[str] = None
    ) -> List[Dict]:
        """List subscriptions"""
        subs = list(self.subscriptions.values())

        if customer_id:
            subs = [s for s in subs if s["customer"] == customer_id]

        if status:
            subs = [s for s in subs if s["status"] == status]

        return subs

    # Invoice methods
    def _create_invoice_for_subscription(self, subscription_id: str) -> Dict:
        """Create an invoice for a subscription"""
        subscription = self.subscriptions[subscription_id]
        price = subscription["items"]["data"][0]["price"]

        invoice_id = f"in_{uuid.uuid4().hex[:14]}"
        now = datetime.utcnow()

        invoice = {
            "id": invoice_id,
            "object": "invoice",
            "customer": subscription["customer"],
            "subscription": subscription_id,
            "status": "paid",  # Auto-paid in mock
            "amount_due": price["unit_amount"],
            "amount_paid": price["unit_amount"],
            "currency": price["currency"],
            "created": int(now.timestamp()),
            "period_start": subscription["current_period_start"],
            "period_end": subscription["current_period_end"],
            "lines": {
                "object": "list",
                "data": [
                    {
                        "id": f"il_{uuid.uuid4().hex[:14]}",
                        "object": "line_item",
                        "amount": price["unit_amount"],
                        "currency": price["currency"],
                        "description": f"{price['metadata']['name']} Plan",
                        "price": price,
                        "quantity": 1,
                    }
                ],
            },
        }

        self.invoices[invoice_id] = invoice
        return invoice

    def list_invoices(
        self, customer_id: Optional[str] = None, subscription_id: Optional[str] = None
    ) -> List[Dict]:
        """List invoices"""
        invoices = list(self.invoices.values())

        if customer_id:
            invoices = [i for i in invoices if i["customer"] == customer_id]

        if subscription_id:
            invoices = [i for i in invoices if i["subscription"] == subscription_id]

        # Sort by created date (newest first)
        invoices.sort(key=lambda i: i["created"], reverse=True)

        return invoices

    def retrieve_invoice(self, invoice_id: str) -> Optional[Dict]:
        """Retrieve an invoice by ID"""
        return self.invoices.get(invoice_id)

    # Price methods
    def list_prices(self) -> List[Dict]:
        """List all available prices"""
        return list(self.prices.values())

    def retrieve_price(self, price_id: str) -> Optional[Dict]:
        """Retrieve a price by ID"""
        return self.prices.get(price_id)


# Singleton instance
_mock_stripe_client = None


def get_mock_stripe_client() -> MockStripeClient:
    """Get mock Stripe client singleton"""
    global _mock_stripe_client
    if _mock_stripe_client is None:
        _mock_stripe_client = MockStripeClient()
    return _mock_stripe_client
