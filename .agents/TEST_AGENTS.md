# Testing the Review Agents

## Quick Test Commands

### Test Security Agent
```bash
# Test on existing auth code
python3 .agents/security/review.py api/routers/auth.py

# Expected: Warnings about not-implemented endpoints
```

### Test Architect Agent
```bash
# Test on full codebase
python3 .agents/architect/review.py .

# Expected: Some warnings about TODO implementations
```

### Test Database Agent
```bash
# Test on migrations
python3 .agents/database/review.py db/migrations/

# Expected: Pass - all migrations have tenant_id and indexes
```

### Test All Agents
```bash
# Run all agents on codebase
python3 .agents/run_all.py .
```

## Creating Test Violations

To verify agents are working, create test files with violations:

### Security Violation Test
```python
# test_security_violation.py
import jwt

# ❌ This should trigger CRITICAL alert
token = jwt.decode(user_token, verify=False)  # Missing algorithm specification

# ❌ This should trigger HIGH alert
query = f"SELECT * FROM users WHERE email = '{email}'"  # SQL injection
```

```bash
python3 .agents/security/review.py test_security_violation.py
# Expected: 2 issues (1 critical, 1 high)
```

### Architecture Violation Test
```typescript
// test_architecture_violation.tsx
import { createClient } from '@/lib/grpc';

// ❌ This should trigger CRITICAL alert
const UploadPage = () => {
  const grpcClient = createClient();  // UI bypassing BFF
  
  const handleUpload = async () => {
    await grpcClient.uploadFile();  // Direct gRPC call
  };
};
```

```bash
python3 .agents/architect/review.py test_architecture_violation.tsx
# Expected: 1 critical issue (ARCH-001)
```

## Verifying CI Integration

```bash
# Simulate CI run locally
.github/workflows/agents.yml

# Or push to a test branch
git checkout -b test-agents
git add .agents/
git commit -m "test: verify agents in CI"
git push origin test-agents
# Create PR and check Actions tab
```

## Agent Performance

Measure agent execution time:

```bash
time python3 .agents/run_all.py .

# Expected: < 30 seconds for entire codebase
# Breakdown:
#   Security: ~5s
#   Architecture: ~3s
#   Architect: ~4s
#   Performance: ~3s
#   Database: ~1s
#   Testing: ~2s
```

## Debugging Agents

Add verbose output:

```bash
# Python debugging
python3 -m pdb .agents/security/review.py api/

# Print debug info
AGENT_DEBUG=1 python3 .agents/architect/review.py services/
```

## Common Issues

**Issue: `ModuleNotFoundError: No module named 'yaml'`**
```bash
pip install pyyaml
```

**Issue: Permission denied**
```bash
chmod +x .agents/**/*.py
```

**Issue: No issues found (but there should be)**
```bash
# Check config
cat .agents/config.yaml

# Check if checks are enabled
# Verify file patterns are matching
```
