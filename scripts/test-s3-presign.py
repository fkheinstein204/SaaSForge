#!/usr/bin/env python3
"""
@file        test-s3-presign.py
@brief       Test LocalStack S3 connectivity and presigned URL generation
@author      Heinstein F.
@date        2025-11-15
"""

import boto3
import os
import hashlib
import sys
from datetime import datetime, timedelta

# LocalStack configuration
S3_ENDPOINT = os.getenv("S3_ENDPOINT", "http://localhost:4566")
S3_REGION = os.getenv("S3_REGION", "us-east-1")
S3_BUCKET = os.getenv("S3_BUCKET", "saasforge-uploads-dev")
AWS_ACCESS_KEY_ID = os.getenv("AWS_ACCESS_KEY_ID", "test")
AWS_SECRET_ACCESS_KEY = os.getenv("AWS_SECRET_ACCESS_KEY", "test")

def test_s3_connection():
    """Test basic S3 connectivity"""
    print("=" * 60)
    print("Testing LocalStack S3 Connection")
    print("=" * 60)

    try:
        # Create S3 client
        s3_client = boto3.client(
            's3',
            endpoint_url=S3_ENDPOINT,
            region_name=S3_REGION,
            aws_access_key_id=AWS_ACCESS_KEY_ID,
            aws_secret_access_key=AWS_SECRET_ACCESS_KEY,
        )

        # List buckets
        print(f"\nEndpoint: {S3_ENDPOINT}")
        print(f"Region: {S3_REGION}")
        print("\nListing S3 buckets...")
        response = s3_client.list_buckets()

        print(f"Found {len(response['Buckets'])} bucket(s):")
        for bucket in response['Buckets']:
            print(f"  - {bucket['Name']} (created: {bucket['CreationDate']})")

        # Verify our bucket exists
        bucket_exists = any(b['Name'] == S3_BUCKET for b in response['Buckets'])
        if bucket_exists:
            print(f"\n✓ Bucket '{S3_BUCKET}' exists")
        else:
            print(f"\n✗ Bucket '{S3_BUCKET}' not found!")
            return False

        # Get bucket lifecycle configuration
        print("\nBucket lifecycle configuration:")
        try:
            lifecycle = s3_client.get_bucket_lifecycle_configuration(Bucket=S3_BUCKET)
            for rule in lifecycle['Rules']:
                print(f"  - Rule: {rule['ID']}")
                print(f"    Status: {rule['Status']}")
                if 'Transitions' in rule:
                    for transition in rule['Transitions']:
                        print(f"    Transition: {transition['Days']} days → {transition['StorageClass']}")
                if 'Expiration' in rule:
                    print(f"    Expiration: {rule['Expiration']['Days']} days")
        except Exception as e:
            print(f"  No lifecycle configuration: {e}")

        # Get bucket versioning
        print("\nBucket versioning:")
        versioning = s3_client.get_bucket_versioning(Bucket=S3_BUCKET)
        print(f"  Status: {versioning.get('Status', 'Not enabled')}")

        # Get bucket encryption
        print("\nBucket encryption:")
        try:
            encryption = s3_client.get_bucket_encryption(Bucket=S3_BUCKET)
            for rule in encryption['ServerSideEncryptionConfiguration']['Rules']:
                sse_algo = rule['ApplyServerSideEncryptionByDefault']['SSEAlgorithm']
                print(f"  Algorithm: {sse_algo}")
        except Exception as e:
            print(f"  No encryption configured: {e}")

        print("\n✓ S3 connection test passed")
        return True

    except Exception as e:
        print(f"\n✗ S3 connection test failed: {e}")
        import traceback
        traceback.print_exc()
        return False


def test_presigned_upload():
    """Test presigned URL generation for upload"""
    print("\n" + "=" * 60)
    print("Testing Presigned URL for Upload")
    print("=" * 60)

    try:
        s3_client = boto3.client(
            's3',
            endpoint_url=S3_ENDPOINT,
            region_name=S3_REGION,
            aws_access_key_id=AWS_ACCESS_KEY_ID,
            aws_secret_access_key=AWS_SECRET_ACCESS_KEY,
        )

        # Test file details
        file_name = "test-upload.txt"
        file_content = b"Hello from LocalStack S3!"
        content_type = "text/plain"
        content_length = len(file_content)

        # Calculate SHA-256 checksum
        checksum = hashlib.sha256(file_content).hexdigest()

        # Object key with tenant isolation
        tenant_id = "tenant-123"
        user_id = "user-456"
        object_key = f"uploads/{tenant_id}/{user_id}/{file_name}"

        print(f"\nGenerating presigned URL for upload...")
        print(f"  Object key: {object_key}")
        print(f"  Content-Type: {content_type}")
        print(f"  Content-Length: {content_length} bytes")
        print(f"  SHA-256: {checksum}")

        # Generate presigned URL with conditions
        presigned_url = s3_client.generate_presigned_url(
            'put_object',
            Params={
                'Bucket': S3_BUCKET,
                'Key': object_key,
                'ContentType': content_type,
                'ContentLength': content_length,
                'Metadata': {
                    'tenant-id': tenant_id,
                    'user-id': user_id,
                    'checksum-sha256': checksum,
                }
            },
            ExpiresIn=600,  # 10 minutes
        )

        print(f"\n✓ Presigned URL generated successfully")
        print(f"  URL: {presigned_url[:100]}...")
        print(f"  Expires in: 600 seconds (10 minutes)")

        # Test upload using presigned URL
        print("\nTesting upload with presigned URL...")
        import requests

        response = requests.put(
            presigned_url,
            data=file_content,
            headers={
                'Content-Type': content_type,
                'Content-Length': str(content_length),
            }
        )

        if response.status_code in [200, 204]:
            print(f"✓ Upload successful (status: {response.status_code})")

            # Verify object exists
            print("\nVerifying uploaded object...")
            obj_info = s3_client.head_object(Bucket=S3_BUCKET, Key=object_key)
            print(f"  Content-Type: {obj_info['ContentType']}")
            print(f"  Content-Length: {obj_info['ContentLength']}")
            print(f"  ETag: {obj_info['ETag']}")
            print(f"  Last-Modified: {obj_info['LastModified']}")

            # Test download
            print("\nTesting download...")
            download_response = s3_client.get_object(Bucket=S3_BUCKET, Key=object_key)
            downloaded_content = download_response['Body'].read()

            if downloaded_content == file_content:
                print("✓ Downloaded content matches uploaded content")
            else:
                print("✗ Downloaded content does not match!")
                return False

            # Cleanup
            print("\nCleaning up test object...")
            s3_client.delete_object(Bucket=S3_BUCKET, Key=object_key)
            print("✓ Test object deleted")

        else:
            print(f"✗ Upload failed (status: {response.status_code})")
            print(f"  Response: {response.text}")
            return False

        print("\n✓ Presigned upload test passed")
        return True

    except Exception as e:
        print(f"\n✗ Presigned upload test failed: {e}")
        import traceback
        traceback.print_exc()
        return False


def test_presigned_download():
    """Test presigned URL generation for download"""
    print("\n" + "=" * 60)
    print("Testing Presigned URL for Download")
    print("=" * 60)

    try:
        s3_client = boto3.client(
            's3',
            endpoint_url=S3_ENDPOINT,
            region_name=S3_REGION,
            aws_access_key_id=AWS_ACCESS_KEY_ID,
            aws_secret_access_key=AWS_SECRET_ACCESS_KEY,
        )

        # Upload a test file first
        object_key = "downloads/test-download.txt"
        file_content = b"Test download content"

        print(f"\nUploading test file...")
        s3_client.put_object(
            Bucket=S3_BUCKET,
            Key=object_key,
            Body=file_content,
            ContentType="text/plain"
        )
        print(f"✓ Test file uploaded: {object_key}")

        # Generate presigned URL for download
        print("\nGenerating presigned URL for download...")
        presigned_url = s3_client.generate_presigned_url(
            'get_object',
            Params={
                'Bucket': S3_BUCKET,
                'Key': object_key,
            },
            ExpiresIn=600,  # 10 minutes
        )

        print(f"✓ Presigned URL generated successfully")
        print(f"  URL: {presigned_url[:100]}...")

        # Test download using presigned URL
        print("\nTesting download with presigned URL...")
        import requests

        response = requests.get(presigned_url)

        if response.status_code == 200:
            print(f"✓ Download successful (status: {response.status_code})")

            if response.content == file_content:
                print("✓ Downloaded content matches expected content")
            else:
                print("✗ Downloaded content does not match!")
                return False
        else:
            print(f"✗ Download failed (status: {response.status_code})")
            return False

        # Cleanup
        print("\nCleaning up test object...")
        s3_client.delete_object(Bucket=S3_BUCKET, Key=object_key)
        print("✓ Test object deleted")

        print("\n✓ Presigned download test passed")
        return True

    except Exception as e:
        print(f"\n✗ Presigned download test failed: {e}")
        import traceback
        traceback.print_exc()
        return False


def test_multipart_upload():
    """Test multipart upload presigned URLs"""
    print("\n" + "=" * 60)
    print("Testing Multipart Upload")
    print("=" * 60)

    try:
        s3_client = boto3.client(
            's3',
            endpoint_url=S3_ENDPOINT,
            region_name=S3_REGION,
            aws_access_key_id=AWS_ACCESS_KEY_ID,
            aws_secret_access_key=AWS_SECRET_ACCESS_KEY,
        )

        object_key = "uploads/multipart-test.bin"

        # Initiate multipart upload
        print("\nInitiating multipart upload...")
        response = s3_client.create_multipart_upload(
            Bucket=S3_BUCKET,
            Key=object_key,
            ContentType="application/octet-stream"
        )
        upload_id = response['UploadId']
        print(f"✓ Multipart upload initiated")
        print(f"  Upload ID: {upload_id}")

        # Generate presigned URLs for parts
        part_size = 5 * 1024 * 1024  # 5 MB
        num_parts = 2
        parts = []

        print(f"\nGenerating presigned URLs for {num_parts} parts...")
        for part_number in range(1, num_parts + 1):
            presigned_url = s3_client.generate_presigned_url(
                'upload_part',
                Params={
                    'Bucket': S3_BUCKET,
                    'Key': object_key,
                    'UploadId': upload_id,
                    'PartNumber': part_number,
                },
                ExpiresIn=600,
            )
            print(f"  Part {part_number}: URL generated")

            # Upload part
            part_data = b"A" * part_size
            import requests
            response = requests.put(presigned_url, data=part_data)

            if response.status_code == 200:
                etag = response.headers['ETag'].strip('"')
                parts.append({'PartNumber': part_number, 'ETag': etag})
                print(f"    ✓ Uploaded (ETag: {etag})")
            else:
                print(f"    ✗ Upload failed (status: {response.status_code})")
                # Abort multipart upload
                s3_client.abort_multipart_upload(
                    Bucket=S3_BUCKET,
                    Key=object_key,
                    UploadId=upload_id
                )
                return False

        # Complete multipart upload
        print("\nCompleting multipart upload...")
        s3_client.complete_multipart_upload(
            Bucket=S3_BUCKET,
            Key=object_key,
            UploadId=upload_id,
            MultipartUpload={'Parts': parts}
        )
        print("✓ Multipart upload completed")

        # Verify object
        obj_info = s3_client.head_object(Bucket=S3_BUCKET, Key=object_key)
        expected_size = part_size * num_parts
        print(f"\nObject details:")
        print(f"  Content-Length: {obj_info['ContentLength']} bytes")
        print(f"  Expected: {expected_size} bytes")

        if obj_info['ContentLength'] == expected_size:
            print("✓ Object size matches expected size")
        else:
            print("✗ Object size mismatch!")
            return False

        # Cleanup
        print("\nCleaning up test object...")
        s3_client.delete_object(Bucket=S3_BUCKET, Key=object_key)
        print("✓ Test object deleted")

        print("\n✓ Multipart upload test passed")
        return True

    except Exception as e:
        print(f"\n✗ Multipart upload test failed: {e}")
        import traceback
        traceback.print_exc()
        return False


def main():
    """Run all tests"""
    print("\nLocalStack S3 Test Suite")
    print("=" * 60)

    tests = [
        ("S3 Connection", test_s3_connection),
        ("Presigned Upload", test_presigned_upload),
        ("Presigned Download", test_presigned_download),
        ("Multipart Upload", test_multipart_upload),
    ]

    results = []
    for test_name, test_func in tests:
        try:
            passed = test_func()
            results.append((test_name, passed))
        except Exception as e:
            print(f"\n✗ Test '{test_name}' crashed: {e}")
            results.append((test_name, False))

    # Summary
    print("\n" + "=" * 60)
    print("Test Summary")
    print("=" * 60)

    passed_count = sum(1 for _, passed in results if passed)
    total_count = len(results)

    for test_name, passed in results:
        status = "✓ PASSED" if passed else "✗ FAILED"
        print(f"{status}: {test_name}")

    print(f"\nTotal: {passed_count}/{total_count} tests passed")

    if passed_count == total_count:
        print("\n🎉 All tests passed!")
        return 0
    else:
        print(f"\n⚠️  {total_count - passed_count} test(s) failed")
        return 1


if __name__ == "__main__":
    sys.exit(main())
