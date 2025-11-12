# Obsolete Manual Migrations

**Date:** 2025-11-12
**Status:** DEPRECATED - Do not use

## Why Deprecated?

These manual SQL migration files have been replaced by Alembic-managed migrations.

### Issues Found:

1. **Schema Inconsistencies:** These files do not match the canonical `db/schema.sql`:
   - `users.password_hash`: TEXT in schema.sql, VARCHAR(255) in migrations
   - `users`: has first_name/last_name in schema.sql, full_name in 001_initial_schema.sql
   - `quotas`: missing file_count fields in migrations
   - `subscriptions.payment_method_id`: Foreign key reference to table that doesn't exist yet

2. **No Version Control:** Manual migrations lack proper dependency tracking

3. **No Rollback Support:** Cannot safely downgrade schema changes

## Migration to Alembic

All schema changes are now managed through Alembic in `api/alembic/versions/`.

### Current Baseline Migration:

- **File:** `api/alembic/versions/5759b12b35b4_initial_schema_baseline.py`
- **Source:** Reads from canonical `db/schema.sql`
- **Status:** Single source of truth for schema

### To Apply Schema:

```bash
cd api
source venv/bin/activate
alembic upgrade head
```

### To Create New Migrations:

```bash
cd api
source venv/bin/activate
alembic revision -m "description_of_change"
# Edit the generated file in api/alembic/versions/
alembic upgrade head
```

## Files in This Directory:

- `001_initial_schema.sql` - Tenants, users, sessions, roles
- `002_upload_schema.sql` - Upload objects, quotas
- `003_payment_schema.sql` - Subscriptions, payment methods, invoices
- `004_notification_schema.sql` - Notifications, webhooks, email templates

**DO NOT USE THESE FILES.** They are preserved for reference only.
