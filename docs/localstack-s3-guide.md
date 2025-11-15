# LocalStack S3 Setup Guide

**@author** Heinstein F.
**@date** 2025-11-15

## Overview

LocalStack provides a local AWS S3 emulator for development and testing without incurring cloud costs.

## Configuration

### Docker Compose

LocalStack is configured in `docker-compose.yml`:

```yaml
localstack:
  image: localstack/localstack:3.0
  ports:
    - "4566:4566"  # All AWS services
  environment:
    - SERVICES=s3
    - AWS_DEFAULT_REGION=us-east-1
    - AWS_ACCESS_KEY_ID=test
    - AWS_SECRET_ACCESS_KEY=test
```

### Bucket Details

- **Bucket Name:** `saasforge-uploads-dev`
- **Region:** `us-east-1`
- **Endpoint:** `http://localhost:4566` (from host) or `http://localstack:4566` (from containers)
- **Access Keys:**
  - Access Key ID: `test`
  - Secret Access Key: `test`

### Features Enabled

✅ Versioning enabled
✅ Default AES-256 encryption
✅ CORS policy configured for `localhost:3000` and `localhost:8000`
✅ Public access blocked
✅ Folder structure: `uploads/`, `temp/`, `avatars/`, `documents/`

## Usage

### Starting LocalStack

```bash
docker compose up -d localstack
```

Wait for healthy status:
```bash
docker compose ps localstack
# Should show "healthy" status
```

### Testing S3 Connection

Run the test suite:
```bash
python3 scripts/test-s3-presign.py
```

This tests:
- ✓ S3 connectivity
- ✓ Presigned upload URLs
- ✓ Presigned download URLs
- ✓ Multipart uploads

### Python SDK Usage

```python
import boto3

s3_client = boto3.client(
    's3',
    endpoint_url='http://localhost:4566',  # Use http://localstack:4566 in containers
    region_name='us-east-1',
    aws_access_key_id='test',
    aws_secret_access_key='test',
)

# Generate presigned upload URL
presigned_url = s3_client.generate_presigned_url(
    'put_object',
    Params={
        'Bucket': 'saasforge-uploads-dev',
        'Key': 'uploads/tenant-id/user-id/file.jpg',
        'ContentType': 'image/jpeg',
    },
    ExpiresIn=600,  # 10 minutes
)

# Upload using presigned URL
import requests
with open('file.jpg', 'rb') as f:
    response = requests.put(presigned_url, data=f)
```

### AWS CLI (awslocal)

Install awslocal wrapper:
```bash
pip install awscli-local
```

List buckets:
```bash
awslocal s3 ls
```

Upload file:
```bash
awslocal s3 cp file.txt s3://saasforge-uploads-dev/test/file.txt
```

Download file:
```bash
awslocal s3 cp s3://saasforge-uploads-dev/test/file.txt downloaded.txt
```

## Presigned URL Security

### Upload URL Parameters

Required parameters for secure uploads:
- `ContentType` - Restricts file type
- `ContentLength` - Prevents oversized uploads
- `Metadata` - Add tenant/user tracking
- `ExpiresIn` - URL expiration (recommended: 600s = 10min)

```python
presigned_url = s3_client.generate_presigned_url(
    'put_object',
    Params={
        'Bucket': 'saasforge-uploads-dev',
        'Key': f'uploads/{tenant_id}/{user_id}/{filename}',
        'ContentType': content_type,
        'ContentLength': file_size,
        'Metadata': {
            'tenant-id': tenant_id,
            'user-id': user_id,
            'checksum-sha256': checksum,
        }
    },
    ExpiresIn=600,
)
```

### Multipart Upload Flow

For large files (> 5MB):

1. **Initiate multipart upload:**
```python
response = s3_client.create_multipart_upload(
    Bucket='saasforge-uploads-dev',
    Key=object_key,
    ContentType=content_type
)
upload_id = response['UploadId']
```

2. **Generate presigned URLs for parts:**
```python
presigned_urls = []
for part_number in range(1, num_parts + 1):
    url = s3_client.generate_presigned_url(
        'upload_part',
        Params={
            'Bucket': 'saasforge-uploads-dev',
            'Key': object_key,
            'UploadId': upload_id,
            'PartNumber': part_number,
        },
        ExpiresIn=3600,  # 1 hour for large uploads
    )
    presigned_urls.append(url)
```

3. **Upload parts (client-side):**
```python
parts = []
for part_number, url in enumerate(presigned_urls, 1):
    response = requests.put(url, data=part_data)
    etag = response.headers['ETag']
    parts.append({'PartNumber': part_number, 'ETag': etag.strip('"')})
```

4. **Complete multipart upload:**
```python
s3_client.complete_multipart_upload(
    Bucket='saasforge-uploads-dev',
    Key=object_key,
    UploadId=upload_id,
    MultipartUpload={'Parts': parts}
)
```

## Folder Structure Convention

Organize objects by tenant and user:

```
saasforge-uploads-dev/
├── uploads/{tenant_id}/{user_id}/{filename}
├── temp/{tenant_id}/{user_id}/{temp_file}
├── avatars/{tenant_id}/{user_id}/avatar.{ext}
└── documents/{tenant_id}/{document_id}/{filename}
```

This provides:
- **Tenant isolation** - Easy to query/delete all tenant data
- **User isolation** - Fine-grained access control
- **Scalability** - S3 performs better with logical prefixes

## Quota Enforcement

Check user storage usage:

```python
# List all objects for a user
paginator = s3_client.get_paginator('list_objects_v2')
pages = paginator.paginate(
    Bucket='saasforge-uploads-dev',
    Prefix=f'uploads/{tenant_id}/{user_id}/'
)

total_size = 0
for page in pages:
    for obj in page.get('Contents', []):
        total_size += obj['Size']

# Check against quota
if total_size > user_quota_bytes:
    raise QuotaExceededError(f"Storage quota exceeded: {total_size} / {user_quota_bytes}")
```

## Troubleshooting

### Container not healthy

```bash
docker compose logs localstack
# Look for initialization errors
```

### Bucket not created

```bash
docker compose restart localstack
# Initialization script runs on startup
```

### Connection refused

- From **host machine**: Use `http://localhost:4566`
- From **containers**: Use `http://localstack:4566`

### Reset LocalStack state

```bash
docker compose down -v localstack
docker compose up -d localstack
# Note: This deletes all data!
```

## Production Migration

When moving to AWS S3:

1. **Update environment variables:**
   ```bash
   S3_ENDPOINT=""  # Empty = use real AWS
   S3_BUCKET="saasforge-uploads-prod"
   AWS_ACCESS_KEY_ID="<real-key>"
   AWS_SECRET_ACCESS_KEY="<real-secret>"
   ```

2. **Enable lifecycle policies in AWS:**
   - Move to IA storage after 30 days
   - Archive to Glacier after 90 days
   - Delete temp files after 1 day

3. **Enable CloudFront CDN** for downloads

4. **Configure S3 bucket policies** for access control

5. **Enable CloudWatch metrics** for monitoring

## References

- LocalStack Docs: https://docs.localstack.cloud/
- AWS S3 SDK (boto3): https://boto3.amazonaws.com/v1/documentation/api/latest/reference/services/s3.html
- Presigned URLs: https://docs.aws.amazon.com/AmazonS3/latest/userguide/PresignedUrlUploadObject.html
- SaaSForge SRS: `docs/srs-boilerplate-saas.md` (Requirements B-40 to B-62)
