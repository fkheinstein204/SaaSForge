-- Test Database Schema for SaaSForge Integration Tests
-- Simplified version of main schema with test fixtures

-- Enable UUID extension
CREATE EXTENSION IF NOT EXISTS "uuid-ossp";

-- Tenants table (minimal for testing)
CREATE TABLE IF NOT EXISTS tenants (
    id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    name TEXT NOT NULL,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP
);

-- Users table (minimal for testing)
CREATE TABLE IF NOT EXISTS users (
    id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    tenant_id UUID NOT NULL REFERENCES tenants(id),
    email TEXT NOT NULL UNIQUE,
    password_hash TEXT NOT NULL,
    totp_secret TEXT,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
    deleted_at TIMESTAMP WITH TIME ZONE,
    CONSTRAINT tenant_isolation CHECK (tenant_id IS NOT NULL)
);

-- API Keys table (minimal for testing)
CREATE TABLE IF NOT EXISTS api_keys (
    id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    user_id UUID NOT NULL REFERENCES users(id),
    tenant_id UUID NOT NULL REFERENCES tenants(id),
    key_hash TEXT NOT NULL,
    name TEXT NOT NULL,
    scopes TEXT,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
    expires_at TIMESTAMP WITH TIME ZONE,
    revoked_at TIMESTAMP WITH TIME ZONE,
    CONSTRAINT tenant_isolation CHECK (tenant_id IS NOT NULL)
);

-- Test Fixtures

-- Test tenant
INSERT INTO tenants (id, name) VALUES
    ('00000000-0000-0000-0000-000000000001', 'Test Tenant 1'),
    ('00000000-0000-0000-0000-000000000002', 'Test Tenant 2')
ON CONFLICT (id) DO NOTHING;

-- Test users
-- Password: "test_password_123" hashed with Argon2id
-- Hash: $argon2id$v=19$m=65536,t=3,p=4$c2FsdDEyMzQ1Njc4OTAxMg$VFGZWxU5RkZDQkRFMTIzNDU2Nzg5MEFCQ0RFRjAxMjM
INSERT INTO users (id, tenant_id, email, password_hash) VALUES
    ('10000000-0000-0000-0000-000000000001', '00000000-0000-0000-0000-000000000001',
     'test@example.com', '$argon2id$v=19$m=65536,t=3,p=4$c2FsdDEyMzQ1Njc4OTAxMg$VFGZWxU5RkZDQkRFMTIzNDU2Nzg5MEFCQ0RFRjAxMjM'),
    ('10000000-0000-0000-0000-000000000002', '00000000-0000-0000-0000-000000000002',
     'tenant2@example.com', '$argon2id$v=19$m=65536,t=3,p=4$c2FsdDEyMzQ1Njc4OTAxMg$VFGZWxU5RkZDQkRFMTIzNDU2Nzg5MEFCQ0RFRjAxMjM'),
    ('10000000-0000-0000-0000-000000000003', '00000000-0000-0000-0000-000000000001',
     'deleted@example.com', '$argon2id$v=19$m=65536,t=3,p=4$c2FsdDEyMzQ1Njc4OTAxMg$VFGZWxU5RkZDQkRFMTIzNDU2Nzg5MEFCQ0RFRjAxMjM')
ON CONFLICT (email) DO NOTHING;

-- Soft-delete one user for testing
UPDATE users SET deleted_at = CURRENT_TIMESTAMP
WHERE email = 'deleted@example.com' AND deleted_at IS NULL;

-- Test API key (key: "sk_test_1234567890abcdef")
INSERT INTO api_keys (id, user_id, tenant_id, key_hash, name, scopes, expires_at) VALUES
    ('20000000-0000-0000-0000-000000000001',
     '10000000-0000-0000-0000-000000000001',
     '00000000-0000-0000-0000-000000000001',
     '$argon2id$v=19$m=65536,t=3,p=4$dGVzdGtleXNhbHQxMjM$dGVzdGhhc2gxMjM0NTY3ODkwYWJjZGVmMTIzNDU2',
     'Test API Key',
     'read,write',
     CURRENT_TIMESTAMP + INTERVAL '1 year')
ON CONFLICT (id) DO NOTHING;
