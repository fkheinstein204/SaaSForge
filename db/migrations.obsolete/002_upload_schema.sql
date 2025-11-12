-- Upload and file management schema

-- Upload objects table
CREATE TABLE upload_objects (
    id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    tenant_id UUID NOT NULL REFERENCES tenants(id),
    user_id UUID NOT NULL REFERENCES users(id),
    filename VARCHAR(255) NOT NULL,
    content_type VARCHAR(100) NOT NULL,
    size_bytes BIGINT NOT NULL,
    checksum_sha256 VARCHAR(64) NOT NULL,
    storage_key VARCHAR(500) NOT NULL,
    storage_url TEXT NOT NULL,
    status VARCHAR(50) NOT NULL DEFAULT 'pending',
    virus_scan_result VARCHAR(50),
    metadata JSONB DEFAULT '{}',
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

-- Quotas table
CREATE TABLE quotas (
    id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    tenant_id UUID NOT NULL REFERENCES tenants(id) UNIQUE,
    used_bytes BIGINT NOT NULL DEFAULT 0,
    limit_bytes BIGINT NOT NULL,
    reset_at TIMESTAMP WITH TIME ZONE NOT NULL,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

-- Transformation profiles table
CREATE TABLE transform_profiles (
    id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    tenant_id UUID NOT NULL REFERENCES tenants(id),
    name VARCHAR(100) NOT NULL,
    config JSONB NOT NULL,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    CONSTRAINT unique_tenant_profile UNIQUE (tenant_id, name)
);

-- Transformation jobs table
CREATE TABLE transform_jobs (
    id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    tenant_id UUID NOT NULL REFERENCES tenants(id),
    object_id UUID NOT NULL REFERENCES upload_objects(id),
    profile_id UUID NOT NULL REFERENCES transform_profiles(id),
    status VARCHAR(50) NOT NULL DEFAULT 'pending',
    output_object_id UUID REFERENCES upload_objects(id),
    error_message TEXT,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    completed_at TIMESTAMP WITH TIME ZONE
);

-- Indexes
CREATE INDEX idx_upload_objects_tenant_id ON upload_objects(tenant_id);
CREATE INDEX idx_upload_objects_user_id ON upload_objects(user_id);
CREATE INDEX idx_upload_objects_status ON upload_objects(status);
CREATE INDEX idx_quotas_tenant_id ON quotas(tenant_id);
CREATE INDEX idx_transform_jobs_status ON transform_jobs(status);

COMMENT ON TABLE upload_objects IS 'Uploaded files with metadata and checksums';
COMMENT ON TABLE quotas IS 'Per-tenant storage quotas';
COMMENT ON TABLE transform_profiles IS 'Image/video transformation configurations';
