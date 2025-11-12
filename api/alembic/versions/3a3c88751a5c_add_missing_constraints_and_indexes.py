"""add_missing_constraints_and_indexes

Revision ID: 3a3c88751a5c
Revises: 5759b12b35b4
Create Date: 2025-11-12 21:42:14.520331

Phase 2 High Priority Fixes:
1. Add UNIQUE constraint to payment_methods.stripe_payment_method_id
2. Add foreign key to subscriptions.payment_method_id (optional reference)
3. Allow NULL for users.password_hash (OAuth-only users)
4. Add deleted_at to sessions, oauth_accounts, webhooks (soft delete)
5. Add updated_at triggers for upload_objects, payment_methods
6. Add performance indexes for common queries
7. Add connection pool timeout handling (documented in code)

"""
from typing import Sequence, Union

from alembic import op
import sqlalchemy as sa


# revision identifiers, used by Alembic.
revision: str = '3a3c88751a5c'
down_revision: Union[str, None] = '5759b12b35b4'
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None


def upgrade() -> None:
    """Apply Phase 2 high priority database fixes"""

    # 1. Add UNIQUE constraint to payment_methods.stripe_payment_method_id
    op.create_unique_constraint(
        'uq_payment_methods_stripe_id',
        'payment_methods',
        ['stripe_payment_method_id']
    )

    # 2. Add foreign key to subscriptions.payment_method_id
    # Note: This is an optional reference (can be NULL)
    op.create_foreign_key(
        'fk_subscriptions_payment_method',
        'subscriptions',
        'payment_methods',
        ['payment_method_id'],
        ['id'],
        ondelete='SET NULL'
    )

    # 3. Allow NULL for users.password_hash (OAuth-only users)
    op.alter_column(
        'users',
        'password_hash',
        nullable=True,
        existing_type=sa.Text()
    )

    # 4. Add deleted_at to tables missing soft delete
    # sessions already has revoked_at (equivalent), so skip
    # oauth_accounts - add deleted_at
    op.add_column(
        'oauth_accounts',
        sa.Column('deleted_at', sa.TIMESTAMP(timezone=True), nullable=True)
    )

    # webhooks - add deleted_at
    op.add_column(
        'webhooks',
        sa.Column('deleted_at', sa.TIMESTAMP(timezone=True), nullable=True)
    )

    # 5. Add updated_at triggers
    # First create the trigger function (idempotent)
    op.execute("""
        CREATE OR REPLACE FUNCTION update_updated_at_column()
        RETURNS TRIGGER AS $$
        BEGIN
            NEW.updated_at = NOW();
            RETURN NEW;
        END;
        $$ LANGUAGE plpgsql;
    """)

    # Add trigger for upload_objects
    op.execute("""
        DROP TRIGGER IF EXISTS trg_upload_objects_updated_at ON upload_objects;
        CREATE TRIGGER trg_upload_objects_updated_at
        BEFORE UPDATE ON upload_objects
        FOR EACH ROW
        EXECUTE FUNCTION update_updated_at_column();
    """)

    # Add trigger for payment_methods
    op.execute("""
        DROP TRIGGER IF EXISTS trg_payment_methods_updated_at ON payment_methods;
        CREATE TRIGGER trg_payment_methods_updated_at
        BEFORE UPDATE ON payment_methods
        FOR EACH ROW
        EXECUTE FUNCTION update_updated_at_column();
    """)

    # 6. Add performance indexes
    # Index for subscriptions.payment_method_id lookups
    op.create_index(
        'idx_subscriptions_payment_method',
        'subscriptions',
        ['payment_method_id'],
        postgresql_where=sa.text('payment_method_id IS NOT NULL')
    )

    # Index for upload_objects.checksum (duplicate detection)
    op.create_index(
        'idx_uploads_checksum',
        'upload_objects',
        ['checksum'],
        postgresql_where=sa.text('checksum IS NOT NULL AND deleted_at IS NULL')
    )

    # Index for api_keys.expires_at (cleanup queries)
    op.create_index(
        'idx_api_keys_expires',
        'api_keys',
        ['expires_at'],
        postgresql_where=sa.text('expires_at IS NOT NULL AND revoked_at IS NULL')
    )

    # Index for invoices.status + tenant_id (common dashboard query)
    op.create_index(
        'idx_invoices_tenant_status',
        'invoices',
        ['tenant_id', 'status']
    )

    # Index for notifications.created_at DESC (recent notifications query)
    op.create_index(
        'idx_notifications_created_desc',
        'notifications',
        [sa.text('created_at DESC')]
    )


def downgrade() -> None:
    """Rollback Phase 2 fixes"""

    # Remove indexes
    op.drop_index('idx_notifications_created_desc', table_name='notifications')
    op.drop_index('idx_invoices_tenant_status', table_name='invoices')
    op.drop_index('idx_api_keys_expires', table_name='api_keys')
    op.drop_index('idx_uploads_checksum', table_name='upload_objects')
    op.drop_index('idx_subscriptions_payment_method', table_name='subscriptions')

    # Remove triggers
    op.execute('DROP TRIGGER IF EXISTS trg_payment_methods_updated_at ON payment_methods')
    op.execute('DROP TRIGGER IF EXISTS trg_upload_objects_updated_at ON upload_objects')
    op.execute('DROP FUNCTION IF EXISTS update_updated_at_column()')

    # Remove deleted_at columns
    op.drop_column('webhooks', 'deleted_at')
    op.drop_column('oauth_accounts', 'deleted_at')

    # Revert users.password_hash to NOT NULL
    # WARNING: This will fail if there are OAuth-only users with NULL passwords
    op.alter_column(
        'users',
        'password_hash',
        nullable=False,
        existing_type=sa.Text()
    )

    # Remove foreign key
    op.drop_constraint('fk_subscriptions_payment_method', 'subscriptions', type_='foreignkey')

    # Remove unique constraint
    op.drop_constraint('uq_payment_methods_stripe_id', 'payment_methods', type_='unique')
