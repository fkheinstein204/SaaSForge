# CI/CD Documentation - SaaSForge

**Version:** 1.0
**Last Updated:** 2025-11-12

## Overview

SaaSForge uses a hybrid CI/CD approach with **self-hosted runners** for performance and cost efficiency, with automatic fallback to **GitHub-hosted runners** for reliability and security isolation (especially on pull requests).

### Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                      GitHub Actions                          │
├─────────────────────────────────────────────────────────────┤
│  Event Type │ Runner Selection │ Artifacts │ Docker Registry│
│─────────────┼──────────────────┼───────────┼────────────────│
│  push       │ self-hosted      │ /srv/*    │ localhost:5000 │
│  PR         │ ubuntu-latest    │ ./        │ Docker Hub     │
│  release    │ ubuntu-latest    │ ./        │ Docker Hub     │
└─────────────────────────────────────────────────────────────┘
```

### Workflows

| Workflow | File | Triggers | Purpose |
|----------|------|----------|---------|
| **CI Pipeline** | `.github/workflows/ci.yml` | `push`, `pull_request` | Build & test all 3 layers (C++, Python, React) |
| **Deploy** | `.github/workflows/deploy.yml` | `push`, `pull_request`, `release` | Build Docker images & deploy to environments |
| **Docker Build** | `.github/workflows/docker-build.yml` | `workflow_call` | Reusable workflow for building individual services |
| **Load Tests** | `.github/workflows/load-test.yml` | Manual, scheduled | Performance testing with k6 |
| **Review Agents** | `.github/workflows/agents.yml` | `pull_request` | Automated code review |

---

## Self-Hosted Runners

### Runner Configuration

**Hostnames:** `nuc-01`, `nuc-02`

**Labels:**
- `self-hosted`
- `Linux`
- `X64`

**Persistent Paths:**
- **Cache:** `/srv/cache/` (ccache, pip, npm, docker)
- **Artifacts:** `/srv/artifacts/${{ github.run_id }}/`

**Requirements:**
- Docker installed and running
- Local Docker registry at `localhost:5000` (optional, fallback to Docker Hub)
- Minimum 8GB RAM, 50GB disk space

### Setting Up Self-Hosted Runners

1. **Install GitHub Actions Runner**
   ```bash
   mkdir -p ~/actions-runner && cd ~/actions-runner
   curl -o actions-runner-linux-x64-2.311.0.tar.gz -L \
     https://github.com/actions/runner/releases/download/v2.311.0/actions-runner-linux-x64-2.311.0.tar.gz
   tar xzf ./actions-runner-linux-x64-2.311.0.tar.gz
   ```

2. **Configure Runner**
   ```bash
   ./config.sh --url https://github.com/YOUR_ORG/SaaSForge \
     --token YOUR_REGISTRATION_TOKEN \
     --name nuc-01 \
     --labels self-hosted,Linux,X64
   ```

3. **Create Persistent Directories**
   ```bash
   sudo mkdir -p /srv/cache/{ccache,pip,npm,docker}
   sudo mkdir -p /srv/artifacts
   sudo chown -R $USER:$USER /srv/cache /srv/artifacts
   ```

4. **Set Up Local Docker Registry (Optional)**
   ```bash
   docker run -d -p 5000:5000 --restart=always \
     --name registry \
     -v /srv/docker-registry:/var/lib/registry \
     registry:2
   ```

5. **Start Runner as Service**
   ```bash
   sudo ./svc.sh install
   sudo ./svc.sh start
   ```

### Runner Selection Logic

Workflows use this pattern to select runners:

```yaml
runs-on: ${{ (github.event_name == 'push' && 'self-hosted') || 'ubuntu-latest' }}
```

**Decision Tree:**
- `push` to `main` or `develop` → `self-hosted` (nuc-01 or nuc-02)
- `pull_request` → `ubuntu-latest` (GitHub-hosted)
- `release` → `ubuntu-latest` (GitHub-hosted)

**Rationale:**
- **Pushes:** Use self-hosted for speed (cached dependencies, local registry)
- **PRs:** Use GitHub-hosted for security isolation (untrusted code from forks)
- **Releases:** Use GitHub-hosted for consistency and audit trail

---

## CI Pipeline (`.github/workflows/ci.yml`)

### Jobs

1. **validate** - Security scanning (Gitleaks, Semgrep, Trivy)
2. **build-cpp** - Build & test 4 C++ gRPC services (matrix strategy)
3. **build-api** - Build & test FastAPI BFF
4. **build-ui** - Build & test React UI
5. **integration-tests** - End-to-end integration tests
6. **ci-success** - Aggregate status & PR comment

### Environment Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `CMAKE_BUILD_TYPE` | `Release` | CMake build type |
| `NODE_VERSION` | `20` | Node.js version |
| `PYTHON_VERSION` | `3.11` | Python version |

### Cache Strategy

**Self-Hosted Runners:**
```yaml
cache-dir: /srv/cache/cpp-deps
artifact-dir: /srv/artifacts/${{ github.run_id }}
```

**GitHub-Hosted Runners:**
```yaml
cache-dir: $HOME/.cache/cpp-deps
artifact-dir: ./artifacts
```

Cache is managed by:
- **C++:** `actions/cache` for third_party + ccache via `ccache-action`
- **Python:** `actions/cache` for pip packages
- **Node.js:** `actions/setup-node` with built-in cache

### Coverage Reporting

All 3 layers report coverage to Codecov:

```yaml
- uses: codecov/codecov-action@v4
  with:
    files: ./coverage.xml
    flags: python-api
    token: ${{ secrets.CODECOV_TOKEN }}
```

**Flags:**
- `cpp-auth`, `cpp-upload`, `cpp-payment`, `cpp-notification`
- `python-api`
- `react-ui`

---

## Deployment Pipeline (`.github/workflows/deploy.yml`)

### Deployment Environments

| Environment | Trigger | URL | Namespace |
|-------------|---------|-----|-----------|
| **Preview** | Pull Request | `pr-{N}.dev.saasforge.io` | `saasforge-pr-{N}` |
| **Staging** | Push to `main` | `staging.saasforge.io` | `saasforge-staging` |
| **Production** | Release (`v*.*.*`) | `saasforge.io` | `saasforge-prod` |

### Docker Registry Strategy

**Local Registry (Self-Hosted):**
1. Check if `localhost:5000` is available
2. If yes → push to `localhost:5000/saasforge-{service}:{tag}`
3. If no → fallback to Docker Hub

**Docker Hub (GitHub-Hosted / Fallback):**
- Push to `docker.io/{namespace}/saasforge-{service}:{tag}`
- Namespace from `${{ secrets.DOCKER_HUB_NAMESPACE }}` or `${{ github.repository_owner }}`

**Example Tag Patterns:**

| Event | Tags |
|-------|------|
| Release `v1.2.3` | `1.2.3`, `1.2`, `1`, `latest` |
| Push to `main` | `staging`, `main-abc1234` |
| Push to `develop` | `develop`, `dev-abc1234` |
| PR #42 | `pr-42-abc1234` |

### Deployment Workflow

**Preview (PRs):**
1. Build Docker images → `pr-{N}-{SHA}`
2. Create namespace `saasforge-pr-{N}`
3. Deploy via Kustomize to dev cluster
4. Run smoke tests
5. Comment PR with preview URL

**Staging (main branch):**
1. Build Docker images → `staging`, `main-{SHA}`
2. Run database migrations (Alembic)
3. Deploy via Kustomize to staging cluster
4. Run smoke tests
5. **Rollback on failure**

**Production (releases):**
1. Build Docker images → `{version}`, `{major}.{minor}`, `{major}`, `latest`
2. Create database backup
3. Run database migrations (Alembic)
4. Deploy via Kustomize to production cluster
5. Run smoke tests
6. **Rollback on failure**
7. Notify via Slack

### Smoke Tests

Automated smoke tests run after every deployment:

```bash
./scripts/smoke-test.sh https://staging.saasforge.io
```

**Tests:**
- ✓ Health check (`/health`)
- ✓ OpenAPI docs (`/docs`)
- ✓ API endpoints exist (return 401/422, not 404)
- ✓ CORS headers
- ✓ Response time < 1s
- ✓ gRPC services (if accessible)

**On Failure:** Automatic rollback via `kubectl rollout undo`

---

## Secrets & Environment Variables

### Required Secrets

| Secret | Usage | Example |
|--------|-------|---------|
| `DOCKER_HUB_USERNAME` | Docker Hub login | `myorg` |
| `DOCKER_HUB_TOKEN` | Docker Hub API token | `dckr_pat_...` |
| `DOCKER_HUB_NAMESPACE` | Docker Hub namespace | `myorg` or `myuser` |
| `KUBE_CONFIG_DEV` | Dev cluster kubeconfig | Base64-encoded |
| `KUBE_CONFIG_STAGING` | Staging cluster kubeconfig | Base64-encoded |
| `KUBE_CONFIG_PRODUCTION` | Production cluster kubeconfig | Base64-encoded |
| `CODECOV_TOKEN` | Codecov upload token | `abcd1234...` |
| `SLACK_WEBHOOK` | Slack notifications | `https://hooks.slack.com/...` |

### Setting Up Secrets

**Via GitHub UI:**
```
Settings → Secrets and variables → Actions → New repository secret
```

**Via GitHub CLI:**
```bash
gh secret set DOCKER_HUB_USERNAME --body "myorg"
gh secret set DOCKER_HUB_TOKEN --body "$(cat ~/docker-token.txt)"
```

**Kubeconfig Setup:**
```bash
# Generate base64-encoded kubeconfig
cat ~/.kube/config-staging | base64 -w 0 > staging-kubeconfig.b64
gh secret set KUBE_CONFIG_STAGING < staging-kubeconfig.b64
rm staging-kubeconfig.b64
```

---

## Docker Images

### Services

| Service | Dockerfile | Port | Description |
|---------|------------|------|-------------|
| **auth** | `docker/auth.Dockerfile` | 50051 | Authentication & authorization |
| **upload** | `docker/upload.Dockerfile` | 50052 | File upload & processing |
| **payment** | `docker/payment.Dockerfile` | 50053 | Payments & subscriptions |
| **notification** | `docker/notification.Dockerfile` | 50054 | Multi-channel notifications |
| **api** | `docker/api.Dockerfile` | 8000 | FastAPI BFF |
| **ui** | `docker/ui.Dockerfile` | 3000 | React frontend |

### Multi-Stage Build Pattern

All C++ services use multi-stage builds:

```dockerfile
# Builder stage
FROM ubuntu:22.04 AS builder
RUN apt-get install build-essential cmake ...
COPY . .
RUN cmake .. && make -j$(nproc) {service}_service

# Runtime stage
FROM ubuntu:22.04
COPY --from=builder /build/build/{service}/{service}_service /usr/local/bin/
CMD ["{service}_service"]
```

**Benefits:**
- Smaller runtime images (100MB vs 1GB)
- No build tools in production images
- Reproducible builds

### Build & Push Manually

**Using Local Registry:**
```bash
docker build -f docker/auth.Dockerfile -t localhost:5000/saasforge-auth:dev .
docker push localhost:5000/saasforge-auth:dev
```

**Using Docker Hub:**
```bash
docker build -f docker/auth.Dockerfile -t myorg/saasforge-auth:dev .
docker push myorg/saasforge-auth:dev
```

---

## Running CI Locally

### C++ Services

```bash
# Install system dependencies
sudo apt-get install build-essential cmake git pkg-config \
  libssl-dev libpq-dev libgrpc++-dev libprotobuf-dev \
  protobuf-compiler-grpc libhiredis-dev libpqxx-dev \
  libgtest-dev libargon2-dev lcov

# Install third-party C++ dependencies
./scripts/install-cpp-deps.sh

# Generate protobuf stubs
./scripts/gen-proto.sh

# Build
cd services/cpp
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_C_COMPILER_LAUNCHER=ccache \
      -DCMAKE_CXX_COMPILER_LAUNCHER=ccache ..
make -j$(nproc)

# Run tests
ctest --output-on-failure -j$(nproc)

# Generate coverage
lcov --capture --directory . --output-file coverage.info
lcov --remove coverage.info '/usr/*' '*/tests/*' '*/third_party/*' --output-file coverage.info
```

### FastAPI BFF

```bash
cd api

# Create virtual environment
python3 -m venv venv
source venv/bin/activate

# Install dependencies
pip install -r requirements.txt
pip install pytest-cov coverage[toml]

# Generate Python gRPC stubs
pip install grpcio-tools
mkdir -p generated
cd ../proto
for proto_file in *.proto; do
  python -m grpc_tools.protoc -I. \
    --python_out=../api/generated \
    --grpc_python_out=../api/generated \
    "$proto_file"
done
cd ../api

# Run linting
black . --check
flake8 .
mypy .

# Run tests with coverage
pytest -v --cov=. --cov-report=xml --cov-report=html
```

### React UI

```bash
cd ui

# Install dependencies
npm ci

# Run linting
npm run lint

# Type checking
npx tsc --noEmit

# Run tests
npm test -- --coverage --watchAll=false

# Build production bundle
npm run build
```

---

## Monitoring CI/CD

### GitHub Actions UI

**Workflow Runs:**
```
Actions → CI Pipeline → [Run #123]
```

**Job Logs:**
```
Actions → CI Pipeline → [Run #123] → build-cpp → auth_service
```

### Self-Hosted Runner Status

**Check Runner Health:**
```bash
# On self-hosted runner
cd ~/actions-runner
./run.sh status

# View logs
journalctl -u actions.runner.* -f
```

**Disk Usage:**
```bash
du -sh /srv/cache/*
du -sh /srv/artifacts/*

# Clean old artifacts (older than 7 days)
find /srv/artifacts -type d -mtime +7 -exec rm -rf {} +
```

### Debugging Failed Builds

**Enable Debug Logging:**

Add secrets to repository:
```bash
gh secret set ACTIONS_STEP_DEBUG --body "true"
gh secret set ACTIONS_RUNNER_DEBUG --body "true"
```

**SSH into Runner (tmate):**

Add this step temporarily:
```yaml
- name: Debug via SSH
  uses: mxschmitt/action-tmate@v3
  if: failure()
```

---

## Performance Optimization

### Cache Hit Rates

**Target Metrics:**
- C++ dependencies: > 95%
- pip packages: > 90%
- npm packages: > 90%
- Docker layers (self-hosted): > 80%

**Check Cache Stats:**
```bash
# ccache stats
ccache -s

# Docker BuildX cache
docker buildx du
```

### Build Times

**Baseline (GitHub-hosted):**
- C++ services: ~8 minutes (cold), ~5 minutes (warm)
- FastAPI BFF: ~3 minutes (cold), ~1 minute (warm)
- React UI: ~4 minutes (cold), ~2 minutes (warm)

**Self-Hosted (with local cache):**
- C++ services: ~3 minutes (cold), ~1 minute (warm)
- FastAPI BFF: ~1 minute (cold), ~30 seconds (warm)
- React UI: ~2 minutes (cold), ~45 seconds (warm)

### Optimizations Applied

1. **Matrix builds** - Build 4 C++ services in parallel
2. **ccache** - Cache C++ object files (500MB limit)
3. **Docker layer cache** - BuildX cache with type=local (self-hosted)
4. **Persistent /srv/cache** - Avoid re-downloading dependencies
5. **Inline script generation** - Create missing scripts on-the-fly
6. **Conditional artifact storage** - Local storage on self-hosted, upload on GitHub-hosted

---

## Security Considerations

### Code Scanning

**Automated Scans:**
- **Gitleaks** - Detect secrets in commits
- **Semgrep** - SAST for security vulnerabilities (OWASP Top 10)
- **Trivy** - Container image vulnerabilities
- **SBOM** - CycloneDX software bill of materials

**Results:** Uploaded to GitHub Security tab (`/security/code-scanning`)

### Pull Request Isolation

**Why GitHub-hosted for PRs?**
- Untrusted code from forks
- No access to self-hosted runner secrets
- Clean environment for every run
- Prevents cache poisoning

### Secret Management

**Best Practices:**
- Use GitHub Secrets, never hardcode
- Rotate tokens every 90 days
- Use environment protection rules for production
- Require approval for production deployments

**Environment Protection (Production):**
```
Settings → Environments → production
✓ Required reviewers: 2
✓ Wait timer: 0 minutes
✓ Deployment branches: v*.*.* only
```

---

## Troubleshooting

### Issue: Docker login failed

**Error:**
```
Error: Cannot perform an interactive login from a non TTY device
```

**Fix:**
```bash
# Ensure secrets are set
gh secret list | grep DOCKER_HUB

# Verify token has push permissions
docker login -u $DOCKER_HUB_USERNAME -p $DOCKER_HUB_TOKEN
```

---

### Issue: Local registry unavailable

**Error:**
```
⚠️ Local registry not available, falling back to Docker Hub
```

**Fix:**
```bash
# Start local registry
docker run -d -p 5000:5000 --restart=always --name registry registry:2

# Verify
curl http://localhost:5000/v2/
```

---

### Issue: Kubernetes deployment failed

**Error:**
```
error: error validating "STDIN": error validating data: ValidationError
```

**Fix:**
```bash
# Check kubeconfig secret
gh secret list | grep KUBE_CONFIG

# Validate kubeconfig locally
echo "$KUBE_CONFIG_STAGING" | base64 -d > /tmp/kubeconfig
export KUBECONFIG=/tmp/kubeconfig
kubectl cluster-info
```

---

### Issue: Smoke tests failed

**Error:**
```
❌ Some smoke tests failed!
Testing: API health check... ✗ FAIL (Expected HTTP 200, got 000)
```

**Fix:**
```bash
# Check if service is actually running
kubectl get pods -n saasforge-staging

# Check service logs
kubectl logs deployment/api -n saasforge-staging --tail=50

# Manual smoke test
./scripts/smoke-test.sh https://staging.saasforge.io
```

---

### Issue: Build artifacts missing

**Error:**
```
warning: Pattern './services/cpp/build/auth/auth_service' did not match any files
```

**Fix:**
```bash
# Check CMake build output
cat services/cpp/build/CMakeCache.txt | grep CMAKE_BUILD_TYPE

# Verify binary was built
ls -la services/cpp/build/auth/

# Check CI logs for build errors
```

---

## Maintenance

### Weekly Tasks

- Review failed builds and fix root causes
- Check disk usage on self-hosted runners (`/srv/cache`, `/srv/artifacts`)
- Rotate Docker Hub tokens if approaching 6TB pull limit

### Monthly Tasks

- Update GitHub Actions versions (Dependabot PRs)
- Review Codecov trends (aim for > 80% coverage)
- Audit unused secrets and environment variables

### Quarterly Tasks

- Review and optimize cache sizes (ccache, Docker layers)
- Update base images (Ubuntu, Node.js, Python)
- Performance benchmarking (compare against baselines)

---

## References

**GitHub Actions Documentation:**
- [Self-hosted runners](https://docs.github.com/en/actions/hosting-your-own-runners)
- [Workflow syntax](https://docs.github.com/en/actions/using-workflows/workflow-syntax-for-github-actions)
- [Reusable workflows](https://docs.github.com/en/actions/using-workflows/reusing-workflows)

**Docker Documentation:**
- [BuildX](https://docs.docker.com/buildx/working-with-buildx/)
- [Multi-stage builds](https://docs.docker.com/develop/develop-images/multistage-build/)
- [Local registry](https://docs.docker.com/registry/deploying/)

**SaaSForge Documentation:**
- [README.md](./README.md) - Project overview
- [CLAUDE.md](./CLAUDE.md) - Development guide
- [docs/srs-boilerplate-saas.md](./docs/srs-boilerplate-saas.md) - Full requirements

---

**Document Version:** 1.0
**Maintained By:** DevOps Team
**Questions?** Open an issue or contact via Slack #devops-ci-cd
