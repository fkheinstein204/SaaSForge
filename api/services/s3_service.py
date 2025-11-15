"""
@file        s3_service.py
@brief       S3 service for presigned URLs, uploads, and file management
@copyright   (c) 2025 FtsCoDe GmbH. All rights reserved.
@author      Heinstein F.
@date        2025-11-15

@details
Provides presigned URL generation, multipart upload support,
quota checking, and file metadata management.
"""

import boto3
import hashlib
import os
from typing import Optional, List, Dict, Any
from datetime import datetime, timedelta
from botocore.config import Config
from botocore.exceptions import ClientError
import mimetypes


class S3Service:
    """S3 service for upload management"""

    # Singleton instance
    _instance = None

    def __new__(cls):
        if cls._instance is None:
            cls._instance = super().__new__(cls)
            cls._instance._initialized = False
        return cls._instance

    def __init__(self):
        if self._initialized:
            return

        # S3 configuration from environment
        self.bucket_name = os.getenv("S3_BUCKET", "saasforge-uploads-dev")
        self.region = os.getenv("S3_REGION", "us-east-1")
        self.endpoint_url = os.getenv("S3_ENDPOINT")  # None for real AWS, URL for LocalStack

        # Create S3 client
        config = Config(
            region_name=self.region,
            signature_version='s3v4',
            s3={'addressing_style': 'path'}  # Required for LocalStack
        )

        client_kwargs = {
            'region_name': self.region,
            'config': config,
        }

        # Add endpoint URL if using LocalStack
        if self.endpoint_url:
            client_kwargs['endpoint_url'] = self.endpoint_url

        # Add credentials from environment
        aws_access_key = os.getenv("AWS_ACCESS_KEY_ID")
        aws_secret_key = os.getenv("AWS_SECRET_ACCESS_KEY")
        if aws_access_key and aws_secret_key:
            client_kwargs['aws_access_key_id'] = aws_access_key
            client_kwargs['aws_secret_access_key'] = aws_secret_key

        self.s3_client = boto3.client('s3', **client_kwargs)

        # File size limits
        self.max_file_size = 5 * 1024 * 1024 * 1024  # 5 GB
        self.multipart_threshold = 100 * 1024 * 1024  # 100 MB
        self.min_part_size = 5 * 1024 * 1024  # 5 MB (S3 minimum)

        # Allowed content types (can be extended)
        self.allowed_content_types = {
            # Images
            'image/jpeg', 'image/png', 'image/gif', 'image/webp', 'image/svg+xml',
            # Documents
            'application/pdf',
            'application/msword',
            'application/vnd.openxmlformats-officedocument.wordprocessingml.document',
            'application/vnd.ms-excel',
            'application/vnd.openxmlformats-officedocument.spreadsheetml.sheet',
            'application/vnd.ms-powerpoint',
            'application/vnd.openxmlformats-officedocument.presentationml.presentation',
            # Text
            'text/plain', 'text/csv', 'text/html', 'text/css', 'text/javascript',
            # Archives
            'application/zip', 'application/x-tar', 'application/gzip',
            # Media
            'video/mp4', 'video/mpeg', 'video/quicktime',
            'audio/mpeg', 'audio/wav', 'audio/ogg',
            # Other
            'application/json', 'application/xml',
        }

        self._initialized = True

    def validate_content_type(self, content_type: str) -> bool:
        """
        Validate if content type is allowed

        Args:
            content_type: MIME type to validate

        Returns:
            True if allowed, False otherwise
        """
        return content_type.lower() in self.allowed_content_types

    def generate_object_key(
        self,
        tenant_id: str,
        user_id: str,
        filename: str,
        folder: str = "uploads"
    ) -> str:
        """
        Generate S3 object key with tenant/user isolation

        Args:
            tenant_id: Tenant ID
            user_id: User ID
            filename: Original filename
            folder: Folder prefix (uploads, avatars, documents, etc.)

        Returns:
            S3 object key: {folder}/{tenant_id}/{user_id}/{timestamp}_{filename}
        """
        # Add timestamp to ensure uniqueness
        timestamp = datetime.utcnow().strftime("%Y%m%d%H%M%S")
        safe_filename = filename.replace(" ", "_")

        return f"{folder}/{tenant_id}/{user_id}/{timestamp}_{safe_filename}"

    async def generate_presigned_upload_url(
        self,
        tenant_id: str,
        user_id: str,
        filename: str,
        content_type: str,
        content_length: int,
        checksum_sha256: Optional[str] = None,
        folder: str = "uploads",
        expires_in: int = 600  # 10 minutes
    ) -> Dict[str, Any]:
        """
        Generate presigned URL for direct upload to S3

        Args:
            tenant_id: Tenant ID for isolation
            user_id: User ID for isolation
            filename: Original filename
            content_type: MIME type
            content_length: File size in bytes
            checksum_sha256: Optional SHA-256 checksum for validation
            folder: Folder prefix
            expires_in: URL expiration in seconds (default: 600s = 10min)

        Returns:
            Dict with presigned_url, object_key, expires_at

        Raises:
            ValueError: If content type not allowed or file too large
        """
        # Validate content type
        if not self.validate_content_type(content_type):
            raise ValueError(f"Content type not allowed: {content_type}")

        # Validate file size
        if content_length > self.max_file_size:
            raise ValueError(
                f"File too large: {content_length} bytes (max: {self.max_file_size})"
            )

        # Generate object key
        object_key = self.generate_object_key(tenant_id, user_id, filename, folder)

        # Build parameters
        params = {
            'Bucket': self.bucket_name,
            'Key': object_key,
            'ContentType': content_type,
            'ContentLength': content_length,
            'Metadata': {
                'tenant-id': tenant_id,
                'user-id': user_id,
                'original-filename': filename,
            }
        }

        # Add checksum if provided
        if checksum_sha256:
            params['Metadata']['checksum-sha256'] = checksum_sha256

        # Generate presigned URL
        presigned_url = self.s3_client.generate_presigned_url(
            'put_object',
            Params=params,
            ExpiresIn=expires_in,
        )

        expires_at = datetime.utcnow() + timedelta(seconds=expires_in)

        return {
            'presigned_url': presigned_url,
            'object_key': object_key,
            'bucket': self.bucket_name,
            'expires_at': expires_at.isoformat(),
            'expires_in': expires_in,
            'method': 'PUT',
            'headers': {
                'Content-Type': content_type,
                'Content-Length': str(content_length),
            }
        }

    async def initiate_multipart_upload(
        self,
        tenant_id: str,
        user_id: str,
        filename: str,
        content_type: str,
        total_size: int,
        checksum_sha256: Optional[str] = None,
        folder: str = "uploads"
    ) -> Dict[str, Any]:
        """
        Initiate multipart upload for large files

        Args:
            tenant_id: Tenant ID
            user_id: User ID
            filename: Original filename
            content_type: MIME type
            total_size: Total file size in bytes
            checksum_sha256: Optional SHA-256 checksum
            folder: Folder prefix

        Returns:
            Dict with upload_id, object_key, part_size, num_parts
        """
        # Validate content type
        if not self.validate_content_type(content_type):
            raise ValueError(f"Content type not allowed: {content_type}")

        # Validate file size
        if total_size > self.max_file_size:
            raise ValueError(f"File too large: {total_size} bytes")

        # Generate object key
        object_key = self.generate_object_key(tenant_id, user_id, filename, folder)

        # Metadata
        metadata = {
            'tenant-id': tenant_id,
            'user-id': user_id,
            'original-filename': filename,
            'total-size': str(total_size),
        }

        if checksum_sha256:
            metadata['checksum-sha256'] = checksum_sha256

        # Initiate multipart upload
        response = self.s3_client.create_multipart_upload(
            Bucket=self.bucket_name,
            Key=object_key,
            ContentType=content_type,
            Metadata=metadata
        )

        upload_id = response['UploadId']

        # Calculate part size and number of parts
        # Use 100MB parts for files > 100MB, minimum 5MB per part
        part_size = max(self.min_part_size, min(100 * 1024 * 1024, total_size // 100))
        num_parts = (total_size + part_size - 1) // part_size

        return {
            'upload_id': upload_id,
            'object_key': object_key,
            'bucket': self.bucket_name,
            'part_size': part_size,
            'num_parts': num_parts,
            'total_size': total_size,
        }

    async def generate_presigned_part_urls(
        self,
        object_key: str,
        upload_id: str,
        num_parts: int,
        expires_in: int = 3600  # 1 hour for large uploads
    ) -> List[Dict[str, Any]]:
        """
        Generate presigned URLs for multipart upload parts

        Args:
            object_key: S3 object key
            upload_id: Multipart upload ID
            num_parts: Number of parts
            expires_in: URL expiration in seconds (default: 3600s = 1 hour)

        Returns:
            List of dicts with part_number, presigned_url, expires_at
        """
        presigned_urls = []

        for part_number in range(1, num_parts + 1):
            url = self.s3_client.generate_presigned_url(
                'upload_part',
                Params={
                    'Bucket': self.bucket_name,
                    'Key': object_key,
                    'UploadId': upload_id,
                    'PartNumber': part_number,
                },
                ExpiresIn=expires_in,
            )

            expires_at = datetime.utcnow() + timedelta(seconds=expires_in)

            presigned_urls.append({
                'part_number': part_number,
                'presigned_url': url,
                'expires_at': expires_at.isoformat(),
                'method': 'PUT',
            })

        return presigned_urls

    async def complete_multipart_upload(
        self,
        object_key: str,
        upload_id: str,
        parts: List[Dict[str, Any]]
    ) -> Dict[str, Any]:
        """
        Complete multipart upload

        Args:
            object_key: S3 object key
            upload_id: Multipart upload ID
            parts: List of dicts with PartNumber and ETag

        Returns:
            Dict with object_key, bucket, location, etag
        """
        # Sort parts by part number
        sorted_parts = sorted(parts, key=lambda p: p['PartNumber'])

        response = self.s3_client.complete_multipart_upload(
            Bucket=self.bucket_name,
            Key=object_key,
            UploadId=upload_id,
            MultipartUpload={'Parts': sorted_parts}
        )

        return {
            'object_key': object_key,
            'bucket': self.bucket_name,
            'location': response['Location'],
            'etag': response['ETag'],
        }

    async def abort_multipart_upload(
        self,
        object_key: str,
        upload_id: str
    ):
        """
        Abort multipart upload (cleanup on failure)

        Args:
            object_key: S3 object key
            upload_id: Multipart upload ID
        """
        self.s3_client.abort_multipart_upload(
            Bucket=self.bucket_name,
            Key=object_key,
            UploadId=upload_id
        )

    async def generate_presigned_download_url(
        self,
        object_key: str,
        expires_in: int = 600,  # 10 minutes
        filename: Optional[str] = None
    ) -> Dict[str, Any]:
        """
        Generate presigned URL for downloading a file

        Args:
            object_key: S3 object key
            expires_in: URL expiration in seconds
            filename: Optional filename for Content-Disposition header

        Returns:
            Dict with presigned_url, expires_at
        """
        params = {
            'Bucket': self.bucket_name,
            'Key': object_key,
        }

        # Add Content-Disposition header for download with custom filename
        if filename:
            params['ResponseContentDisposition'] = f'attachment; filename="{filename}"'

        presigned_url = self.s3_client.generate_presigned_url(
            'get_object',
            Params=params,
            ExpiresIn=expires_in,
        )

        expires_at = datetime.utcnow() + timedelta(seconds=expires_in)

        return {
            'presigned_url': presigned_url,
            'object_key': object_key,
            'expires_at': expires_at.isoformat(),
            'expires_in': expires_in,
        }

    async def get_object_metadata(self, object_key: str) -> Dict[str, Any]:
        """
        Get object metadata

        Args:
            object_key: S3 object key

        Returns:
            Dict with metadata, content_type, content_length, last_modified, etag
        """
        try:
            response = self.s3_client.head_object(
                Bucket=self.bucket_name,
                Key=object_key
            )

            return {
                'object_key': object_key,
                'content_type': response.get('ContentType'),
                'content_length': response.get('ContentLength'),
                'last_modified': response.get('LastModified').isoformat() if response.get('LastModified') else None,
                'etag': response.get('ETag'),
                'metadata': response.get('Metadata', {}),
            }
        except ClientError as e:
            if e.response['Error']['Code'] == '404':
                raise ValueError(f"Object not found: {object_key}")
            raise

    async def delete_object(self, object_key: str):
        """
        Delete an object from S3

        Args:
            object_key: S3 object key
        """
        self.s3_client.delete_object(
            Bucket=self.bucket_name,
            Key=object_key
        )

    async def calculate_user_storage(
        self,
        tenant_id: str,
        user_id: str
    ) -> Dict[str, Any]:
        """
        Calculate total storage used by a user

        Args:
            tenant_id: Tenant ID
            user_id: User ID

        Returns:
            Dict with total_size, object_count, by_folder
        """
        prefix = f"uploads/{tenant_id}/{user_id}/"

        paginator = self.s3_client.get_paginator('list_objects_v2')
        pages = paginator.paginate(Bucket=self.bucket_name, Prefix=prefix)

        total_size = 0
        object_count = 0
        by_folder = {}

        for page in pages:
            for obj in page.get('Contents', []):
                size = obj['Size']
                total_size += size
                object_count += 1

                # Track by folder
                key_parts = obj['Key'].split('/')
                if len(key_parts) > 0:
                    folder = key_parts[0]
                    by_folder[folder] = by_folder.get(folder, 0) + size

        return {
            'total_size': total_size,
            'object_count': object_count,
            'by_folder': by_folder,
            'tenant_id': tenant_id,
            'user_id': user_id,
        }

    async def check_quota(
        self,
        tenant_id: str,
        user_id: str,
        file_size: int,
        quota_bytes: int
    ) -> bool:
        """
        Check if user has enough quota for upload

        Args:
            tenant_id: Tenant ID
            user_id: User ID
            file_size: Size of file to upload
            quota_bytes: User's quota in bytes

        Returns:
            True if enough quota, False otherwise
        """
        storage = await self.calculate_user_storage(tenant_id, user_id)
        current_usage = storage['total_size']

        # Allow 10% grace overage for paid plans (configurable)
        grace_quota = quota_bytes * 1.1

        return (current_usage + file_size) <= grace_quota


def get_s3_service() -> S3Service:
    """
    Get S3 service singleton instance

    Returns:
        S3Service instance
    """
    return S3Service()
