"""
@file        upload.py
@brief       Upload router for S3 presigned URLs and file management
@copyright   (c) 2025 FtsCoDe GmbH. All rights reserved.
@author      Heinstein F.
@date        2025-11-15

@details
Handles:
- Presigned URL generation for uploads
- Multipart upload support for large files
- Presigned download URLs
- Storage quota checking
- File metadata retrieval
"""

from fastapi import APIRouter, Depends, HTTPException, status, Header
from typing import Optional
from models.upload import (
    PresignedUploadRequest, PresignedUploadResponse,
    MultipartUploadInitRequest, MultipartUploadInitResponse,
    MultipartPartUrlsResponse,
    MultipartUploadCompleteRequest, MultipartUploadCompleteResponse,
    MultipartUploadAbortRequest,
    PresignedDownloadResponse,
    ObjectMetadataResponse,
    StorageUsageResponse,
    QuotaCheckRequest, QuotaCheckResponse,
)
from services.s3_service import get_s3_service, S3Service
from dependencies.auth import get_current_user, UserContext

router = APIRouter()


@router.post("/presign", response_model=PresignedUploadResponse)
async def generate_presigned_upload(
    request: PresignedUploadRequest,
    user: UserContext = Depends(get_current_user),
    s3_service: S3Service = Depends(get_s3_service)
):
    """
    Generate presigned URL for direct S3 upload

    **Flow:**
    1. Validate JWT and extract user/tenant
    2. Check storage quota
    3. Generate presigned URL with 10-minute expiry
    4. Return URL with required headers

    **Security:**
    - Content-Type enforcement
    - Content-Length enforcement
    - Optional SHA-256 checksum validation
    - Tenant/user isolation via object key prefix
    """
    user_id, tenant_id = user.user_id, user.tenant_id

    # Check quota (default: 10GB per user)
    quota_bytes = 10 * 1024 * 1024 * 1024  # 10 GB
    has_quota = await s3_service.check_quota(
        tenant_id=tenant_id,
        user_id=user_id,
        file_size=request.content_length,
        quota_bytes=quota_bytes
    )

    if not has_quota:
        storage = await s3_service.calculate_user_storage(tenant_id, user_id)
        raise HTTPException(
            status_code=status.HTTP_413_REQUEST_ENTITY_TOO_LARGE,
            detail={
                "code": "QUOTA_EXCEEDED",
                "message": "Storage quota exceeded",
                "current_usage": storage['total_size'],
                "quota_bytes": quota_bytes,
                "file_size": request.content_length,
            }
        )

    try:
        result = await s3_service.generate_presigned_upload_url(
            tenant_id=tenant_id,
            user_id=user_id,
            filename=request.filename,
            content_type=request.content_type,
            content_length=request.content_length,
            checksum_sha256=request.checksum_sha256,
            folder=request.folder,
        )

        return PresignedUploadResponse(**result)

    except ValueError as e:
        raise HTTPException(
            status_code=status.HTTP_400_BAD_REQUEST,
            detail=str(e)
        )
    except Exception as e:
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail=f"Failed to generate presigned URL: {str(e)}"
        )


@router.post("/multipart/init", response_model=MultipartUploadInitResponse)
async def initiate_multipart_upload(
    request: MultipartUploadInitRequest,
    user: UserContext = Depends(get_current_user),
    s3_service: S3Service = Depends(get_s3_service)
):
    """
    Initiate multipart upload for large files (>100MB)

    **Flow:**
    1. Validate JWT and extract user/tenant
    2. Check storage quota
    3. Create multipart upload in S3
    4. Return upload_id and part configuration

    **Usage:**
    - For files >100MB (recommended)
    - Supports files up to 5GB
    - Parts uploaded individually with presigned URLs
    """
    user_id, tenant_id = user.user_id, user.tenant_id

    # Check quota
    quota_bytes = 10 * 1024 * 1024 * 1024  # 10 GB
    has_quota = await s3_service.check_quota(
        tenant_id=tenant_id,
        user_id=user_id,
        file_size=request.total_size,
        quota_bytes=quota_bytes
    )

    if not has_quota:
        storage = await s3_service.calculate_user_storage(tenant_id, user_id)
        raise HTTPException(
            status_code=status.HTTP_413_REQUEST_ENTITY_TOO_LARGE,
            detail={
                "code": "QUOTA_EXCEEDED",
                "message": "Storage quota exceeded",
                "current_usage": storage['total_size'],
                "quota_bytes": quota_bytes,
                "file_size": request.total_size,
            }
        )

    try:
        result = await s3_service.initiate_multipart_upload(
            tenant_id=tenant_id,
            user_id=user_id,
            filename=request.filename,
            content_type=request.content_type,
            total_size=request.total_size,
            checksum_sha256=request.checksum_sha256,
            folder=request.folder,
        )

        return MultipartUploadInitResponse(**result)

    except ValueError as e:
        raise HTTPException(
            status_code=status.HTTP_400_BAD_REQUEST,
            detail=str(e)
        )
    except Exception as e:
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail=f"Failed to initiate multipart upload: {str(e)}"
        )


@router.get("/multipart/{upload_id}/parts", response_model=MultipartPartUrlsResponse)
async def get_multipart_part_urls(
    upload_id: str,
    object_key: str,
    num_parts: int,
    user: UserContext = Depends(get_current_user),
    s3_service: S3Service = Depends(get_s3_service)
):
    """
    Get presigned URLs for uploading multipart parts

    **Flow:**
    1. Validate JWT
    2. Generate presigned URLs for each part (1-hour expiry)
    3. Return array of URLs

    **Client Usage:**
    ```javascript
    // Upload each part
    for (let i = 0; i < parts.length; i++) {
      const response = await fetch(parts[i].presigned_url, {
        method: 'PUT',
        body: partData[i],
      });
      etags[i] = response.headers.get('ETag');
    }
    ```
    """
    user_id, tenant_id = user.user_id, user.tenant_id

    try:
        parts = await s3_service.generate_presigned_part_urls(
            object_key=object_key,
            upload_id=upload_id,
            num_parts=num_parts,
        )

        return MultipartPartUrlsResponse(
            upload_id=upload_id,
            object_key=object_key,
            parts=parts
        )

    except Exception as e:
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail=f"Failed to generate part URLs: {str(e)}"
        )


@router.post("/multipart/complete", response_model=MultipartUploadCompleteResponse)
async def complete_multipart_upload(
    request: MultipartUploadCompleteRequest,
    user: UserContext = Depends(get_current_user),
    s3_service: S3Service = Depends(get_s3_service)
):
    """
    Complete multipart upload after all parts uploaded

    **Flow:**
    1. Validate JWT
    2. Verify all parts uploaded
    3. Combine parts into final object
    4. Return object details

    **Request Body:**
    ```json
    {
      "upload_id": "...",
      "object_key": "...",
      "parts": [
        {"part_number": 1, "etag": "abc123"},
        {"part_number": 2, "etag": "def456"}
      ]
    }
    ```
    """
    user_id, tenant_id = user.user_id, user.tenant_id

    try:
        # Convert parts to S3 format
        s3_parts = [
            {
                'PartNumber': part.PartNumber,
                'ETag': part.ETag.strip('"'),  # Remove quotes from ETag
            }
            for part in request.parts
        ]

        result = await s3_service.complete_multipart_upload(
            object_key=request.object_key,
            upload_id=request.upload_id,
            parts=s3_parts
        )

        return MultipartUploadCompleteResponse(**result)

    except Exception as e:
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail=f"Failed to complete multipart upload: {str(e)}"
        )


@router.post("/multipart/abort", status_code=status.HTTP_204_NO_CONTENT)
async def abort_multipart_upload(
    request: MultipartUploadAbortRequest,
    user: UserContext = Depends(get_current_user),
    s3_service: S3Service = Depends(get_s3_service)
):
    """
    Abort multipart upload (cleanup on failure/cancel)

    **Flow:**
    1. Validate JWT
    2. Delete all uploaded parts
    3. Cancel upload

    **Use Cases:**
    - User cancels upload
    - Upload errors/timeouts
    - Client disconnects
    """
    user_id, tenant_id = user.user_id, user.tenant_id

    try:
        await s3_service.abort_multipart_upload(
            object_key=request.object_key,
            upload_id=request.upload_id
        )

    except Exception as e:
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail=f"Failed to abort multipart upload: {str(e)}"
        )


@router.post("/download", response_model=PresignedDownloadResponse)
async def generate_presigned_download(
    object_key: str,
    filename: Optional[str] = None,
    expires_in: int = 600,
    user: UserContext = Depends(get_current_user),
    s3_service: S3Service = Depends(get_s3_service)
):
    """
    Generate presigned URL for downloading a file

    **Flow:**
    1. Validate JWT
    2. Verify object exists and user has access
    3. Generate presigned download URL (10-minute expiry default)

    **Security:**
    - User can only download their own files (tenant/user isolation)
    - Optional custom filename for Content-Disposition header
    """
    user_id, tenant_id = user.user_id, user.tenant_id

    # Verify object belongs to user (tenant/user isolation)
    expected_prefix = f"uploads/{tenant_id}/{user_id}/"
    if not object_key.startswith(expected_prefix):
        raise HTTPException(
            status_code=status.HTTP_403_FORBIDDEN,
            detail="Access denied: object does not belong to user"
        )

    try:
        result = await s3_service.generate_presigned_download_url(
            object_key=object_key,
            expires_in=expires_in,
            filename=filename
        )

        return PresignedDownloadResponse(**result)

    except ValueError as e:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=str(e)
        )
    except Exception as e:
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail=f"Failed to generate download URL: {str(e)}"
        )


@router.get("/metadata/{object_key:path}", response_model=ObjectMetadataResponse)
async def get_object_metadata(
    object_key: str,
    user: UserContext = Depends(get_current_user),
    s3_service: S3Service = Depends(get_s3_service)
):
    """
    Get object metadata (size, type, last modified, etc.)

    **Returns:**
    - content_type
    - content_length
    - last_modified
    - etag
    - custom metadata (tenant-id, user-id, etc.)
    """
    user_id, tenant_id = user.user_id, user.tenant_id

    # Verify object belongs to user
    expected_prefix = f"uploads/{tenant_id}/{user_id}/"
    if not object_key.startswith(expected_prefix):
        raise HTTPException(
            status_code=status.HTTP_403_FORBIDDEN,
            detail="Access denied"
        )

    try:
        metadata = await s3_service.get_object_metadata(object_key)
        return ObjectMetadataResponse(**metadata)

    except ValueError as e:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=str(e)
        )
    except Exception as e:
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail=f"Failed to get metadata: {str(e)}"
        )


@router.delete("/{object_key:path}", status_code=status.HTTP_204_NO_CONTENT)
async def delete_object(
    object_key: str,
    user: UserContext = Depends(get_current_user),
    s3_service: S3Service = Depends(get_s3_service)
):
    """
    Delete an object

    **Security:**
    - User can only delete their own files
    """
    user_id, tenant_id = user.user_id, user.tenant_id

    # Verify object belongs to user
    expected_prefix = f"uploads/{tenant_id}/{user_id}/"
    if not object_key.startswith(expected_prefix):
        raise HTTPException(
            status_code=status.HTTP_403_FORBIDDEN,
            detail="Access denied"
        )

    try:
        await s3_service.delete_object(object_key)

    except Exception as e:
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail=f"Failed to delete object: {str(e)}"
        )


@router.get("/storage/usage", response_model=StorageUsageResponse)
async def get_storage_usage(
    user: UserContext = Depends(get_current_user),
    s3_service: S3Service = Depends(get_s3_service)
):
    """
    Get current user's storage usage

    **Returns:**
    - total_size: Total bytes used
    - object_count: Number of objects
    - by_folder: Usage breakdown by folder
    - quota_bytes: User's quota limit
    - quota_percentage: Percentage of quota used
    """
    user_id, tenant_id = user.user_id, user.tenant_id

    try:
        usage = await s3_service.calculate_user_storage(tenant_id, user_id)

        # Add quota info
        quota_bytes = 10 * 1024 * 1024 * 1024  # 10 GB default
        quota_percentage = (usage['total_size'] / quota_bytes) * 100 if quota_bytes > 0 else 0

        return StorageUsageResponse(
            **usage,
            quota_bytes=quota_bytes,
            quota_percentage=round(quota_percentage, 2)
        )

    except Exception as e:
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail=f"Failed to calculate storage usage: {str(e)}"
        )


@router.post("/quota/check", response_model=QuotaCheckResponse)
async def check_quota(
    request: QuotaCheckRequest,
    user: UserContext = Depends(get_current_user),
    s3_service: S3Service = Depends(get_s3_service)
):
    """
    Check if user has enough quota for upload

    **Request:**
    ```json
    {
      "file_size": 104857600
    }
    ```

    **Response:**
    ```json
    {
      "allowed": true,
      "current_usage": 5368709120,
      "quota_bytes": 10737418240,
      "available_bytes": 5368709120,
      "file_size": 104857600,
      "would_exceed": false
    }
    ```
    """
    user_id, tenant_id = user.user_id, user.tenant_id

    try:
        quota_bytes = 10 * 1024 * 1024 * 1024  # 10 GB default
        storage = await s3_service.calculate_user_storage(tenant_id, user_id)
        current_usage = storage['total_size']

        allowed = await s3_service.check_quota(
            tenant_id=tenant_id,
            user_id=user_id,
            file_size=request.file_size,
            quota_bytes=quota_bytes
        )

        available_bytes = max(0, quota_bytes - current_usage)
        would_exceed = (current_usage + request.file_size) > quota_bytes

        return QuotaCheckResponse(
            allowed=allowed,
            current_usage=current_usage,
            quota_bytes=quota_bytes,
            available_bytes=available_bytes,
            file_size=request.file_size,
            would_exceed=would_exceed
        )

    except Exception as e:
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail=f"Failed to check quota: {str(e)}"
        )
