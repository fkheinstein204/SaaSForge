"""
@file        upload.py
@brief       Upload-related Pydantic models
@copyright   (c) 2025 FtsCoDe GmbH. All rights reserved.
@author      Heinstein F.
@date        2025-11-15
"""

from pydantic import BaseModel, Field, validator
from typing import Optional, List, Dict, Any
from datetime import datetime


class PresignedUploadRequest(BaseModel):
    """Request for presigned upload URL"""
    filename: str = Field(..., min_length=1, max_length=255)
    content_type: str = Field(..., min_length=1)
    content_length: int = Field(..., gt=0, le=5*1024*1024*1024)  # Max 5GB
    checksum_sha256: Optional[str] = Field(None, min_length=64, max_length=64)
    folder: str = Field(default="uploads", pattern="^(uploads|avatars|documents|temp)$")

    @validator('filename')
    def validate_filename(cls, v):
        """Validate filename doesn't contain path traversal"""
        if '..' in v or '/' in v or '\\' in v:
            raise ValueError("Filename cannot contain path traversal characters")
        return v


class PresignedUploadResponse(BaseModel):
    """Response with presigned upload URL"""
    presigned_url: str
    object_key: str
    bucket: str
    expires_at: str
    expires_in: int
    method: str
    headers: Dict[str, str]


class MultipartUploadInitRequest(BaseModel):
    """Request to initiate multipart upload"""
    filename: str = Field(..., min_length=1, max_length=255)
    content_type: str = Field(..., min_length=1)
    total_size: int = Field(..., gt=100*1024*1024, le=5*1024*1024*1024)  # 100MB - 5GB
    checksum_sha256: Optional[str] = Field(None, min_length=64, max_length=64)
    folder: str = Field(default="uploads", pattern="^(uploads|avatars|documents|temp)$")

    @validator('filename')
    def validate_filename(cls, v):
        """Validate filename doesn't contain path traversal"""
        if '..' in v or '/' in v or '\\' in v:
            raise ValueError("Filename cannot contain path traversal characters")
        return v


class MultipartUploadInitResponse(BaseModel):
    """Response after initiating multipart upload"""
    upload_id: str
    object_key: str
    bucket: str
    part_size: int
    num_parts: int
    total_size: int


class MultipartPartUrl(BaseModel):
    """Presigned URL for a single part"""
    part_number: int
    presigned_url: str
    expires_at: str
    method: str


class MultipartPartUrlsResponse(BaseModel):
    """Response with presigned URLs for all parts"""
    upload_id: str
    object_key: str
    parts: List[MultipartPartUrl]


class MultipartPart(BaseModel):
    """Part info for completing multipart upload"""
    PartNumber: int = Field(..., alias="part_number")
    ETag: str = Field(..., alias="etag")

    class Config:
        populate_by_name = True


class MultipartUploadCompleteRequest(BaseModel):
    """Request to complete multipart upload"""
    upload_id: str
    object_key: str
    parts: List[MultipartPart]


class MultipartUploadCompleteResponse(BaseModel):
    """Response after completing multipart upload"""
    object_key: str
    bucket: str
    location: str
    etag: str


class MultipartUploadAbortRequest(BaseModel):
    """Request to abort multipart upload"""
    upload_id: str
    object_key: str


class PresignedDownloadRequest(BaseModel):
    """Request for presigned download URL"""
    object_key: str = Field(..., min_length=1)
    filename: Optional[str] = Field(None, max_length=255)
    expires_in: int = Field(default=600, gt=0, le=3600)  # 1-3600 seconds


class PresignedDownloadResponse(BaseModel):
    """Response with presigned download URL"""
    presigned_url: str
    object_key: str
    expires_at: str
    expires_in: int


class ObjectMetadataResponse(BaseModel):
    """Object metadata response"""
    object_key: str
    content_type: Optional[str]
    content_length: Optional[int]
    last_modified: Optional[str]
    etag: Optional[str]
    metadata: Dict[str, str]


class StorageUsageResponse(BaseModel):
    """User storage usage response"""
    total_size: int
    object_count: int
    by_folder: Dict[str, int]
    tenant_id: str
    user_id: str
    quota_bytes: Optional[int] = None
    quota_percentage: Optional[float] = None


class QuotaCheckRequest(BaseModel):
    """Request to check quota before upload"""
    file_size: int = Field(..., gt=0)


class QuotaCheckResponse(BaseModel):
    """Quota check response"""
    allowed: bool
    current_usage: int
    quota_bytes: int
    available_bytes: int
    file_size: int
    would_exceed: bool


# Legacy models for backward compatibility
class PresignedUrlRequest(BaseModel):
    """Legacy request model"""
    filename: str = Field(..., min_length=1, max_length=255)
    content_type: str
    content_length: int = Field(..., gt=0)
    checksum_sha256: str = Field(..., min_length=64, max_length=64)
    metadata: Optional[Dict[str, str]] = None


class PresignedUrlResponse(BaseModel):
    """Legacy response model"""
    url: str
    upload_id: str
    expires_in: int
    required_headers: Dict[str, str]


class QuotaResponse(BaseModel):
    """Quota response"""
    tenant_id: str
    used_bytes: int
    limit_bytes: int
    remaining_bytes: int
    reset_at: datetime
