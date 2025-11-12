"""gRPC client wrapper for Upload Service."""

import grpc
import os
from typing import List
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'generated'))

from upload_pb2 import (
    PresignedUrlRequest, PresignedUrlResponse,
    CompleteUploadRequest, CompleteUploadResponse,
    TransformRequest, TransformResponse,
    DeleteObjectRequest, DeleteObjectResponse,
    GetQuotaRequest, GetQuotaResponse
)
from upload_pb2_grpc import UploadServiceStub


class UploadClient:
    """Client for Upload Service gRPC calls."""

    def __init__(self, host: str = None, port: int = None):
        self.host = host or os.getenv("UPLOAD_SERVICE_HOST", "localhost")
        self.port = port or int(os.getenv("UPLOAD_SERVICE_PORT", "50052"))
        self.address = f"{self.host}:{self.port}"

        # Load mTLS certificates for secure service-to-service communication
        ca_cert_path = os.getenv("GRPC_CA_CERT_PATH", "certs/ca.crt")
        client_cert_path = os.getenv("GRPC_CLIENT_CERT_PATH", "certs/client.crt")
        client_key_path = os.getenv("GRPC_CLIENT_KEY_PATH", "certs/client.key")

        try:
            with open(ca_cert_path, "rb") as f:
                ca_cert = f.read()
            with open(client_cert_path, "rb") as f:
                client_cert = f.read()
            with open(client_key_path, "rb") as f:
                client_key = f.read()

            credentials = grpc.ssl_channel_credentials(
                root_certificates=ca_cert,
                private_key=client_key,
                certificate_chain=client_cert
            )

            self.channel = grpc.secure_channel(self.address, credentials)
        except FileNotFoundError as e:
            print(f"WARNING: mTLS certs not found ({e}), using insecure channel")
            self.channel = grpc.insecure_channel(self.address)

        self.stub = UploadServiceStub(self.channel)

    def generate_presigned_url(
        self,
        filename: str,
        content_length: int,
        content_type: str,
        metadata: List[tuple]
    ) -> PresignedUrlResponse:
        """Generate presigned URL for file upload."""
        request = PresignedUrlRequest(
            filename=filename,
            content_length=content_length,
            content_type=content_type
        )
        return self.stub.GeneratePresignedUrl(request, metadata=metadata)

    def complete_upload(
        self,
        upload_id: str,
        etag: str,
        metadata: List[tuple]
    ) -> CompleteUploadResponse:
        """Mark upload as completed."""
        request = CompleteUploadRequest(
            upload_id=upload_id,
            etag=etag
        )
        return self.stub.CompleteUpload(request, metadata=metadata)

    def transform_object(
        self,
        object_id: str,
        profile_id: str,
        metadata: List[tuple]
    ) -> TransformResponse:
        """Transform uploaded object (resize, compress, etc)."""
        request = TransformRequest(
            object_id=object_id,
            profile_id=profile_id
        )
        return self.stub.TransformObject(request, metadata=metadata)

    def delete_object(
        self,
        object_id: str,
        metadata: List[tuple]
    ) -> DeleteObjectResponse:
        """Delete uploaded object."""
        request = DeleteObjectRequest(object_id=object_id)
        return self.stub.DeleteObject(request, metadata=metadata)

    def get_quota(self, metadata: List[tuple]) -> GetQuotaResponse:
        """Get storage quota for tenant."""
        request = GetQuotaRequest()
        return self.stub.GetQuota(request, metadata=metadata)

    def close(self):
        """Close gRPC channel."""
        if self.channel:
            self.channel.close()

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()
