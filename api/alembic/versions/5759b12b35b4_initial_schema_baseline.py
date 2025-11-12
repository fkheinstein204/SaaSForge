"""initial_schema_baseline

Revision ID: 5759b12b35b4
Revises:
Create Date: 2025-11-12 21:39:20.102930

Creates complete SaaSForge schema from db/schema.sql:
- 14 core tables (tenants, users, sessions, roles, api_keys, oauth_accounts,
  upload_objects, quotas, subscriptions, payment_methods, invoices, usage_records,
  notifications, notification_preferences, webhooks, email_templates)
- 1 audit table (audit_log)
- All indexes, constraints, and foreign keys
- UUID and pgcrypto extensions
"""
from typing import Sequence, Union
import os

from alembic import op
import sqlalchemy as sa


# revision identifiers, used by Alembic.
revision: str = '5759b12b35b4'
down_revision: Union[str, None] = None
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None


def upgrade() -> None:
    """Apply complete SaaSForge schema from db/schema.sql"""

    # Read schema.sql relative to project root
    schema_path = os.path.join(
        os.path.dirname(__file__), '..', '..', '..', 'db', 'schema.sql'
    )
    schema_path = os.path.abspath(schema_path)

    with open(schema_path, 'r') as f:
        schema_sql = f.read()

    # Execute the full schema
    op.execute(schema_sql)


def downgrade() -> None:
    """Drop all tables in reverse dependency order"""

    # Drop tables in reverse order to respect foreign keys
    tables = [
        'audit_log',
        'email_templates',
        'webhooks',
        'notification_preferences',
        'notifications',
        'usage_records',
        'invoices',
        'payment_methods',
        'subscriptions',
        'quotas',
        'upload_objects',
        'oauth_accounts',
        'api_keys',
        'user_roles',
        'roles',
        'sessions',
        'users',
        'tenants',
    ]

    for table in tables:
        op.execute(f'DROP TABLE IF EXISTS {table} CASCADE')

    # Drop extensions
    op.execute('DROP EXTENSION IF EXISTS pgcrypto')
    op.execute('DROP EXTENSION IF EXISTS "uuid-ossp"')
