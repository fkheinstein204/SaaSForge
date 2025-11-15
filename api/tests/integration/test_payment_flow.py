"""
@file        test_payment_flow.py
@brief       Integration tests for payment and subscription flows
@copyright   (c) 2025 FtsCoDe GmbH. All rights reserved.
@author      Heinstein F.
@date        2025-11-15

Tests:
5. Create subscription with MockStripe
6. Update subscription (upgrade/downgrade)
7. Cancel subscription
"""

import pytest
from fastapi import status


class TestPaymentFlow:
    """Test payment and subscription flows"""

    def test_list_available_prices(self, client):
        """
        Test: List available pricing plans

        Steps:
        1. Request pricing plans
        2. Verify all tiers are returned
        3. Verify pricing structure
        """
        response = client.get("/v1/payments/prices")

        assert response.status_code == status.HTTP_200_OK
        data = response.json()
        assert "data" in data
        prices = data["data"]

        # Should have multiple pricing tiers
        assert len(prices) > 0

        # Verify price structure
        for price in prices:
            assert "id" in price
            assert "product" in price
            assert "currency" in price
            assert "unit_amount" in price
            assert "recurring" in price
            assert "metadata" in price
            assert price["currency"] == "usd"

    def test_create_customer(self, client, auth_headers, test_user_data):
        """
        Test: Create Stripe customer

        Steps:
        1. Create customer
        2. Verify customer ID is returned
        3. Verify customer metadata
        """
        customer_data = {
            "email": test_user_data["email"],
            "name": test_user_data["name"],
        }

        response = client.post(
            "/v1/payments/customers",
            json=customer_data,
            headers=auth_headers
        )

        assert response.status_code == status.HTTP_200_OK
        data = response.json()
        assert "id" in data
        assert data["id"].startswith("cus_")
        assert data["email"] == test_user_data["email"]
        assert data["name"] == test_user_data["name"]

    def test_create_subscription_flow(
        self, client, auth_headers, test_customer, test_payment_method, mock_stripe_client
    ):
        """
        Test 5: Create subscription with MockStripe

        Steps:
        1. Create customer
        2. Create payment method
        3. Attach payment method to customer
        4. Create subscription
        5. Verify subscription is active
        """
        # Attach payment method to customer
        mock_stripe_client.attach_payment_method(
            test_payment_method["id"],
            test_customer["id"]
        )

        # Create subscription
        subscription_data = {
            "price_id": "price_starter_monthly",
            "payment_method_id": test_payment_method["id"],
        }

        response = client.post(
            f"/v1/payments/subscriptions?customer_id={test_customer['id']}",
            json=subscription_data,
            headers=auth_headers
        )

        assert response.status_code == status.HTTP_200_OK
        data = response.json()
        assert "id" in data
        assert data["id"].startswith("sub_")
        assert data["customer"] == test_customer["id"]
        assert data["status"] == "active"
        assert "items" in data
        assert len(data["items"]["data"]) > 0

        # Verify invoice was created
        invoices = mock_stripe_client.list_invoices(customer_id=test_customer["id"])
        assert len(invoices) > 0

    def test_update_subscription_plan(
        self, client, auth_headers, test_customer, test_payment_method, mock_stripe_client
    ):
        """
        Test 6: Update subscription (upgrade/downgrade)

        Steps:
        1. Create subscription with starter plan
        2. Upgrade to pro plan
        3. Verify subscription is updated
        4. Verify new invoice is created
        """
        # Attach payment method
        mock_stripe_client.attach_payment_method(
            test_payment_method["id"],
            test_customer["id"]
        )

        # Create subscription with starter plan
        subscription = mock_stripe_client.create_subscription(
            customer_id=test_customer["id"],
            price_id="price_starter_monthly",
            payment_method_id=test_payment_method["id"],
        )

        # Upgrade to pro plan
        update_data = {
            "price_id": "price_pro_monthly",
        }

        response = client.patch(
            f"/v1/payments/subscriptions/{subscription['id']}",
            json=update_data,
            headers=auth_headers
        )

        assert response.status_code == status.HTTP_200_OK
        data = response.json()
        assert data["id"] == subscription["id"]

        # Verify plan was updated
        updated_price = data["items"]["data"][0]["price"]
        assert updated_price["id"] == "price_pro_monthly"

    def test_cancel_subscription(
        self, client, auth_headers, test_customer, test_payment_method, mock_stripe_client
    ):
        """
        Test 7: Cancel subscription

        Steps:
        1. Create subscription
        2. Cancel subscription immediately
        3. Verify subscription status is canceled
        4. Cancel subscription at period end
        5. Verify cancel_at_period_end flag
        """
        # Attach payment method
        mock_stripe_client.attach_payment_method(
            test_payment_method["id"],
            test_customer["id"]
        )

        # Create subscription
        subscription = mock_stripe_client.create_subscription(
            customer_id=test_customer["id"],
            price_id="price_starter_monthly",
            payment_method_id=test_payment_method["id"],
        )

        # Cancel immediately
        cancel_data = {
            "at_period_end": False,
        }

        response = client.post(
            f"/v1/payments/subscriptions/{subscription['id']}/cancel",
            json=cancel_data,
            headers=auth_headers
        )

        assert response.status_code == status.HTTP_200_OK
        data = response.json()
        assert data["status"] == "canceled"
        assert "canceled_at" in data

    def test_list_customer_invoices(
        self, client, auth_headers, test_customer, test_payment_method, mock_stripe_client
    ):
        """
        Test: List customer invoices

        Steps:
        1. Create subscription (generates invoice)
        2. List invoices for customer
        3. Verify invoice structure
        """
        # Attach payment method
        mock_stripe_client.attach_payment_method(
            test_payment_method["id"],
            test_customer["id"]
        )

        # Create subscription (auto-generates invoice)
        mock_stripe_client.create_subscription(
            customer_id=test_customer["id"],
            price_id="price_starter_monthly",
            payment_method_id=test_payment_method["id"],
        )

        # List invoices
        response = client.get(
            f"/v1/payments/customers/{test_customer['id']}/invoices",
            headers=auth_headers
        )

        assert response.status_code == status.HTTP_200_OK
        data = response.json()
        assert "data" in data
        invoices = data["data"]
        assert len(invoices) > 0

        # Verify invoice structure
        invoice = invoices[0]
        assert "id" in invoice
        assert invoice["customer"] == test_customer["id"]
        assert "amount_due" in invoice
        assert "status" in invoice
        assert invoice["status"] == "paid"  # MockStripe auto-pays

    def test_plan_comparison(self, client):
        """
        Test: Get plan comparison data

        Steps:
        1. Request plan comparison
        2. Verify all tiers are returned
        3. Verify plan features
        """
        response = client.get("/v1/payments/plans/compare")

        assert response.status_code == status.HTTP_200_OK
        data = response.json()
        assert "plans" in data
        plans = data["plans"]

        # Should have 4 tiers: free, starter, pro, enterprise
        assert len(plans) >= 4

        # Verify plan structure
        for plan in plans:
            assert "tier" in plan
            assert "name" in plan
            assert "price_monthly" in plan
            assert "storage_gb" in plan
            assert "features" in plan
            assert isinstance(plan["features"], list)
