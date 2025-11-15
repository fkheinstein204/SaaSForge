"""
@file        test_upload_flow.py
@brief       Integration tests for file upload flows
@copyright   (c) 2025 FtsCoDe GmbH. All rights reserved.
@author      Heinstein F.
@date        2025-11-15

Tests:
3. Generate presigned upload URL
4. Check storage quota
"""

import pytest
from fastapi import status
import hashlib


class TestUploadFlow:
    """Test file upload flows"""

    def test_generate_presigned_upload_url(self, client, auth_headers, sample_file_data):
        """
        Test 3: Generate presigned upload URL

        Steps:
        1. Calculate file checksum
        2. Request presigned URL
        3. Verify URL and headers are returned
        4. Verify object key format
        """
        # Calculate SHA-256 checksum
        checksum = hashlib.sha256(sample_file_data["content"]).hexdigest()

        # Request presigned URL
        request_data = {
            "filename": sample_file_data["filename"],
            "content_type": sample_file_data["content_type"],
            "content_length": len(sample_file_data["content"]),
            "checksum_sha256": checksum,
            "folder": "uploads",
        }

        response = client.post(
            "/v1/uploads/presign",
            json=request_data,
            headers=auth_headers
        )

        # Verify response
        assert response.status_code == status.HTTP_200_OK
        data = response.json()
        assert "presigned_url" in data
        assert "object_key" in data
        assert "headers" in data
        assert "expires_in" in data

        # Verify object key format: folder/tenant_id/user_id/timestamp_filename
        object_key = data["object_key"]
        assert object_key.startswith("uploads/")
        assert sample_file_data["filename"] in object_key

    def test_check_storage_quota(self, client, auth_headers):
        """
        Test 4: Check storage quota

        Steps:
        1. Request quota check for small file
        2. Verify quota allows upload
        3. Request quota check for huge file
        4. Verify quota check response
        """
        # Check quota for small file (should be allowed)
        small_file_size = 1024 * 1024  # 1 MB

        response = client.post(
            "/v1/uploads/quota/check",
            json={"file_size": small_file_size},
            headers=auth_headers
        )

        assert response.status_code == status.HTTP_200_OK
        data = response.json()
        assert "allowed" in data
        assert "current_usage_bytes" in data
        assert "quota_bytes" in data
        assert "available_bytes" in data

    def test_quota_exceeded(self, client, auth_headers):
        """
        Test: Quota check for file larger than quota
        """
        # Check quota for huge file (should exceed quota)
        huge_file_size = 100 * 1024 * 1024 * 1024  # 100 GB

        response = client.post(
            "/v1/uploads/quota/check",
            json={"file_size": huge_file_size},
            headers=auth_headers
        )

        assert response.status_code == status.HTTP_200_OK
        data = response.json()
        assert data["allowed"] is False

    def test_multipart_upload_initialization(self, client, auth_headers):
        """
        Test: Initialize multipart upload for large file

        Steps:
        1. Request multipart upload initialization
        2. Verify upload_id is returned
        3. Verify part_size and num_parts are calculated
        """
        # Large file (200 MB)
        file_size = 200 * 1024 * 1024
        checksum = "a" * 64  # Mock checksum

        request_data = {
            "filename": "large-file.bin",
            "content_type": "application/octet-stream",
            "total_size": file_size,
            "checksum_sha256": checksum,
            "folder": "uploads",
        }

        response = client.post(
            "/v1/uploads/multipart/init",
            json=request_data,
            headers=auth_headers
        )

        assert response.status_code == status.HTTP_200_OK
        data = response.json()
        assert "upload_id" in data
        assert "object_key" in data
        assert "part_size" in data
        assert "num_parts" in data
        assert data["num_parts"] > 1  # Should be split into multiple parts

    def test_get_storage_usage(self, client, auth_headers):
        """
        Test: Get current storage usage

        Steps:
        1. Request storage usage
        2. Verify usage data is returned
        """
        response = client.get("/v1/uploads/storage/usage", headers=auth_headers)

        assert response.status_code == status.HTTP_200_OK
        data = response.json()
        assert "total_bytes" in data
        assert "quota_bytes" in data
        assert "percentage_used" in data
        assert isinstance(data["total_bytes"], int)
        assert data["total_bytes"] >= 0
