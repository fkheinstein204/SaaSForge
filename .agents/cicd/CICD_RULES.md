# CI/CD Security & Best Practices Rules

**Version:** 1.0
**Last Updated:** 2025-11-12

This document defines the security rules and best practices enforced by the CI/CD Review Agent for SaaSForge.

---

## Table of Contents

1. [GitHub Actions Workflow Security](#github-actions-workflow-security)
2. [Docker Security](#docker-security)
3. [Kubernetes Security](#kubernetes-security)
4. [Docker Compose Best Practices](#docker-compose-best-practices)
5. [Kustomize Configuration](#kustomize-configuration)

---

## GitHub Actions Workflow Security

### CICD-001: Overly Permissive Workflow Permissions

**Severity:** Critical

**Description:**
Using `permissions: write-all` violates the principle of least privilege and can allow malicious code to modify repository contents, publish packages, or access secrets.

**Bad Example:**
```yaml
permissions: write-all  # ❌ Dangerous

jobs:
  build:
    runs-on: ubuntu-latest
    permissions: write-all  # ❌ Also dangerous at job level
```

**Good Example:**
```yaml
permissions:
  contents: read
  packages: write
  pull-requests: write

jobs:
  build:
    runs-on: ubuntu-latest
    permissions:
      contents: read  # ✅ Only what's needed
```

**Rationale:**
- Limits blast radius if workflow is compromised
- Required for GitHub security best practices
- Prevents accidental destructive actions

---

### CICD-002: Unpinned Third-Party Actions

**Severity:** High

**Description:**
Using version tags (`@v3`) instead of commit SHA allows action maintainers to push malicious updates.

**Bad Example:**
```yaml
- uses: some-org/some-action@v3  # ❌ Mutable tag
- uses: another-org/action@main  # ❌ Even worse - tracks branch
```

**Good Example:**
```yaml
# ✅ Pinned to immutable commit SHA
- uses: some-org/some-action@a1b2c3d4e5f6...  # Full SHA-256

# ✅ Trusted publishers can use tags
- uses: actions/checkout@v4  # OK for actions/*, github/*, docker/*
```

**Rationale:**
- Prevents supply chain attacks
- Ensures reproducible builds
- GitHub recommends pinning to SHA for security-critical workflows

**Exception:**
Trusted publishers (actions, github, docker, aws-actions, azure, google-github-actions) are exempt from this rule.

---

### CICD-003: Hardcoded Secrets in Workflows

**Severity:** Critical

**Description:**
Secrets committed to workflow files are visible in repository history and compromise security.

**Bad Example:**
```yaml
env:
  API_KEY: sk-1234567890abcdef  # ❌ Exposed in git history
  DATABASE_URL: postgresql://user:password@host/db  # ❌ Credentials visible
```

**Good Example:**
```yaml
env:
  API_KEY: ${{ secrets.API_KEY }}  # ✅ From GitHub Secrets
  DATABASE_URL: ${{ secrets.DATABASE_URL }}  # ✅ Secure
```

**Rationale:**
- Secrets in git history are permanent (even if deleted later)
- GitHub Secrets are encrypted and only exposed during workflow execution
- Prevents credential leakage to public repositories

---

### CICD-004: Missing Timeout Values

**Severity:** Medium

**Description:**
Jobs without timeouts can run indefinitely, consuming resources and delaying feedback.

**Bad Example:**
```yaml
jobs:
  build:
    runs-on: ubuntu-latest
    steps:  # ❌ No timeout - could run for 6 hours (GitHub default)
      - run: npm install
```

**Good Example:**
```yaml
jobs:
  build:
    runs-on: ubuntu-latest
    timeout-minutes: 30  # ✅ Fails after 30 minutes
    steps:
      - run: npm install
```

**Rationale:**
- Prevents runaway processes
- Faster failure detection
- Reduces billing for GitHub-hosted runners

**Recommended Timeouts:**
- Build jobs: 10-30 minutes
- Test jobs: 15-45 minutes
- Deployment jobs: 15-60 minutes

---

### CICD-005: Self-Hosted Runners for Pull Requests

**Severity:** Critical

**Description:**
Running untrusted PR code on self-hosted runners can compromise infrastructure.

**Bad Example:**
```yaml
on:
  pull_request:  # ❌ Untrusted code from forks

jobs:
  build:
    runs-on: self-hosted  # ❌ Dangerous - allows code execution on your infrastructure
```

**Good Example:**
```yaml
on:
  pull_request:

jobs:
  build:
    runs-on: ubuntu-latest  # ✅ Isolated GitHub-hosted runner

  deploy:
    runs-on: self-hosted  # ✅ OK for internal workflows only
    if: github.event_name == 'push'  # Only on pushes, not PRs
```

**Rationale:**
- Pull requests from forks can execute malicious code
- Self-hosted runners have access to internal networks and secrets
- GitHub-hosted runners provide isolation and ephemeral environments

**SaaSForge Pattern:**
```yaml
runs-on: ${{ (github.event_name == 'push' && 'self-hosted') || 'ubuntu-latest' }}
```

---

### CICD-006: Unsafe pull_request_target Usage

**Severity:** Critical

**Description:**
`pull_request_target` runs in the context of the base repository with write access, allowing PR code to steal secrets.

**Bad Example:**
```yaml
on:
  pull_request_target:  # ❌ Dangerous event

jobs:
  build:
    steps:
      - uses: actions/checkout@v4  # ❌ Checks out PR code with write access
```

**Good Example:**
```yaml
on:
  pull_request_target:  # Only if needed for comment permissions

jobs:
  build:
    steps:
      - uses: actions/checkout@v4
        with:
          ref: ${{ github.event.pull_request.head.sha }}  # ✅ Explicit PR head checkout
```

**Rationale:**
- `pull_request_target` gives PR code access to repository secrets
- Malicious PRs can exfiltrate secrets or modify the repository
- Should only be used when commenting on PRs from forks

**Recommendation:**
Use `pull_request` instead of `pull_request_target` unless you need to comment on PRs.

---

### CICD-007: Missing Concurrency Control

**Severity:** Low

**Description:**
Without concurrency control, multiple workflow runs can conflict (e.g., deploying simultaneously).

**Bad Example:**
```yaml
on:
  push:

jobs:
  deploy:  # ❌ Multiple pushes trigger parallel deploys
    runs-on: ubuntu-latest
```

**Good Example:**
```yaml
on:
  push:

concurrency:
  group: ${{ github.workflow }}-${{ github.ref }}  # ✅ One deployment per branch
  cancel-in-progress: true  # Cancel old runs

jobs:
  deploy:
    runs-on: ubuntu-latest
```

**Rationale:**
- Prevents race conditions in deployments
- Cancels outdated workflow runs
- Saves resources by skipping stale builds

---

### CICD-008: Dangerous Pipe to Shell

**Severity:** High

**Description:**
Piping curl/wget directly to shell allows Man-in-the-Middle attacks.

**Bad Example:**
```yaml
- run: curl https://example.com/install.sh | sh  # ❌ MITM vulnerable
- run: wget -O- https://example.com/script.sh | bash  # ❌ No verification
```

**Good Example:**
```yaml
- run: |
    curl -o install.sh https://example.com/install.sh
    echo "expected_sha256  install.sh" | sha256sum -c -  # ✅ Verify checksum
    chmod +x install.sh
    ./install.sh
```

**Rationale:**
- Allows verification before execution
- Prevents MITM injection of malicious code
- Provides audit trail of what was executed

---

### CICD-009: Unprotected Deployment Jobs

**Severity:** Medium

**Description:**
Deployment jobs should have conditional execution or environment protection.

**Bad Example:**
```yaml
jobs:
  deploy-production:
    runs-on: ubuntu-latest  # ❌ No conditions - runs on every commit
    steps:
      - run: kubectl apply -f production.yaml
```

**Good Example:**
```yaml
jobs:
  deploy-production:
    runs-on: ubuntu-latest
    if: github.ref == 'refs/heads/main'  # ✅ Only on main branch
    environment: production  # ✅ Requires approval
    steps:
      - run: kubectl apply -f production.yaml
```

**Rationale:**
- Prevents accidental production deployments
- Enforces approval workflows
- Provides audit trail

**SaaSForge Patterns:**
- Preview: `if: github.event_name == 'pull_request'`
- Staging: `if: github.ref == 'refs/heads/main'`
- Production: `if: github.event_name == 'release'`

---

### CICD-010: Missing continue-on-error for Non-Critical Jobs

**Severity:** Low

**Description:**
Linting, formatting, and security scans should warn without blocking builds.

**Bad Example:**
```yaml
jobs:
  lint:
    runs-on: ubuntu-latest  # ❌ Blocks build on linting failures
    steps:
      - run: npm run lint
```

**Good Example:**
```yaml
jobs:
  lint:
    runs-on: ubuntu-latest
    continue-on-error: true  # ✅ Warns but doesn't block
    steps:
      - run: npm run lint
```

**Rationale:**
- Allows iterative development
- Provides feedback without blocking
- Critical jobs (build, test) should still fail fast

---

## Docker Security

### CICD-021: Vulnerable Base Images

**Severity:** High

**Description:**
Using outdated or EOL base images exposes containers to known vulnerabilities.

**Vulnerable Images:**
- Ubuntu: 18.04, 16.04, 14.04
- Alpine: < 3.17
- Node.js: 10, 12, 14
- Python: 2.7, 3.6, 3.7
- Debian: jessie, stretch

**Bad Example:**
```dockerfile
FROM ubuntu:18.04  # ❌ EOL - no security updates
FROM alpine:3.10   # ❌ Vulnerable version
FROM node:14       # ❌ Unsupported
```

**Good Example:**
```dockerfile
FROM ubuntu:22.04  # ✅ LTS with support until 2027
FROM alpine:3.18   # ✅ Current stable
FROM node:20-alpine  # ✅ Supported + minimal
```

**Rationale:**
- EOL images don't receive security patches
- Reduces attack surface
- Compliance requirements (PCI-DSS, SOC 2)

---

### CICD-022: Running as Root

**Severity:** High

**Description:**
Containers running as root can escape to compromise the host.

**Bad Example:**
```dockerfile
FROM ubuntu:22.04
RUN apt-get update && apt-get install -y nginx
CMD ["nginx"]  # ❌ Runs as root (UID 0)
```

**Good Example:**
```dockerfile
FROM ubuntu:22.04
RUN apt-get update && apt-get install -y nginx && \
    useradd -r -s /bin/false appuser
USER appuser  # ✅ Runs as non-root
CMD ["nginx"]
```

**Rationale:**
- Limits container escape impact
- Follows least privilege principle
- Required by Kubernetes Pod Security Standards

---

### CICD-023: Hardcoded Secrets in Layers

**Severity:** Critical

**Description:**
Secrets in Docker layers are visible in image history.

**Bad Example:**
```dockerfile
RUN echo "API_KEY=sk-1234567890" >> /app/config  # ❌ Visible in layer
ENV DATABASE_PASSWORD=hunter2  # ❌ In image history
```

**Good Example:**
```dockerfile
# ✅ Use build arguments (not in final image)
ARG BUILD_TOKEN
RUN curl -H "Authorization: Bearer $BUILD_TOKEN" ...

# ✅ Or mount secrets (BuildKit)
RUN --mount=type=secret,id=api_key \
    curl -H "Authorization: Bearer $(cat /run/secrets/api_key)" ...
```

**Rationale:**
- Docker layers are immutable and stored in registries
- Anyone with image access can extract secrets
- Even deleted layers remain in image history

---

### CICD-024: Single-Stage Builds

**Severity:** Medium

**Description:**
Single-stage builds include build tools and source code in the final image.

**Bad Example:**
```dockerfile
FROM ubuntu:22.04
RUN apt-get install -y build-essential cmake git
COPY . /app
RUN cd /app && make  # ❌ Build tools in final image (1GB)
```

**Good Example:**
```dockerfile
# Builder stage
FROM ubuntu:22.04 AS builder
RUN apt-get install -y build-essential cmake
COPY . /app
RUN cd /app && make

# Runtime stage - ✅ Only binary, no build tools (100MB)
FROM ubuntu:22.04
COPY --from=builder /app/bin/myapp /usr/local/bin/
CMD ["myapp"]
```

**Rationale:**
- Smaller images (faster pulls, less storage)
- Reduced attack surface (no build tools)
- Faster deployments

**SaaSForge C++ Services:**
All services use multi-stage builds (auth, upload, payment, notification).

---

### CICD-025: Missing HEALTHCHECK

**Severity:** Low

**Description:**
Without health checks, orchestrators can't detect failed containers.

**Bad Example:**
```dockerfile
FROM node:20
COPY . /app
CMD ["node", "server.js"]  # ❌ No health check
```

**Good Example:**
```dockerfile
FROM node:20
COPY . /app
HEALTHCHECK --interval=30s --timeout=3s --retries=3 \
  CMD curl -f http://localhost:8000/health || exit 1  # ✅ Health check
CMD ["node", "server.js"]
```

**Rationale:**
- Enables Docker/Kubernetes to restart unhealthy containers
- Prevents routing traffic to failed instances
- Improves reliability

---

### CICD-026: Using :latest Tag

**Severity:** Medium

**Description:**
`:latest` tag is mutable and can break reproducibility.

**Bad Example:**
```dockerfile
FROM node:latest  # ❌ Undefined version - builds can break
```

**Good Example:**
```dockerfile
FROM node:20.10.0-alpine3.18  # ✅ Pinned version
# Or use digest for immutability
FROM node:20-alpine@sha256:abc123...  # ✅ Immutable
```

**Rationale:**
- Reproducible builds
- Prevents unexpected breakage from upstream updates
- Easier rollback when issues occur

---

### CICD-027: Missing --no-install-recommends

**Severity:** Low

**Description:**
Installing recommended packages bloats images.

**Bad Example:**
```dockerfile
RUN apt-get update && apt-get install -y curl  # ❌ Installs ~50MB of recommended packages
```

**Good Example:**
```dockerfile
RUN apt-get update && apt-get install -y --no-install-recommends curl  # ✅ Minimal install
```

**Rationale:**
- Smaller images (20-30% reduction)
- Faster builds
- Reduced attack surface

---

### CICD-028: Missing Package Manager Cleanup

**Severity:** Low

**Description:**
Package manager caches waste space in final image.

**Bad Example:**
```dockerfile
RUN apt-get update && \
    apt-get install -y curl  # ❌ Leaves /var/lib/apt/lists/ cache
```

**Good Example:**
```dockerfile
RUN apt-get update && \
    apt-get install -y curl && \
    rm -rf /var/lib/apt/lists/*  # ✅ Cleanup in same layer
```

**Rationale:**
- Reduces image size by 10-20MB
- Cleanup in same RUN statement prevents cache from being committed to layer

---

### CICD-029: COPY Before Dependency Installation

**Severity:** Medium

**Description:**
Copying source code before installing dependencies breaks Docker layer cache.

**Bad Example:**
```dockerfile
COPY . /app  # ❌ Invalidates cache on any code change
RUN npm install  # Reinstalls dependencies every time
```

**Good Example:**
```dockerfile
COPY package*.json /app/  # ✅ Copy dependency files first
RUN npm ci  # Cached unless package.json changes
COPY . /app  # Copy source after dependencies
```

**Rationale:**
- Dramatically faster builds (30-90% time savings)
- Only reinstalls dependencies when they change
- Standard Docker best practice

---

### CICD-030: Privileged Containers

**Severity:** Critical

**Description:**
Privileged mode gives containers full access to the host.

**Bad Example:**
```dockerfile
# In docker run command or compose
docker run --privileged myimage  # ❌ Full host access
```

**Good Example:**
```dockerfile
# No privileged flag - ✅ Runs with limited capabilities
docker run myimage

# If specific capabilities needed, grant only those
docker run --cap-add=NET_ADMIN myimage  # ✅ Minimal privileges
```

**Rationale:**
- Prevents container escape
- Limits damage from compromised containers
- Violates principle of least privilege

---

## Kubernetes Security

### CICD-011: Missing Resource Limits

**Severity:** Medium

**Description:**
Containers without resource limits can starve other pods.

**Bad Example:**
```yaml
spec:
  containers:
  - name: api
    image: myapp:latest  # ❌ No limits - can consume all node resources
```

**Good Example:**
```yaml
spec:
  containers:
  - name: api
    image: myapp:latest
    resources:
      requests:  # ✅ Guaranteed resources
        cpu: 100m
        memory: 128Mi
      limits:  # ✅ Maximum allowed
        cpu: 500m
        memory: 512Mi
```

**Rationale:**
- Prevents OOMKilled and CPU throttling
- Enables Kubernetes autoscaling
- Protects other workloads from resource starvation

**SaaSForge Guidelines:**
- **C++ services:** 200m CPU / 256Mi RAM (request), 1000m / 1Gi (limit)
- **FastAPI:** 300m / 512Mi (request), 2000m / 2Gi (limit)
- **React UI:** 100m / 128Mi (request), 500m / 512Mi (limit)

---

### CICD-012: Missing Health Probes

**Severity:** High

**Description:**
Pods without health probes can remain in broken state indefinitely.

**Bad Example:**
```yaml
spec:
  containers:
  - name: api
    image: myapp:latest  # ❌ No probes - K8s can't detect failures
```

**Good Example:**
```yaml
spec:
  containers:
  - name: api
    image: myapp:latest
    livenessProbe:  # ✅ Restart if unhealthy
      httpGet:
        path: /health
        port: 8000
      initialDelaySeconds: 30
      periodSeconds: 10
    readinessProbe:  # ✅ Don't route traffic until ready
      httpGet:
        path: /health
        port: 8000
      initialDelaySeconds: 5
      periodSeconds: 5
```

**Rationale:**
- Automatic recovery from failures
- Prevents routing traffic to unhealthy pods
- Required for zero-downtime deployments

**Probe Types:**
- **Liveness:** Restart unhealthy containers
- **Readiness:** Stop routing traffic to unready containers
- **Startup:** Allow slow-starting containers more time

---

### CICD-013: Privileged Containers

**Severity:** Critical

**Description:**
Privileged pods can access host resources and escape the container.

**Bad Example:**
```yaml
spec:
  containers:
  - name: api
    securityContext:
      privileged: true  # ❌ Full host access
```

**Good Example:**
```yaml
spec:
  containers:
  - name: api
    securityContext:
      privileged: false  # ✅ Default - no privileged access
      allowPrivilegeEscalation: false
      capabilities:
        drop: ["ALL"]  # Drop all capabilities
```

**Rationale:**
- Prevents container escape
- Limits blast radius of compromised pods
- Required by Pod Security Standards (Restricted)

---

### CICD-014: Missing runAsNonRoot

**Severity:** Medium

**Description:**
Pods running as root violate least privilege principle.

**Bad Example:**
```yaml
spec:
  containers:
  - name: api
    image: myapp:latest  # ❌ Runs as root by default
```

**Good Example:**
```yaml
spec:
  securityContext:
    runAsNonRoot: true  # ✅ Enforces non-root execution
    runAsUser: 1000
    fsGroup: 1000
  containers:
  - name: api
    image: myapp:latest
```

**Rationale:**
- Limits damage from container escape
- Enforces Dockerfile USER directive
- Compliance requirement (PCI-DSS, SOC 2)

---

### CICD-015: Missing Tenant Isolation Labels

**Severity:** Low

**Description:**
Multi-tenant SaaS applications should label resources by tenant.

**Bad Example:**
```yaml
metadata:
  name: api-deployment  # ❌ No tenant labels
```

**Good Example:**
```yaml
metadata:
  name: api-deployment
  labels:
    app: api
    tenant-id: "all"  # ✅ Tenant label for network policies
```

**Rationale:**
- Enables tenant-specific network policies
- Facilitates monitoring and cost allocation
- Supports future tenant-per-namespace architecture

---

### CICD-016: Hardcoded Configuration Values

**Severity:** High

**Description:**
Secrets and config should use ConfigMaps/Secrets, not environment variables.

**Bad Example:**
```yaml
env:
- name: DATABASE_URL
  value: "postgresql://user:password@host/db"  # ❌ Hardcoded
```

**Good Example:**
```yaml
env:
- name: DATABASE_URL
  valueFrom:
    secretKeyRef:  # ✅ From Secret
      name: db-credentials
      key: url
```

**Rationale:**
- Secrets stored encrypted in etcd
- Can rotate secrets without redeploying
- Prevents credentials in manifest files

---

### CICD-017: Missing Namespace

**Severity:** Medium

**Description:**
Resources without namespaces deploy to `default`, causing conflicts.

**Bad Example:**
```yaml
metadata:
  name: api-deployment  # ❌ No namespace - uses default
```

**Good Example:**
```yaml
metadata:
  name: api-deployment
  namespace: saasforge-staging  # ✅ Explicit namespace
```

**Rationale:**
- Enables multi-tenant/multi-environment clusters
- RBAC scoped to namespaces
- Resource quotas per namespace

---

### CICD-018: hostPath Volumes

**Severity:** High

**Description:**
hostPath mounts give pods access to host filesystem.

**Bad Example:**
```yaml
volumes:
- name: data
  hostPath:  # ❌ Mounts host filesystem
    path: /var/lib/myapp
```

**Good Example:**
```yaml
volumes:
- name: data
  persistentVolumeClaim:  # ✅ Use PVCs instead
    claimName: myapp-data
```

**Rationale:**
- Prevents pods from accessing host secrets/configs
- hostPath ties pod to specific node
- Use PVCs for portable storage

---

### CICD-019: Missing PodDisruptionBudget

**Severity:** Low

**Description:**
Multi-replica deployments should have PDBs to prevent all pods being evicted.

**Bad Example:**
```yaml
spec:
  replicas: 3  # ❌ No PDB - all 3 can be evicted during node drain
```

**Good Example:**
```yaml
# Separate PDB resource
apiVersion: policy/v1
kind: PodDisruptionBudget
metadata:
  name: api-pdb
spec:
  minAvailable: 1  # ✅ At least 1 pod always running
  selector:
    matchLabels:
      app: api
```

**Rationale:**
- Prevents service outages during node maintenance
- Ensures high availability
- Required for production deployments

---

### CICD-020: Missing imagePullPolicy

**Severity:** Low

**Description:**
Without explicit imagePullPolicy, Kubernetes may use cached images.

**Bad Example:**
```yaml
spec:
  containers:
  - name: api
    image: myapp:latest  # ❌ No pull policy - may use stale cache
```

**Good Example:**
```yaml
spec:
  containers:
  - name: api
    image: myapp:v1.2.3
    imagePullPolicy: IfNotPresent  # ✅ Explicit policy

  # For :latest or testing
  - name: debug
    image: myapp:latest
    imagePullPolicy: Always  # ✅ Always pull for :latest
```

**Rationale:**
- Ensures correct image version is deployed
- Prevents stale cache issues
- Explicit is better than implicit

**Recommended Policies:**
- Production: `IfNotPresent` with versioned tags
- Development: `Always` with :latest tag

---

## Docker Compose Best Practices

### CICD-031 to CICD-034

See implementation in review.py for Docker Compose checks covering privileged mode, hardcoded secrets, health checks, and image tags.

---

## Kustomize Configuration

### CICD-035 to CICD-037

See implementation in review.py for Kustomize checks covering namespace specification, common labels, and image digest usage.

---

## References

- [GitHub Actions Security Hardening](https://docs.github.com/en/actions/security-guides/security-hardening-for-github-actions)
- [Docker Security Best Practices](https://docs.docker.com/develop/security-best-practices/)
- [Kubernetes Pod Security Standards](https://kubernetes.io/docs/concepts/security/pod-security-standards/)
- [OWASP Docker Security Cheat Sheet](https://cheatsheetseries.owasp.org/cheatsheets/Docker_Security_Cheat_Sheet.html)

---

**Document Version:** 1.0
**Maintained By:** Security & DevOps Teams
**Questions?** Open an issue or contact via Slack #security
