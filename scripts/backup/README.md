# PostgreSQL Backup Strategy

**SECURITY FIX #14:** Automated database backup and recovery system

## Overview

This backup strategy provides:
- **Automated daily backups** with configurable retention
- **Encrypted backups** using GPG
- **S3-compatible storage** for offsite backups
- **Point-in-time recovery** capability
- **Retention policy:** 7 daily, 4 weekly, 6 monthly backups

## Quick Start

### 1. Manual Backup

```bash
# Basic backup (local only)
./scripts/backup/postgres-backup.sh

# Encrypted backup
export GPG_RECIPIENT="backup@saasforge.com"
./scripts/backup/postgres-backup.sh --encrypt

# Backup with S3 upload
export S3_BUCKET="saasforge-backups"
./scripts/backup/postgres-backup.sh --encrypt --s3
```

### 2. Manual Restore

```bash
# Restore from local backup
./scripts/backup/postgres-restore.sh backups/postgres/saasforge_20251115_120000.sql.gz

# Restore from S3
export S3_BUCKET="saasforge-backups"
./scripts/backup/postgres-restore.sh --s3 saasforge_20251115_120000.sql.gz

# Force restore (drops existing database)
./scripts/backup/postgres-restore.sh --force backups/postgres/saasforge_20251115_120000.sql.gz
```

## Automated Backups (Cron)

### Setup Daily Backups at 2 AM

1. Create environment file for cron:

```bash
cat > /etc/cron.d/postgres-backup.env << EOF
POSTGRES_HOST=localhost
POSTGRES_PORT=5432
POSTGRES_USER=saasforge
POSTGRES_PASSWORD=your_secure_password
BACKUP_DIR=/var/backups/postgres
GPG_RECIPIENT=backup@saasforge.com
S3_BUCKET=saasforge-backups
S3_PREFIX=postgres-backups
AWS_ACCESS_KEY_ID=your_aws_key
AWS_SECRET_ACCESS_KEY=your_aws_secret
EOF

chmod 600 /etc/cron.d/postgres-backup.env
```

2. Add cron job:

```bash
# Edit crontab
crontab -e

# Add this line (daily at 2 AM):
0 2 * * * source /etc/cron.d/postgres-backup.env && /path/to/scripts/backup/postgres-backup.sh --encrypt --s3 >> /var/log/postgres-backup.log 2>&1
```

### Docker Compose Setup

Add a backup service to `docker-compose.yml`:

```yaml
  postgres-backup:
    image: postgres:15
    depends_on:
      postgres:
        condition: service_healthy
    environment:
      POSTGRES_HOST: postgres
      POSTGRES_PORT: 5432
      POSTGRES_USER: saasforge
      POSTGRES_PASSWORD: dev_password
      BACKUP_DIR: /backups
    volumes:
      - ./scripts/backup:/scripts
      - ./backups:/backups
    command: >
      bash -c "
        while true; do
          echo 'Running backup at $(date)'
          /scripts/postgres-backup.sh
          echo 'Next backup in 24 hours'
          sleep 86400
        done
      "
```

## Backup Retention Policy

| Backup Type | Retention Period | Storage Class |
|-------------|------------------|---------------|
| Daily       | 7 days           | STANDARD      |
| Weekly      | 4 weeks          | STANDARD_IA   |
| Monthly     | 6 months         | GLACIER       |

**Total Storage:** Approximately 30-50 backups depending on database size.

## Encryption (GPG)

### Generate GPG Key for Backups

```bash
# Generate key
gpg --full-generate-key

# Export public key
gpg --export --armor backup@saasforge.com > backup-public.key

# Import on restore server
gpg --import backup-public.key
```

### Encrypt/Decrypt Manually

```bash
# Encrypt
gpg --encrypt --recipient backup@saasforge.com backup.sql.gz

# Decrypt
gpg --decrypt backup.sql.gz.gpg > backup.sql.gz
```

## S3 Configuration

### AWS S3

```bash
# Install AWS CLI
apt-get install awscli

# Configure credentials
aws configure

# Test upload
aws s3 cp test.txt s3://saasforge-backups/postgres-backups/
```

### S3-Compatible (MinIO, Backblaze B2, etc.)

```bash
# Configure custom endpoint
aws configure set s3.endpoint_url https://s3.backblaze.com

# Or use environment variable
export AWS_ENDPOINT_URL=https://s3.backblaze.com
```

## Monitoring & Alerts

### Check Backup Status

```bash
# List recent backups
ls -lh backups/postgres/ | tail -10

# Check backup logs
tail -f backups/postgres/backup_*.log

# Verify S3 backups
aws s3 ls s3://saasforge-backups/postgres-backups/
```

### Alert on Backup Failure

Add to cron job:

```bash
0 2 * * * /path/to/postgres-backup.sh --encrypt --s3 || echo "PostgreSQL backup failed at $(date)" | mail -s "ALERT: Backup Failed" admin@saasforge.com
```

## Disaster Recovery Procedures

### 1. Complete Database Loss

```bash
# 1. Stop application
docker-compose down

# 2. Download latest backup from S3
export S3_BUCKET="saasforge-backups"
aws s3 cp s3://$S3_BUCKET/postgres-backups/saasforge_latest.sql.gz.gpg /tmp/

# 3. Restore database
./scripts/backup/postgres-restore.sh --force /tmp/saasforge_latest.sql.gz.gpg

# 4. Verify restoration
docker-compose exec postgres psql -U saasforge -d saasforge -c "\dt"

# 5. Restart application
docker-compose up -d
```

### 2. Point-in-Time Recovery

```bash
# Restore to specific timestamp
./scripts/backup/postgres-restore.sh backups/postgres/saasforge_20251115_140000.sql.gz

# Verify data at that point in time
docker-compose exec postgres psql -U saasforge -d saasforge
```

### 3. Data Corruption

```bash
# 1. Identify corruption timeframe
# 2. Find backup before corruption
ls -lh backups/postgres/

# 3. Restore from clean backup
./scripts/backup/postgres-restore.sh --force backups/postgres/saasforge_YYYYMMDD_HHMMSS.sql.gz
```

## Testing Backups

**CRITICAL:** Always test your backups!

```bash
# 1. Create test database
docker-compose exec postgres psql -U saasforge -d postgres -c "CREATE DATABASE saasforge_test;"

# 2. Restore to test database
export POSTGRES_DB=saasforge_test
./scripts/backup/postgres-restore.sh backups/postgres/latest.sql.gz

# 3. Verify data integrity
docker-compose exec postgres psql -U saasforge -d saasforge_test -c "SELECT COUNT(*) FROM users;"

# 4. Cleanup
docker-compose exec postgres psql -U saasforge -d postgres -c "DROP DATABASE saasforge_test;"
```

## Backup Size Estimation

| Database Size | Compressed Backup | Encrypted Backup | S3 Storage (30 days) |
|---------------|-------------------|------------------|----------------------|
| 100 MB        | ~20 MB            | ~20 MB           | ~600 MB              |
| 1 GB          | ~200 MB           | ~200 MB          | ~6 GB                |
| 10 GB         | ~2 GB             | ~2 GB            | ~60 GB               |

**Cost Estimate (AWS S3):**
- STANDARD: $0.023/GB/month
- STANDARD_IA: $0.0125/GB/month
- GLACIER: $0.004/GB/month

## Security Considerations

1. **Encryption:** Always encrypt backups containing PII or financial data
2. **Access Control:** Restrict S3 bucket access with IAM policies
3. **Key Management:** Store GPG private keys in secure vault (AWS KMS, HashiCorp Vault)
4. **Audit Logs:** Enable S3 access logging and CloudTrail
5. **Retention:** Follow GDPR/compliance requirements (7 years for financial data)

## Troubleshooting

### Backup Failed: "disk full"

```bash
# Check disk usage
df -h

# Clean old backups manually
find backups/postgres -name "*.sql.gz*" -mtime +30 -delete
```

### Restore Failed: "database exists"

```bash
# Use --force flag to drop and recreate
./scripts/backup/postgres-restore.sh --force backup.sql.gz
```

### S3 Upload Failed: "access denied"

```bash
# Check AWS credentials
aws sts get-caller-identity

# Verify bucket permissions
aws s3api get-bucket-acl --bucket saasforge-backups
```

## Integration with Monitoring (Prometheus)

Export backup metrics:

```bash
# Add to backup script
echo "postgres_backup_size_bytes $(stat -f%z "$BACKUP_FILE")" > /var/lib/node_exporter/postgres_backup.prom
echo "postgres_backup_timestamp_seconds $(date +%s)" >> /var/lib/node_exporter/postgres_backup.prom
```

Alert rule:

```yaml
- alert: PostgresBackupStale
  expr: time() - postgres_backup_timestamp_seconds > 86400 * 2
  annotations:
    summary: "PostgreSQL backup is older than 2 days"
```

## References

- [PostgreSQL Backup & Recovery](https://www.postgresql.org/docs/current/backup.html)
- [pg_dump Documentation](https://www.postgresql.org/docs/current/app-pgdump.html)
- [GPG Encryption Guide](https://gnupg.org/documentation/)
- [AWS S3 Lifecycle Policies](https://docs.aws.amazon.com/AmazonS3/latest/userguide/object-lifecycle-mgmt.html)
