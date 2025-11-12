# CI/CD Review Agent

**Version:** 1.0
**Status:** Active
**Maintainer:** DevOps & Security Teams

## Overview

The CI/CD Review Agent validates GitHub Actions workflows, Kubernetes manifests, Dockerfiles, and deployment configurations for security vulnerabilities and best practices violations.

This is the **8th specialized agent** in the SaaSForge review ecosystem, complementing the existing security, architecture, performance, database, and testing agents by extending protection to the infrastructure and deployment layer.

## Scope

### Supported File Types

| File Type | Pattern | Checks |
|-----------|---------|--------|
| **GitHub Actions Workflows** | `.github/workflows/*.yml` | Security, permissions, action pinning, secrets |
| **Dockerfiles** | `*Dockerfile`, `*.Dockerfile` | Base images, USER directive, multi-stage builds, secrets |
| **Kubernetes Manifests** | `k8s/**/*.yaml` | Resource limits, health probes, security context, RBAC |
| **Docker Compose** | `docker-compose*.yml` | Privileged mode, secrets, health checks, image tags |
| **Kustomize** | `kustomization.yaml` | Namespace, labels, image digests |

### Check Categories

**37 total checks** across 5 categories:

#### 1. GitHub Actions Security (CICD-001 to CICD-010)
- Overly permissive workflow permissions
- Unpinned third-party actions
- Hardcoded secrets in workflows
- Missing timeout values
- Self-hosted runner security for PRs
- Unsafe pull_request_target usage
- Missing concurrency control
- Dangerous pipe to shell commands
- Unprotected deployment jobs
- Missing continue-on-error for non-critical jobs

#### 2. Docker Security (CICD-021 to CICD-030)
- Vulnerable base images (EOL versions)
- Running as root (missing USER directive)
- Hardcoded secrets in layers
- Single-stage builds (bloated images)
- Missing HEALTHCHECK
- Using :latest tag
- Missing --no-install-recommends
- Missing package manager cleanup
- COPY before dependency installation (cache busting)
- Privileged containers

#### 3. Kubernetes Security (CICD-011 to CICD-020)
- Missing resource limits/requests
- Missing health probes (liveness, readiness)
- Privileged containers
- Missing runAsNonRoot security context
- Missing tenant isolation labels
- Hardcoded values instead of ConfigMaps/Secrets
- Missing namespace specification
- hostPath volumes (security risk)
- Missing PodDisruptionBudget
- Missing imagePullPolicy

#### 4. Docker Compose (CICD-031 to CICD-034)
- Privileged mode
- Hardcoded secrets in environment
- Missing health checks
- Using latest tag

#### 5. Kustomize (CICD-035 to CICD-037)
- Missing namespace
- Missing common labels
- Image tag mutations without digest

## Installation

### Prerequisites

- Python 3.11+
- PyYAML library

### Setup

```bash
# Install dependencies
pip install pyyaml

# Make script executable
chmod +x .agents/cicd/review.py

# Test installation
python3 .agents/cicd/review.py --help
```

## Usage

### Manual Execution

```bash
# Review entire repository
python3 .agents/cicd/review.py .

# Review specific directory
python3 .agents/cicd/review.py ./k8s

# Review from project root
cd /path/to/SaaSForge
python3 .agents/cicd/review.py .
```

### CI Integration

The agent automatically runs in GitHub Actions when CI/CD files are modified:

```yaml
# Triggered by changes to:
- .github/workflows/**
- k8s/**
- docker/**
- Dockerfile
- docker-compose.yml
```

See `.github/workflows/agents.yml` for full integration.

## Configuration

### Config File: `.agents/config.yaml`

```yaml
cicd:
  enabled: true
  severity_threshold: medium
  checks:
    workflow_security: true
    action_pinning: true
    secrets_exposure: true
    deployment_safety: true
    kubernetes_best_practices: true
    docker_security: true
  fail_on_critical: true
```

### Configuration Options

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `enabled` | boolean | `true` | Enable/disable agent |
| `severity_threshold` | string | `medium` | Minimum severity to report |
| `fail_on_critical` | boolean | `true` | Exit 1 on critical/high issues |
| `checks.*` | boolean | `true` | Enable/disable specific check categories |

## Output Format

### Console Output

```
================================================================================
CI/CD REVIEW REPORT
================================================================================

🔴 [CRITICAL] CICD-003: Hardcoded secret detected in workflow
   Location: .github/workflows/deploy.yml:15

🟠 [HIGH] CICD-002: Unpinned third-party action "some-org/action" (use @sha256)
   Location: .github/workflows/ci.yml:42

🟡 [MEDIUM] CICD-011: Container "api" missing resource limits
   Location: k8s/api-deployment.yaml:0

🟢 [LOW] CICD-007: Missing concurrency control
   Location: .github/workflows/build.yml:0

================================================================================
Summary: 1 critical, 1 high, 1 medium, 1 low
================================================================================

❌ CI/CD review failed (critical or high severity issues found)
```

### Exit Codes

| Code | Meaning | When |
|------|---------|------|
| 0 | Success | No issues or warnings only |
| 1 | Failure | Critical or high severity issues found |

## Integration with Other Agents

The CI/CD agent works alongside:

1. **Security Agent** - Application-level security (JWT, SQL injection, XSS)
2. **Architecture Agent** - Three-tier architecture enforcement
3. **Performance Agent** - NFR targets and N+1 queries
4. **Database Agent** - Migration quality and multi-tenancy
5. **Testing Agent** - Coverage and test quality

**Workflow:**
```
PR Created
  ↓
Security Agent (app code)
Architecture Agent (layer violations)
Performance Agent (slow queries)
Database Agent (migrations)
Testing Agent (coverage)
CI/CD Agent (workflows/k8s/docker)  ← New!
  ↓
All Agents Pass → Merge Allowed
```

## Examples

### Example 1: GitHub Actions Workflow

**Bad (4 issues):**
```yaml
permissions: write-all  # ❌ CICD-001: Overly permissive

on:
  pull_request:

jobs:
  build:
    runs-on: self-hosted  # ❌ CICD-005: Self-hosted for PRs
    steps:
      - uses: some-org/action@v3  # ❌ CICD-002: Unpinned action
      - run: curl https://install.sh | sh  # ❌ CICD-008: Pipe to shell
```

**Good:**
```yaml
permissions:
  contents: read
  pull-requests: write

on:
  pull_request:

jobs:
  build:
    runs-on: ubuntu-latest  # ✅ GitHub-hosted for PRs
    timeout-minutes: 30
    steps:
      - uses: some-org/action@a1b2c3d...  # ✅ Pinned to SHA
      - run: |
          curl -o install.sh https://install.sh
          chmod +x install.sh && ./install.sh
```

### Example 2: Dockerfile

**Bad (3 issues):**
```dockerfile
FROM ubuntu:18.04  # ❌ CICD-021: Vulnerable base image

RUN apt-get update && apt-get install -y curl
ENV API_KEY=sk-123456  # ❌ CICD-023: Hardcoded secret

CMD ["./app"]  # ❌ CICD-022: Running as root
```

**Good:**
```dockerfile
FROM ubuntu:22.04  # ✅ Supported LTS

RUN apt-get update && \
    apt-get install -y --no-install-recommends curl && \
    rm -rf /var/lib/apt/lists/*

RUN useradd -r -s /bin/false appuser
USER appuser  # ✅ Non-root

CMD ["./app"]
```

### Example 3: Kubernetes Deployment

**Bad (2 issues):**
```yaml
spec:
  template:
    spec:
      containers:
      - name: api
        image: myapp:latest  # ❌ CICD-020: No imagePullPolicy
        # ❌ CICD-011: No resource limits
        # ❌ CICD-012: No health probes
```

**Good:**
```yaml
spec:
  template:
    spec:
      securityContext:
        runAsNonRoot: true  # ✅
      containers:
      - name: api
        image: myapp:v1.2.3
        imagePullPolicy: IfNotPresent  # ✅
        resources:  # ✅
          requests:
            cpu: 100m
            memory: 128Mi
          limits:
            cpu: 500m
            memory: 512Mi
        livenessProbe:  # ✅
          httpGet:
            path: /health
            port: 8000
        readinessProbe:  # ✅
          httpGet:
            path: /health
            port: 8000
```

## Troubleshooting

### Issue: Agent not finding files

**Symptom:**
```
Found 0 CI/CD files to review
```

**Solution:**
```bash
# Ensure you're in project root
pwd  # Should be /path/to/SaaSForge

# Check that files exist
ls .github/workflows/
ls k8s/
ls docker/

# Run from correct directory
cd /path/to/SaaSForge
python3 .agents/cicd/review.py .
```

### Issue: YAML parsing errors

**Symptom:**
```
Invalid YAML syntax in workflow
```

**Solution:**
```bash
# Validate YAML syntax
yamllint .github/workflows/ci.yml

# Check for tabs (use spaces only)
grep -P '\t' .github/workflows/ci.yml
```

### Issue: False positives

**Symptom:**
Agent flags valid patterns as issues.

**Solution:**
1. Check if pattern is actually secure (refer to CICD_RULES.md)
2. If false positive, update `.agents/config.yaml` to disable specific check
3. Report issue to maintainers for agent improvement

## Development

### Adding New Checks

1. Add check method to `CICDReviewAgent` class
2. Assign unique check ID (CICD-XXX)
3. Document in CICD_RULES.md
4. Add test cases
5. Update this README

### Testing

```bash
# Create test files with known issues
mkdir -p test/fixtures

# Run agent on test files
python3 .agents/cicd/review.py test/fixtures

# Verify expected issues are detected
```

## References

- [CICD_RULES.md](./CICD_RULES.md) - Detailed rule documentation
- [GitHub Actions Security](https://docs.github.com/en/actions/security-guides/security-hardening-for-github-actions)
- [Docker Security Best Practices](https://docs.docker.com/develop/security-best-practices/)
- [Kubernetes Security](https://kubernetes.io/docs/concepts/security/)

## Support

**Issues:** Open a GitHub issue with:
- Agent version
- Input files (sanitized)
- Expected vs actual behavior

**Questions:** Slack #devops-agents or #security

## Changelog

### v1.0 (2025-11-12)
- Initial release
- 37 checks across 5 categories
- Support for workflows, Dockerfiles, K8s, Compose, Kustomize
- Integration with GitHub Actions

---

**Maintained By:** DevOps & Security Teams
**License:** Internal Use Only
