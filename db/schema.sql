-- SaaSForge Database Schema
-- PostgreSQL 15+
-- Multi-tenant SaaS boilerplate with strict tenant isolation
--
-- Core Entities:
-- Identity & Access: tenants, users, sessions, roles, api_keys
-- Content Management: upload_objects, quotas
-- Billing: subscriptions, payment_methods, invoices, usage_records
-- Notifications: notifications, notification_preferences, webhooks, email_templates

-- Enable UUID extension
CREATE EXTENSION IF NOT EXISTS "uuid-ossp";

-- Enable pgcrypto for encryption functions
CREATE EXTENSION IF NOT EXISTS "pgcrypto";

-- ============================================================================
-- IDENTITY & ACCESS MANAGEMENT
-- ============================================================================

-- Tenants: Top-level isolation boundary
CREATE TABLE tenants (
    id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    name VARCHAR(255) NOT NULL,
    slug VARCHAR(100) UNIQUE NOT NULL,
    plan_id VARCHAR(50) NOT NULL DEFAULT 'free',
    status VARCHAR(20) NOT NULL DEFAULT 'active',
    settings JSONB DEFAULT '{}',
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    deleted_at TIMESTAMP WITH TIME ZONE,

    CONSTRAINT tenant_status_check CHECK (status IN ('active', 'suspended', 'cancelled')),
    CONSTRAINT tenant_slug_format CHECK (slug ~ '^[a-z0-9-]+$')
);

CREATE INDEX idx_tenants_slug ON tenants(slug) WHERE deleted_at IS NULL;
CREATE INDEX idx_tenants_status ON tenants(status) WHERE deleted_at IS NULL;

-- Users: Authentication principals
CREATE TABLE users (
    id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    tenant_id UUID NOT NULL REFERENCES tenants(id) ON DELETE CASCADE,
    email VARCHAR(255) NOT NULL,
    email_verified BOOLEAN NOT NULL DEFAULT FALSE,
    password_hash TEXT NOT NULL,  -- Argon2id hash in PHC format
    first_name VARCHAR(100),
    last_name VARCHAR(100),
    totp_secret VARCHAR(32),  -- Base32 encoded TOTP secret
    totp_enabled BOOLEAN NOT NULL DEFAULT FALSE,
    last_login_at TIMESTAMP WITH TIME ZONE,
    last_login_ip INET,
    failed_login_attempts INT NOT NULL DEFAULT 0,
    locked_until TIMESTAMP WITH TIME ZONE,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    deleted_at TIMESTAMP WITH TIME ZONE,

    CONSTRAINT users_tenant_isolation CHECK (tenant_id IS NOT NULL),
    CONSTRAINT users_email_format CHECK (email ~* '^[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}$')
);

CREATE UNIQUE INDEX idx_users_email_tenant ON users(email, tenant_id) WHERE deleted_at IS NULL;
CREATE INDEX idx_users_tenant ON users(tenant_id) WHERE deleted_at IS NULL;
CREATE INDEX idx_users_last_login ON users(last_login_at DESC);

COMMENT ON COLUMN users.password_hash IS 'Argon2id hash in PHC format: $argon2id$v=19$m=65536,t=3,p=4$salt$hash';
COMMENT ON COLUMN users.failed_login_attempts IS 'Reset to 0 on successful login. Lock account after 5 attempts.';

-- Sessions: Active user sessions with refresh tokens
CREATE TABLE sessions (
    id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    user_id UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    tenant_id UUID NOT NULL REFERENCES tenants(id) ON DELETE CASCADE,
    refresh_token_hash TEXT NOT NULL,  -- SHA-256 hash of refresh token
    device_fingerprint VARCHAR(255),
    user_agent TEXT,
    ip_address INET,
    expires_at TIMESTAMP WITH TIME ZONE NOT NULL,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    last_used_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    revoked_at TIMESTAMP WITH TIME ZONE,

    CONSTRAINT session_tenant_isolation CHECK (tenant_id IS NOT NULL)
);

CREATE INDEX idx_sessions_user ON sessions(user_id) WHERE revoked_at IS NULL;
CREATE INDEX idx_sessions_tenant ON sessions(tenant_id) WHERE revoked_at IS NULL;
CREATE INDEX idx_sessions_expires ON sessions(expires_at) WHERE revoked_at IS NULL;
CREATE INDEX idx_sessions_refresh_token ON sessions(refresh_token_hash) WHERE revoked_at IS NULL;

-- Roles: RBAC role definitions
CREATE TABLE roles (
    id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    tenant_id UUID REFERENCES tenants(id) ON DELETE CASCADE,  -- NULL for system roles
    name VARCHAR(50) NOT NULL,
    description TEXT,
    permissions JSONB NOT NULL DEFAULT '[]',
    is_system BOOLEAN NOT NULL DEFAULT FALSE,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

CREATE UNIQUE INDEX idx_roles_tenant_name ON roles(tenant_id, name);
CREATE INDEX idx_roles_system ON roles(is_system);

COMMENT ON COLUMN roles.is_system IS 'System roles (admin, user, guest) cannot be modified or deleted';
COMMENT ON COLUMN roles.permissions IS 'Array of permission strings: ["read:users", "write:uploads", "admin:*"]';

-- User Roles: Many-to-many relationship
CREATE TABLE user_roles (
    user_id UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    role_id UUID NOT NULL REFERENCES roles(id) ON DELETE CASCADE,
    assigned_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    assigned_by UUID REFERENCES users(id),

    PRIMARY KEY (user_id, role_id)
);

CREATE INDEX idx_user_roles_user ON user_roles(user_id);
CREATE INDEX idx_user_roles_role ON user_roles(role_id);

-- API Keys: Long-lived programmatic access tokens
CREATE TABLE api_keys (
    id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    user_id UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    tenant_id UUID NOT NULL REFERENCES tenants(id) ON DELETE CASCADE,
    name VARCHAR(100) NOT NULL,
    key_hash TEXT NOT NULL,  -- SHA-256 hash of API key
    key_prefix VARCHAR(10) NOT NULL,  -- First 8 chars for identification (sk_1234567)
    scopes TEXT NOT NULL,  -- Comma-separated: "read:uploads,write:notifications"
    last_used_at TIMESTAMP WITH TIME ZONE,
    last_used_ip INET,
    expires_at TIMESTAMP WITH TIME ZONE,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    revoked_at TIMESTAMP WITH TIME ZONE,

    CONSTRAINT api_key_tenant_isolation CHECK (tenant_id IS NOT NULL)
);

CREATE INDEX idx_api_keys_user ON api_keys(user_id) WHERE revoked_at IS NULL;
CREATE INDEX idx_api_keys_tenant ON api_keys(tenant_id) WHERE revoked_at IS NULL;
CREATE INDEX idx_api_keys_key_hash ON api_keys(key_hash) WHERE revoked_at IS NULL;
CREATE INDEX idx_api_keys_prefix ON api_keys(key_prefix);

COMMENT ON COLUMN api_keys.key_prefix IS 'Format: sk_XXXXXXXX for quick identification in logs';

-- OAuth Accounts: External identity provider linkages
CREATE TABLE oauth_accounts (
    id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    user_id UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    tenant_id UUID NOT NULL REFERENCES tenants(id) ON DELETE CASCADE,
    provider VARCHAR(50) NOT NULL,  -- google, github, microsoft
    provider_user_id VARCHAR(255) NOT NULL,
    provider_email VARCHAR(255),
    access_token TEXT,  -- Encrypted at rest
    refresh_token TEXT,  -- Encrypted at rest
    token_expires_at TIMESTAMP WITH TIME ZONE,
    scopes TEXT,
    profile_data JSONB,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),

    CONSTRAINT oauth_tenant_isolation CHECK (tenant_id IS NOT NULL),
    CONSTRAINT oauth_provider_check CHECK (provider IN ('google', 'github', 'microsoft'))
);

CREATE UNIQUE INDEX idx_oauth_provider_user ON oauth_accounts(provider, provider_user_id);
CREATE INDEX idx_oauth_user ON oauth_accounts(user_id);
CREATE INDEX idx_oauth_tenant ON oauth_accounts(tenant_id);

COMMENT ON COLUMN oauth_accounts.access_token IS 'Encrypted with pgcrypto using application key';

-- ============================================================================
-- CONTENT MANAGEMENT
-- ============================================================================

-- Upload Objects: File storage metadata
CREATE TABLE upload_objects (
    id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    tenant_id UUID NOT NULL REFERENCES tenants(id) ON DELETE CASCADE,
    user_id UUID NOT NULL REFERENCES users(id) ON DELETE SET NULL,
    object_key VARCHAR(1024) NOT NULL,  -- S3 object key: tenant_id/user_id/timestamp_filename
    filename VARCHAR(255) NOT NULL,
    content_type VARCHAR(100) NOT NULL,
    size BIGINT NOT NULL,  -- Bytes
    checksum VARCHAR(64),  -- SHA-256 hex
    status VARCHAR(20) NOT NULL DEFAULT 'pending',
    virus_scan_status VARCHAR(20),
    virus_scan_result TEXT,
    transformations JSONB DEFAULT '[]',
    metadata JSONB DEFAULT '{}',
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    completed_at TIMESTAMP WITH TIME ZONE,
    deleted_at TIMESTAMP WITH TIME ZONE,

    CONSTRAINT upload_tenant_isolation CHECK (tenant_id IS NOT NULL),
    CONSTRAINT upload_status_check CHECK (status IN ('pending', 'processing', 'completed', 'failed')),
    CONSTRAINT upload_virus_check CHECK (virus_scan_status IN ('pending', 'clean', 'infected', 'error')),
    CONSTRAINT upload_size_positive CHECK (size > 0)
);

CREATE INDEX idx_uploads_tenant ON upload_objects(tenant_id) WHERE deleted_at IS NULL;
CREATE INDEX idx_uploads_user ON upload_objects(user_id) WHERE deleted_at IS NULL;
CREATE INDEX idx_uploads_status ON upload_objects(status);
CREATE INDEX idx_uploads_created ON upload_objects(created_at DESC);
CREATE INDEX idx_uploads_object_key ON upload_objects(object_key);

COMMENT ON COLUMN upload_objects.transformations IS 'Array of transformation results: [{"profile_id": "thumbnail", "url": "...", "size": 12345}]';

-- Quotas: Tenant storage quotas
CREATE TABLE quotas (
    tenant_id UUID PRIMARY KEY REFERENCES tenants(id) ON DELETE CASCADE,
    used_bytes BIGINT NOT NULL DEFAULT 0,
    limit_bytes BIGINT NOT NULL DEFAULT 10737418240,  -- 10 GB default
    file_count INT NOT NULL DEFAULT 0,
    file_count_limit INT NOT NULL DEFAULT 10000,
    updated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),

    CONSTRAINT quota_used_positive CHECK (used_bytes >= 0),
    CONSTRAINT quota_limit_positive CHECK (limit_bytes > 0),
    CONSTRAINT quota_file_count_positive CHECK (file_count >= 0)
);

CREATE INDEX idx_quotas_usage ON quotas(used_bytes, limit_bytes);

COMMENT ON TABLE quotas IS 'Tenant storage quotas with 10% grace overage allowed (enforced in application)';

-- ============================================================================
-- BILLING & PAYMENTS
-- ============================================================================

-- Subscriptions: Tenant subscription plans
CREATE TABLE subscriptions (
    id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    tenant_id UUID NOT NULL REFERENCES tenants(id) ON DELETE CASCADE,
    stripe_subscription_id VARCHAR(100) UNIQUE,
    plan_id VARCHAR(50) NOT NULL,
    status INT NOT NULL,  -- Enum: 0=unspecified, 1=active, 2=past_due, 3=canceled, 4=unpaid, 5=trialing
    quantity INT NOT NULL DEFAULT 1,
    current_period_start TIMESTAMP WITH TIME ZONE NOT NULL,
    current_period_end TIMESTAMP WITH TIME ZONE NOT NULL,
    cancel_at TIMESTAMP WITH TIME ZONE,
    canceled_at TIMESTAMP WITH TIME ZONE,
    trial_start TIMESTAMP WITH TIME ZONE,
    trial_end TIMESTAMP WITH TIME ZONE,
    mrr DECIMAL(10, 2) NOT NULL DEFAULT 0.00,  -- Monthly Recurring Revenue
    payment_method_id UUID REFERENCES payment_methods(id),
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),

    CONSTRAINT subscription_tenant_isolation CHECK (tenant_id IS NOT NULL),
    CONSTRAINT subscription_quantity_positive CHECK (quantity > 0),
    CONSTRAINT subscription_mrr_positive CHECK (mrr >= 0)
);

CREATE UNIQUE INDEX idx_subscriptions_tenant ON subscriptions(tenant_id);
CREATE INDEX idx_subscriptions_status ON subscriptions(status);
CREATE INDEX idx_subscriptions_stripe ON subscriptions(stripe_subscription_id);
CREATE INDEX idx_subscriptions_period ON subscriptions(current_period_end);

COMMENT ON COLUMN subscriptions.status IS '1=active, 2=past_due, 3=canceled, 4=unpaid, 5=trialing';
COMMENT ON COLUMN subscriptions.mrr IS 'Calculated as: plan_base_price * quantity (excluding usage-based charges)';

-- Payment Methods: Stored payment methods (tokenized)
CREATE TABLE payment_methods (
    id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    tenant_id UUID NOT NULL REFERENCES tenants(id) ON DELETE CASCADE,
    stripe_payment_method_id VARCHAR(100) NOT NULL,
    type VARCHAR(20) NOT NULL,  -- card, bank_account, etc.
    last4 VARCHAR(4),
    brand VARCHAR(50),  -- visa, mastercard, etc.
    exp_month INT,
    exp_year INT,
    is_default BOOLEAN NOT NULL DEFAULT FALSE,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    deleted_at TIMESTAMP WITH TIME ZONE,

    CONSTRAINT payment_tenant_isolation CHECK (tenant_id IS NOT NULL),
    CONSTRAINT payment_type_check CHECK (type IN ('card', 'bank_account', 'sepa_debit'))
);

CREATE INDEX idx_payment_methods_tenant ON payment_methods(tenant_id) WHERE deleted_at IS NULL;
CREATE INDEX idx_payment_methods_stripe ON payment_methods(stripe_payment_method_id);
CREATE INDEX idx_payment_methods_default ON payment_methods(tenant_id, is_default) WHERE is_default = TRUE AND deleted_at IS NULL;

COMMENT ON COLUMN payment_methods.last4 IS 'Last 4 digits only for PCI compliance';

-- Invoices: Billing invoices
CREATE TABLE invoices (
    id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    tenant_id UUID NOT NULL REFERENCES tenants(id) ON DELETE CASCADE,
    subscription_id UUID REFERENCES subscriptions(id) ON DELETE SET NULL,
    stripe_invoice_id VARCHAR(100) UNIQUE,
    invoice_number VARCHAR(50),
    status VARCHAR(20) NOT NULL DEFAULT 'draft',
    amount_due DECIMAL(10, 2) NOT NULL DEFAULT 0.00,
    amount_paid DECIMAL(10, 2) NOT NULL DEFAULT 0.00,
    amount_remaining DECIMAL(10, 2) NOT NULL DEFAULT 0.00,
    tax DECIMAL(10, 2) NOT NULL DEFAULT 0.00,
    currency VARCHAR(3) NOT NULL DEFAULT 'USD',
    line_items JSONB NOT NULL DEFAULT '[]',
    due_date TIMESTAMP WITH TIME ZONE NOT NULL,
    paid_at TIMESTAMP WITH TIME ZONE,
    pdf_url TEXT,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),

    CONSTRAINT invoice_tenant_isolation CHECK (tenant_id IS NOT NULL),
    CONSTRAINT invoice_status_check CHECK (status IN ('draft', 'open', 'paid', 'void', 'uncollectible')),
    CONSTRAINT invoice_amounts_positive CHECK (amount_due >= 0 AND amount_paid >= 0)
);

CREATE INDEX idx_invoices_tenant ON invoices(tenant_id);
CREATE INDEX idx_invoices_subscription ON invoices(subscription_id);
CREATE INDEX idx_invoices_status ON invoices(status);
CREATE INDEX idx_invoices_due_date ON invoices(due_date);
CREATE INDEX idx_invoices_stripe ON invoices(stripe_invoice_id);

COMMENT ON COLUMN invoices.line_items IS 'Array of line items: [{"description": "Pro Plan", "amount": 99.00, "quantity": 1}]';

-- Usage Records: Usage-based billing metrics
CREATE TABLE usage_records (
    id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    tenant_id UUID NOT NULL REFERENCES tenants(id) ON DELETE CASCADE,
    subscription_id UUID NOT NULL REFERENCES subscriptions(id) ON DELETE CASCADE,
    metric_name VARCHAR(100) NOT NULL,  -- api_calls, storage_gb, bandwidth_gb
    quantity BIGINT NOT NULL,
    timestamp TIMESTAMP WITH TIME ZONE NOT NULL,
    aggregated BOOLEAN NOT NULL DEFAULT FALSE,
    invoice_id UUID REFERENCES invoices(id),
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),

    CONSTRAINT usage_tenant_isolation CHECK (tenant_id IS NOT NULL),
    CONSTRAINT usage_quantity_positive CHECK (quantity > 0)
);

CREATE INDEX idx_usage_tenant ON usage_records(tenant_id);
CREATE INDEX idx_usage_subscription ON usage_records(subscription_id);
CREATE INDEX idx_usage_metric ON usage_records(metric_name, timestamp DESC);
CREATE INDEX idx_usage_aggregated ON usage_records(aggregated) WHERE aggregated = FALSE;

COMMENT ON TABLE usage_records IS 'Metered billing: api_calls, storage_gb, bandwidth_gb, etc.';

-- ============================================================================
-- NOTIFICATIONS & COMMUNICATION
-- ============================================================================

-- Notifications: Multi-channel notification log
CREATE TABLE notifications (
    id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    tenant_id UUID NOT NULL REFERENCES tenants(id) ON DELETE CASCADE,
    user_id UUID REFERENCES users(id) ON DELETE SET NULL,
    channel INT NOT NULL,  -- Enum: 0=unspecified, 1=email, 2=sms, 3=push, 4=webhook
    status INT NOT NULL DEFAULT 1,  -- Enum: 0=unspecified, 1=pending, 2=sent, 3=delivered, 4=failed, 5=bounced
    payload JSONB NOT NULL,
    error_message TEXT,
    retry_count INT NOT NULL DEFAULT 0,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    sent_at TIMESTAMP WITH TIME ZONE,
    delivered_at TIMESTAMP WITH TIME ZONE,

    CONSTRAINT notification_tenant_isolation CHECK (tenant_id IS NOT NULL),
    CONSTRAINT notification_retry_limit CHECK (retry_count <= 5)
);

CREATE INDEX idx_notifications_tenant ON notifications(tenant_id);
CREATE INDEX idx_notifications_user ON notifications(user_id);
CREATE INDEX idx_notifications_channel_status ON notifications(channel, status);
CREATE INDEX idx_notifications_created ON notifications(created_at DESC);
CREATE INDEX idx_notifications_retry ON notifications(status, retry_count) WHERE status = 4;  -- Failed notifications

COMMENT ON COLUMN notifications.channel IS '1=email, 2=sms, 3=push, 4=webhook';
COMMENT ON COLUMN notifications.status IS '1=pending, 2=sent, 3=delivered, 4=failed, 5=bounced';

-- Notification Preferences: User notification settings
CREATE TABLE notification_preferences (
    user_id UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    tenant_id UUID NOT NULL REFERENCES tenants(id) ON DELETE CASCADE,
    email_enabled BOOLEAN NOT NULL DEFAULT TRUE,
    sms_enabled BOOLEAN NOT NULL DEFAULT FALSE,
    push_enabled BOOLEAN NOT NULL DEFAULT TRUE,
    marketing_emails BOOLEAN NOT NULL DEFAULT FALSE,
    notification_types JSONB DEFAULT '{}',
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),

    PRIMARY KEY (user_id, tenant_id),
    CONSTRAINT preference_tenant_isolation CHECK (tenant_id IS NOT NULL)
);

CREATE INDEX idx_notification_prefs_tenant ON notification_preferences(tenant_id);

COMMENT ON COLUMN notification_preferences.notification_types IS 'Per-type preferences: {"login": true, "payment": true, "report": false}';

-- Webhooks: Outbound webhook endpoints
CREATE TABLE webhooks (
    id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    tenant_id UUID NOT NULL REFERENCES tenants(id) ON DELETE CASCADE,
    url VARCHAR(2048) NOT NULL,
    secret VARCHAR(64),  -- HMAC signature secret
    events TEXT NOT NULL,  -- Comma-separated: "user.created,payment.succeeded"
    status VARCHAR(20) NOT NULL DEFAULT 'active',
    failure_count INT NOT NULL DEFAULT 0,
    last_triggered_at TIMESTAMP WITH TIME ZONE,
    last_success_at TIMESTAMP WITH TIME ZONE,
    last_failure_at TIMESTAMP WITH TIME ZONE,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),

    CONSTRAINT webhook_tenant_isolation CHECK (tenant_id IS NOT NULL),
    CONSTRAINT webhook_status_check CHECK (status IN ('active', 'disabled', 'suspended')),
    CONSTRAINT webhook_url_check CHECK (url ~ '^https?://')
);

CREATE INDEX idx_webhooks_tenant ON webhooks(tenant_id) WHERE status = 'active';
CREATE INDEX idx_webhooks_status ON webhooks(status);
CREATE INDEX idx_webhooks_failure_count ON webhooks(failure_count) WHERE failure_count > 5;

COMMENT ON COLUMN webhooks.failure_count IS 'Disable webhook after 10 consecutive failures';

-- Email Templates: Transactional email templates
CREATE TABLE email_templates (
    id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    tenant_id UUID REFERENCES tenants(id) ON DELETE CASCADE,  -- NULL for system templates
    name VARCHAR(100) NOT NULL,
    slug VARCHAR(100) NOT NULL,
    subject VARCHAR(255) NOT NULL,
    body_html TEXT NOT NULL,
    body_text TEXT NOT NULL,
    variables JSONB DEFAULT '[]',  -- ["user_name", "reset_link", "otp_code"]
    is_system BOOLEAN NOT NULL DEFAULT FALSE,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),

    CONSTRAINT email_template_slug_unique UNIQUE (tenant_id, slug)
);

CREATE INDEX idx_email_templates_tenant ON email_templates(tenant_id);
CREATE INDEX idx_email_templates_slug ON email_templates(slug);

COMMENT ON COLUMN email_templates.variables IS 'Template variables for Mustache/Handlebars: ["{{user_name}}", "{{reset_link}}"]';

-- ============================================================================
-- AUDIT & COMPLIANCE
-- ============================================================================

-- Audit Log: Security and compliance audit trail
CREATE TABLE audit_log (
    id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    tenant_id UUID REFERENCES tenants(id) ON DELETE SET NULL,
    user_id UUID REFERENCES users(id) ON DELETE SET NULL,
    action VARCHAR(100) NOT NULL,  -- user.login, subscription.created, file.deleted
    resource_type VARCHAR(50) NOT NULL,
    resource_id UUID,
    ip_address INET,
    user_agent TEXT,
    metadata JSONB DEFAULT '{}',
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

CREATE INDEX idx_audit_log_tenant ON audit_log(tenant_id, created_at DESC);
CREATE INDEX idx_audit_log_user ON audit_log(user_id, created_at DESC);
CREATE INDEX idx_audit_log_action ON audit_log(action, created_at DESC);
CREATE INDEX idx_audit_log_resource ON audit_log(resource_type, resource_id);

COMMENT ON TABLE audit_log IS '7-year retention for financial events, 1-year for operational events (GDPR Section 13.1)';

-- ============================================================================
-- FUNCTIONS & TRIGGERS
-- ============================================================================

-- Updated_at trigger function
CREATE OR REPLACE FUNCTION update_updated_at_column()
RETURNS TRIGGER AS $$
BEGIN
    NEW.updated_at = NOW();
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

-- Apply updated_at trigger to all relevant tables
CREATE TRIGGER update_tenants_updated_at BEFORE UPDATE ON tenants FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();
CREATE TRIGGER update_users_updated_at BEFORE UPDATE ON users FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();
CREATE TRIGGER update_roles_updated_at BEFORE UPDATE ON roles FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();
CREATE TRIGGER update_oauth_accounts_updated_at BEFORE UPDATE ON oauth_accounts FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();
CREATE TRIGGER update_subscriptions_updated_at BEFORE UPDATE ON subscriptions FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();
CREATE TRIGGER update_invoices_updated_at BEFORE UPDATE ON invoices FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();
CREATE TRIGGER update_notification_preferences_updated_at BEFORE UPDATE ON notification_preferences FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();
CREATE TRIGGER update_webhooks_updated_at BEFORE UPDATE ON webhooks FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();
CREATE TRIGGER update_email_templates_updated_at BEFORE UPDATE ON email_templates FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();
CREATE TRIGGER update_quotas_updated_at BEFORE UPDATE ON quotas FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();

-- ============================================================================
-- INITIAL DATA
-- ============================================================================

-- System roles (cannot be modified)
INSERT INTO roles (id, tenant_id, name, description, permissions, is_system) VALUES
('00000000-0000-0000-0000-000000000001', NULL, 'admin', 'Full system access', '["admin:*"]', TRUE),
('00000000-0000-0000-0000-000000000002', NULL, 'user', 'Standard user access', '["read:own", "write:own"]', TRUE),
('00000000-0000-0000-0000-000000000003', NULL, 'guest', 'Read-only access', '["read:public"]', TRUE);

-- System email templates
INSERT INTO email_templates (id, tenant_id, name, slug, subject, body_html, body_text, variables, is_system) VALUES
('00000000-0000-0000-0000-000000000010', NULL, 'Welcome Email', 'welcome',
 'Welcome to {{tenant_name}}!',
 '<h1>Welcome {{user_name}}!</h1><p>Thanks for joining {{tenant_name}}.</p>',
 'Welcome {{user_name}}!\n\nThanks for joining {{tenant_name}}.',
 '["user_name", "tenant_name"]', TRUE),

('00000000-0000-0000-0000-000000000011', NULL, 'Password Reset', 'password_reset',
 'Reset your password',
 '<h1>Password Reset</h1><p>Click here to reset: <a href="{{reset_link}}">{{reset_link}}</a></p><p>Expires in 1 hour.</p>',
 'Password Reset\n\nClick here to reset: {{reset_link}}\n\nExpires in 1 hour.',
 '["user_name", "reset_link"]', TRUE),

('00000000-0000-0000-0000-000000000012', NULL, 'OTP Code', 'otp_code',
 'Your verification code: {{otp_code}}',
 '<h1>Verification Code</h1><p>Your code is: <strong>{{otp_code}}</strong></p><p>Expires in 10 minutes.</p>',
 'Verification Code\n\nYour code is: {{otp_code}}\n\nExpires in 10 minutes.',
 '["user_name", "otp_code"]', TRUE);

-- ============================================================================
-- VIEWS FOR REPORTING
-- ============================================================================

-- Active subscriptions with MRR
CREATE VIEW v_active_subscriptions AS
SELECT
    s.id,
    s.tenant_id,
    t.name AS tenant_name,
    s.plan_id,
    s.status,
    s.quantity,
    s.mrr,
    s.current_period_start,
    s.current_period_end
FROM subscriptions s
JOIN tenants t ON s.tenant_id = t.id
WHERE s.status = 1  -- Active
  AND t.deleted_at IS NULL;

-- Tenant storage usage
CREATE VIEW v_tenant_storage_usage AS
SELECT
    q.tenant_id,
    t.name AS tenant_name,
    q.used_bytes,
    q.limit_bytes,
    ROUND((q.used_bytes::DECIMAL / q.limit_bytes) * 100, 2) AS usage_percent,
    q.file_count,
    q.file_count_limit
FROM quotas q
JOIN tenants t ON q.tenant_id = t.id
WHERE t.deleted_at IS NULL;

-- Failed notification summary
CREATE VIEW v_failed_notifications AS
SELECT
    tenant_id,
    channel,
    COUNT(*) AS failure_count,
    MAX(created_at) AS last_failure
FROM notifications
WHERE status = 4  -- Failed
  AND created_at > NOW() - INTERVAL '24 hours'
GROUP BY tenant_id, channel;

-- ============================================================================
-- COMMENTS & DOCUMENTATION
-- ============================================================================

COMMENT ON DATABASE saasforge IS 'SaaSForge multi-tenant SaaS boilerplate - PostgreSQL 15+';
COMMENT ON SCHEMA public IS 'Main schema for all SaaSForge entities';

COMMENT ON TABLE tenants IS 'Top-level isolation boundary. All resources belong to exactly one tenant.';
COMMENT ON TABLE users IS 'Authentication principals with Argon2id password hashing';
COMMENT ON TABLE sessions IS 'Active user sessions with 30-day refresh token expiry';
COMMENT ON TABLE api_keys IS 'Long-lived programmatic access tokens with scope restrictions';
COMMENT ON TABLE upload_objects IS 'File storage metadata with virus scanning and transformations';
COMMENT ON TABLE subscriptions IS 'Stripe subscription management with MRR tracking';
COMMENT ON TABLE notifications IS 'Multi-channel notification delivery log (email, SMS, push, webhook)';
COMMENT ON TABLE audit_log IS 'Security and compliance audit trail with 7-year retention for financial events';

-- Grant permissions (adjust for your environment)
-- GRANT SELECT, INSERT, UPDATE, DELETE ON ALL TABLES IN SCHEMA public TO saasforge_app;
-- GRANT USAGE, SELECT ON ALL SEQUENCES IN SCHEMA public TO saasforge_app;
