-- Sample seed data for development

-- Insert sample tenant
INSERT INTO tenants (id, name, slug, status) VALUES
('11111111-1111-1111-1111-111111111111', 'Demo Company', 'demo-company', 'active');

-- Insert sample user (password: password123)
INSERT INTO users (id, tenant_id, email, password_hash, full_name, email_verified_at) VALUES
('22222222-2222-2222-2222-222222222222',
 '11111111-1111-1111-1111-111111111111',
 'admin@demo.com',
 '$2b$12$LQv3c1yqBWVHxkd0LHAkCOYz6TtxMQJqhN8/LewY5lW0nQtFHmCq6',  -- password123
 'Demo Admin',
 NOW());

-- Insert sample roles
INSERT INTO roles (id, tenant_id, name, permissions) VALUES
('33333333-3333-3333-3333-333333333333',
 '11111111-1111-1111-1111-111111111111',
 'admin',
 '["read:*", "write:*", "delete:*"]'),
('44444444-4444-4444-4444-444444444444',
 '11111111-1111-1111-1111-111111111111',
 'user',
 '["read:own", "write:own"]');

-- Assign admin role to user
INSERT INTO user_roles (user_id, role_id) VALUES
('22222222-2222-2222-2222-222222222222', '33333333-3333-3333-3333-333333333333');

-- Insert sample quota
INSERT INTO quotas (tenant_id, used_bytes, limit_bytes, reset_at) VALUES
('11111111-1111-1111-1111-111111111111', 0, 10737418240, NOW() + INTERVAL '1 month');  -- 10GB

-- Insert sample email templates
INSERT INTO email_templates (tenant_id, name, subject, body_html, body_text, variables) VALUES
('11111111-1111-1111-1111-111111111111',
 'welcome',
 'Welcome to {{company_name}}',
 '<h1>Welcome {{user_name}}!</h1><p>Thanks for joining us.</p>',
 'Welcome {{user_name}}! Thanks for joining us.',
 '["company_name", "user_name"]');
