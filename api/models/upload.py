"""
Upload models
"""
from pydantic import BaseModel, Field
from typing import Optional, Dict
from datetime import datetime


class PresignedUrlRequest(BaseModel):
    filename: str = Field(..., min_length=1, max_length=255)
    content_type: str
    content_length: int = Field(..., gt=0)
    checksum_sha256: str = Field(..., min_length=64, max_length=64)
    metadata: Optional[Dict[str, str]] = None


class PresignedUrlResponse(BaseModel):
    url: str
    upload_id: str
    expires_in: int  # seconds (600 = 10 minutes)
    required_headers: Dict[str, str]


class CompleteUploadRequest(BaseModel):
    upload_id: str
    etag: str


class CompleteUploadResponse(BaseModel):
    object_id: str
    url: str
    size: int
    checksum_sha256: str


class TransformRequest(BaseModel):
    object_id: str
    profile_id: str
    output_format: Optional[str] = None


class TransformResponse(BaseModel):
    job_id: str
    status: str
    estimated_completion: Optional[datetime] = None


class QuotaResponse(BaseModel):
    tenant_id: str
    used_bytes: int
    limit_bytes: int
    remaining_bytes: int
    reset_at: datetime
