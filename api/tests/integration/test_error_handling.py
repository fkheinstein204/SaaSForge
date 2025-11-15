"""
@file        test_error_handling.py
@brief       Integration tests for error handling scenarios
@copyright   (c) 2025 FtsCoDe GmbH. All rights reserved.
@author      Heinstein F.
@date        2025-11-15

Tests:
8. Invalid JWT token handling
9. Quota exceeded error
10. Invalid request validation
"""

import pytest
from fastapi import status
import jwt
import os
from datetime import datetime, timedelta


class TestErrorHandling:
    """Test error handling scenarios"""

    def test_invalid_jwt_token(self, client):
        """
        Test 8: Invalid JWT token handling

        Steps:
        1. Send request with invalid token
        2. Verify 401 Unauthorized
        3. Send request with expired token
        4. Verify 401 Unauthorized
        """
        # Invalid token format
        invalid_headers = {"Authorization": "Bearer invalid-token-format"}

        response = client.get("/v1/uploads/storage/usage", headers=invalid_headers)
        assert response.status_code == status.HTTP_401_UNAUTHORIZED

        # Expired token
        expired_payload = {
            "sub": "user_123",
            "user_id": "user_123",
            "tenant_id": "tenant_123",
            "email": "test@example.com",
            "exp": datetime.utcnow() - timedelta(hours=1),  # Expired 1 hour ago
            "iat": datetime.utcnow() - timedelta(hours=2),
        }

        expired_token = jwt.encode(
            expired_payload,
            os.getenv("JWT_SECRET_KEY"),
            algorithm=os.getenv("JWT_ALGORITHM")
        )

        expired_headers = {"Authorization": f"Bearer {expired_token}"}

        response = client.get("/v1/uploads/storage/usage", headers=expired_headers)
        assert response.status_code == status.HTTP_401_UNAUTHORIZED

    def test_missing_authorization_header(self, client):
        """
        Test: Request to protected endpoint without auth header
        """
        response = client.get("/v1/uploads/storage/usage")

        assert response.status_code == status.HTTP_401_UNAUTHORIZED

    def test_quota_exceeded_error(self, client, auth_headers):
        """
        Test 9: Quota exceeded error handling

        Steps:
        1. Request presigned URL for file larger than quota
        2. Verify 413 Payload Too Large or quota error
        """
        # Try to upload a file larger than default quota (10GB)
        huge_file_size = 15 * 1024 * 1024 * 1024  # 15 GB
        checksum = "a" * 64

        request_data = {
            "filename": "huge-file.bin",
            "content_type": "application/octet-stream",
            "content_length": huge_file_size,
            "checksum_sha256": checksum,
            "folder": "uploads",
        }

        response = client.post(
            "/v1/uploads/presign",
            json=request_data,
            headers=auth_headers
        )

        # Should return error (either 413 or 400 with quota error)
        assert response.status_code in [
            status.HTTP_400_BAD_REQUEST,
            status.HTTP_413_REQUEST_ENTITY_TOO_LARGE
        ]

        if response.status_code == status.HTTP_400_BAD_REQUEST:
            data = response.json()
            assert "detail" in data
            # Error message should mention quota
            assert "quota" in data["detail"].lower() or "exceeded" in data["detail"].lower()

    def test_invalid_request_validation(self, client, auth_headers):
        """
        Test 10: Invalid request validation

        Test cases:
        1. Missing required fields
        2. Invalid field types
        3. Invalid field values
        """
        # Missing required field (filename)
        invalid_data = {
            "content_type": "text/plain",
            "content_length": 1024,
        }

        response = client.post(
            "/v1/uploads/presign",
            json=invalid_data,
            headers=auth_headers
        )

        assert response.status_code == status.HTTP_422_UNPROCESSABLE_ENTITY

        # Invalid field type (content_length should be int)
        invalid_data = {
            "filename": "test.txt",
            "content_type": "text/plain",
            "content_length": "not-a-number",
        }

        response = client.post(
            "/v1/uploads/presign",
            json=invalid_data,
            headers=auth_headers
        )

        assert response.status_code == status.HTTP_422_UNPROCESSABLE_ENTITY

        # Invalid folder value
        invalid_data = {
            "filename": "test.txt",
            "content_type": "text/plain",
            "content_length": 1024,
            "folder": "invalid-folder",  # Only uploads, avatars, documents, temp allowed
        }

        response = client.post(
            "/v1/uploads/presign",
            json=invalid_data,
            headers=auth_headers
        )

        assert response.status_code == status.HTTP_422_UNPROCESSABLE_ENTITY

    def test_invalid_subscription_update(
        self, client, auth_headers
    ):
        """
        Test: Update non-existent subscription

        Steps:
        1. Try to update subscription that doesn't exist
        2. Verify 404 Not Found
        """
        update_data = {
            "price_id": "price_pro_monthly",
        }

        response = client.patch(
            "/v1/payments/subscriptions/sub_nonexistent",
            json=update_data,
            headers=auth_headers
        )

        assert response.status_code == status.HTTP_404_NOT_FOUND

    def test_invalid_payment_method(
        self, client, auth_headers, test_customer
    ):
        """
        Test: Create subscription with invalid payment method

        Steps:
        1. Try to create subscription with non-existent payment method
        2. Verify error response
        """
        subscription_data = {
            "price_id": "price_starter_monthly",
            "payment_method_id": "pm_nonexistent",
        }

        response = client.post(
            f"/v1/payments/subscriptions?customer_id={test_customer['id']}",
            json=subscription_data,
            headers=auth_headers
        )

        # Should return error (400 or 404)
        assert response.status_code in [
            status.HTTP_400_BAD_REQUEST,
            status.HTTP_404_NOT_FOUND
        ]

    def test_invalid_price_id(
        self, client, auth_headers, test_customer, test_payment_method, mock_stripe_client
    ):
        """
        Test: Create subscription with invalid price ID

        Steps:
        1. Try to create subscription with non-existent price
        2. Verify error response
        """
        # Attach payment method
        mock_stripe_client.attach_payment_method(
            test_payment_method["id"],
            test_customer["id"]
        )

        subscription_data = {
            "price_id": "price_nonexistent",
            "payment_method_id": test_payment_method["id"],
        }

        response = client.post(
            f"/v1/payments/subscriptions?customer_id={test_customer['id']}",
            json=subscription_data,
            headers=auth_headers
        )

        assert response.status_code == status.HTTP_400_BAD_REQUEST
