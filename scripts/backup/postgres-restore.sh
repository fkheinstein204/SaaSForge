#!/bin/bash

################################################################################
# @file        postgres-restore.sh
# @brief       PostgreSQL database restoration script
# @author      Heinstein F.
# @date        2025-11-15
#
# @details
# SECURITY FIX: Database restoration from backups (Fix #14)
#
# Features:
# - Restore from local or S3 backups
# - Decrypt GPG-encrypted backups
# - Optional point-in-time recovery
# - Pre-restore validation
#
# Usage:
#   ./postgres-restore.sh [OPTIONS] BACKUP_FILE
#
# Options:
#   -h, --help       Show this help message
#   -d, --database   Target database name (default: saasforge)
#   -f, --force      Force restore (drop existing database)
#   -s, --s3         Download backup from S3
#
# Environment Variables:
#   POSTGRES_HOST        PostgreSQL host (default: localhost)
#   POSTGRES_PORT        PostgreSQL port (default: 5432)
#   POSTGRES_USER        PostgreSQL user (default: saasforge)
#   POSTGRES_PASSWORD    PostgreSQL password
#   S3_BUCKET            S3 bucket for backups
#   S3_PREFIX            S3 prefix (default: postgres-backups)
#
# Examples:
#   # Restore from local backup
#   ./postgres-restore.sh backups/postgres/saasforge_20251115_120000.sql.gz
#
#   # Restore from encrypted backup
#   ./postgres-restore.sh backups/postgres/saasforge_20251115_120000.sql.gz.gpg
#
#   # Download from S3 and restore
#   ./postgres-restore.sh --s3 saasforge_20251115_120000.sql.gz
################################################################################

set -euo pipefail

# Default configuration
POSTGRES_HOST="${POSTGRES_HOST:-localhost}"
POSTGRES_PORT="${POSTGRES_PORT:-5432}"
POSTGRES_USER="${POSTGRES_USER:-saasforge}"
POSTGRES_DB="${POSTGRES_DB:-saasforge}"
FORCE=false
FROM_S3=false
S3_BUCKET="${S3_BUCKET:-}"
S3_PREFIX="${S3_PREFIX:-postgres-backups}"
BACKUP_FILE=""

# Parse arguments
while [[ $# -gt 0 ]]; do
  case $1 in
    -h|--help)
      grep '^#' "$0" | grep -v '#!/bin/bash' | sed 's/^# \?//'
      exit 0
      ;;
    -d|--database)
      POSTGRES_DB="$2"
      shift 2
      ;;
    -f|--force)
      FORCE=true
      shift
      ;;
    -s|--s3)
      FROM_S3=true
      shift
      ;;
    *)
      BACKUP_FILE="$1"
      shift
      ;;
  esac
done

# Validate requirements
if [ -z "$BACKUP_FILE" ]; then
  echo "ERROR: No backup file specified"
  echo "Use --help for usage information"
  exit 1
fi

if [ -z "${POSTGRES_PASSWORD:-}" ]; then
  echo "ERROR: POSTGRES_PASSWORD environment variable not set"
  exit 1
fi

if [ "$FROM_S3" = true ] && [ -z "$S3_BUCKET" ]; then
  echo "ERROR: S3_BUCKET environment variable not set for S3 download"
  exit 1
fi

# Download from S3 if requested
if [ "$FROM_S3" = true ]; then
  echo "[$(date)] Downloading backup from S3..."
  LOCAL_FILE="/tmp/$(basename "$BACKUP_FILE")"
  S3_KEY="$S3_PREFIX/$BACKUP_FILE"

  if aws s3 cp "s3://$S3_BUCKET/$S3_KEY" "$LOCAL_FILE"; then
    echo "[$(date)] Downloaded: $LOCAL_FILE"
    BACKUP_FILE="$LOCAL_FILE"
  else
    echo "ERROR: Failed to download backup from S3"
    exit 1
  fi
fi

# Check if backup file exists
if [ ! -f "$BACKUP_FILE" ]; then
  echo "ERROR: Backup file not found: $BACKUP_FILE"
  exit 1
fi

# Decrypt if GPG-encrypted
if [[ "$BACKUP_FILE" == *.gpg ]]; then
  echo "[$(date)] Decrypting backup..."
  DECRYPTED_FILE="${BACKUP_FILE%.gpg}"

  if gpg --decrypt --output "$DECRYPTED_FILE" "$BACKUP_FILE"; then
    echo "[$(date)] Backup decrypted: $DECRYPTED_FILE"
    BACKUP_FILE="$DECRYPTED_FILE"
  else
    echo "ERROR: Decryption failed!"
    exit 1
  fi
fi

# Confirm restoration
if [ "$FORCE" = false ]; then
  echo ""
  echo "WARNING: This will restore the database '$POSTGRES_DB' from:"
  echo "  $BACKUP_FILE"
  echo ""
  echo "All current data in the database will be LOST!"
  echo ""
  read -p "Are you sure you want to continue? (yes/no): " CONFIRM

  if [ "$CONFIRM" != "yes" ]; then
    echo "Restoration cancelled."
    exit 0
  fi
fi

# Export password for pg_restore
export PGPASSWORD="$POSTGRES_PASSWORD"

# Drop existing database if forced
if [ "$FORCE" = true ]; then
  echo "[$(date)] Dropping existing database: $POSTGRES_DB"
  psql -h "$POSTGRES_HOST" -p "$POSTGRES_PORT" -U "$POSTGRES_USER" -d postgres \
    -c "DROP DATABASE IF EXISTS $POSTGRES_DB;" 2>/dev/null || true

  echo "[$(date)] Creating new database: $POSTGRES_DB"
  psql -h "$POSTGRES_HOST" -p "$POSTGRES_PORT" -U "$POSTGRES_USER" -d postgres \
    -c "CREATE DATABASE $POSTGRES_DB OWNER $POSTGRES_USER;"
fi

# Perform restoration
echo "[$(date)] Restoring database from backup..."

if [[ "$BACKUP_FILE" == *.gz ]]; then
  # Decompress and restore
  gunzip -c "$BACKUP_FILE" | pg_restore -h "$POSTGRES_HOST" -p "$POSTGRES_PORT" \
    -U "$POSTGRES_USER" -d "$POSTGRES_DB" --verbose --no-owner --no-acl
else
  # Restore directly
  pg_restore -h "$POSTGRES_HOST" -p "$POSTGRES_PORT" -U "$POSTGRES_USER" \
    -d "$POSTGRES_DB" --verbose --no-owner --no-acl "$BACKUP_FILE"
fi

if [ $? -eq 0 ]; then
  echo "[$(date)] Database restored successfully!"

  # Verify restoration
  TABLE_COUNT=$(psql -h "$POSTGRES_HOST" -p "$POSTGRES_PORT" -U "$POSTGRES_USER" \
    -d "$POSTGRES_DB" -t -c "SELECT COUNT(*) FROM information_schema.tables WHERE table_schema='public';")

  echo "[$(date)] Verification: $TABLE_COUNT tables restored"
  echo "[$(date)] Restoration completed successfully!"
else
  echo "[$(date)] ERROR: Restoration failed!"
  exit 1
fi

# Cleanup temporary files
if [ "$FROM_S3" = true ]; then
  rm -f "$BACKUP_FILE"
fi

exit 0
