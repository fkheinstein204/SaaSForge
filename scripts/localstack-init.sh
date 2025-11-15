#!/bin/bash
#
# @file        localstack-init.sh
# @brief       Initialize LocalStack S3 with buckets and lifecycle policies
# @author      Heinstein F.
# @date        2025-11-15
#

set -e

echo "Initializing LocalStack S3..."

# Wait for LocalStack to be fully ready
sleep 2

# Set AWS CLI to use LocalStack endpoint
export AWS_ACCESS_KEY_ID=test
export AWS_SECRET_ACCESS_KEY=test
export AWS_DEFAULT_REGION=us-east-1

ENDPOINT_URL="http://localhost:4566"
BUCKET_NAME="saasforge-uploads-dev"

# Create S3 bucket
echo "Creating S3 bucket: $BUCKET_NAME"
awslocal s3 mb s3://$BUCKET_NAME --region us-east-1 || true

# Enable versioning
echo "Enabling versioning on bucket: $BUCKET_NAME"
awslocal s3api put-bucket-versioning \
    --bucket $BUCKET_NAME \
    --versioning-configuration Status=Enabled

# Configure lifecycle policy (skip for MVP - LocalStack has issues with this)
echo "Skipping lifecycle policy configuration (not critical for MVP)..."
# Note: In production AWS S3, you would configure lifecycle policies here

# Configure CORS
echo "Configuring CORS policy..."
cat > /tmp/cors-policy.json <<EOF
{
  "CORSRules": [
    {
      "AllowedOrigins": ["http://localhost:3000", "http://localhost:8000"],
      "AllowedMethods": ["GET", "PUT", "POST", "DELETE", "HEAD"],
      "AllowedHeaders": ["*"],
      "ExposeHeaders": ["ETag", "x-amz-request-id"],
      "MaxAgeSeconds": 3600
    }
  ]
}
EOF

awslocal s3api put-bucket-cors \
    --bucket $BUCKET_NAME \
    --cors-configuration file:///tmp/cors-policy.json

# Set bucket encryption
echo "Enabling default encryption..."
awslocal s3api put-bucket-encryption \
    --bucket $BUCKET_NAME \
    --server-side-encryption-configuration '{
      "Rules": [{
        "ApplyServerSideEncryptionByDefault": {
          "SSEAlgorithm": "AES256"
        }
      }]
    }'

# Set bucket public access block
echo "Blocking public access..."
awslocal s3api put-public-access-block \
    --bucket $BUCKET_NAME \
    --public-access-block-configuration \
      "BlockPublicAcls=true,IgnorePublicAcls=true,BlockPublicPolicy=true,RestrictPublicBuckets=true"

# Create folder structure
echo "Creating folder structure..."
awslocal s3api put-object --bucket $BUCKET_NAME --key uploads/ --content-length 0
awslocal s3api put-object --bucket $BUCKET_NAME --key temp/ --content-length 0
awslocal s3api put-object --bucket $BUCKET_NAME --key avatars/ --content-length 0
awslocal s3api put-object --bucket $BUCKET_NAME --key documents/ --content-length 0

# Verify bucket creation
echo "Verifying bucket configuration..."
awslocal s3 ls
awslocal s3api get-bucket-versioning --bucket $BUCKET_NAME

echo "LocalStack S3 initialization complete!"
echo "Bucket: $BUCKET_NAME"
echo "Endpoint: $ENDPOINT_URL"
echo "Region: $AWS_DEFAULT_REGION"
