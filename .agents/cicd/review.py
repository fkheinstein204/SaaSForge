#!/usr/bin/env python3
"""
CI/CD Review Agent
Validates GitHub Actions workflows, Kubernetes manifests, Dockerfiles, and deployment configurations
"""
import re
import sys
import os
from pathlib import Path
from typing import List, Tuple
import yaml


class CICDReviewAgent:
    def __init__(self, config_path=".agents/config.yaml"):
        with open(config_path) as f:
            config = yaml.safe_load(f)
            self.config = config.get('cicd', {
                'enabled': True,
                'severity_threshold': 'medium',
                'fail_on_critical': True
            })
        self.issues = []

        # Vulnerable base images
        self.vulnerable_images = [
            'ubuntu:18.04', 'ubuntu:16.04', 'ubuntu:14.04',
            'alpine:3.10', 'alpine:3.11', 'alpine:3.12', 'alpine:3.13', 'alpine:3.14', 'alpine:3.15', 'alpine:3.16',
            'node:10', 'node:12', 'node:14',
            'python:2.7', 'python:3.6', 'python:3.7',
            'debian:jessie', 'debian:stretch'
        ]

        # Trusted action publishers
        self.trusted_publishers = ['actions', 'github', 'docker', 'aws-actions', 'azure', 'google-github-actions']

    def review_file(self, file_path: str) -> List[Tuple[str, str, int]]:
        """Review a single file for CI/CD issues"""
        if not os.path.exists(file_path):
            print(f"Error: File not found: {file_path}")
            return []

        with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()
            lines = content.split('\n')

        file_name = Path(file_path).name
        ext = Path(file_path).suffix

        # GitHub Actions workflows
        if '.github/workflows' in file_path and ext in ['.yml', '.yaml']:
            self._review_github_workflow(content, lines, file_path)

        # Dockerfiles
        elif 'Dockerfile' in file_name or file_name.endswith('.Dockerfile'):
            self._review_dockerfile(content, lines, file_path)

        # Kubernetes manifests
        elif ext in ['.yml', '.yaml'] and ('k8s/' in file_path or 'kubernetes/' in file_path):
            self._review_kubernetes(content, lines, file_path)

        # Docker Compose
        elif 'docker-compose' in file_name:
            self._review_docker_compose(content, lines, file_path)

        # Kustomize
        elif file_name == 'kustomization.yaml':
            self._review_kustomize(content, lines, file_path)

        return self.issues

    def _review_github_workflow(self, content: str, lines: List[str], file_path: str):
        """Review GitHub Actions workflow files"""

        try:
            workflow = yaml.safe_load(content)
        except yaml.YAMLError:
            self._add_issue('high', 'Invalid YAML syntax in workflow', file_path, 0)
            return

        # CICD-001: Check for overly permissive workflow permissions
        if 'permissions' in workflow:
            if workflow['permissions'] == 'write-all':
                self._add_issue('critical', 'CICD-001: Workflow has write-all permissions (use least privilege)', file_path, 0)
        else:
            # Check job-level permissions
            jobs = workflow.get('jobs', {})
            for job_name, job in jobs.items():
                if job.get('permissions') == 'write-all':
                    self._add_issue('critical', f'CICD-001: Job "{job_name}" has write-all permissions', file_path, 0)

        # CICD-002: Check for unpinned third-party actions
        for i, line in enumerate(lines, 1):
            uses_match = re.search(r'uses:\s*([^\s@]+)@([^\s]+)', line)
            if uses_match:
                action = uses_match.group(1)
                version = uses_match.group(2)
                publisher = action.split('/')[0]

                # Skip trusted publishers
                if publisher not in self.trusted_publishers:
                    # Check if pinned to SHA256
                    if not re.match(r'^[a-f0-9]{64}$', version):
                        self._add_issue('high', f'CICD-002: Unpinned third-party action "{action}" (use @sha256 instead of @{version})', file_path, i)

        # CICD-003: Check for secrets in workflow files
        for i, line in enumerate(lines, 1):
            if re.search(r'(password|token|secret|api[_-]?key)\s*:\s*["\']?[a-zA-Z0-9]{20,}["\']?', line, re.IGNORECASE):
                if '${{' not in line and 'secrets.' not in line:
                    self._add_issue('critical', 'CICD-003: Hardcoded secret detected in workflow', file_path, i)

        # CICD-004: Check for missing timeout values
        jobs = workflow.get('jobs', {})
        for job_name, job in jobs.items():
            if 'timeout-minutes' not in job:
                self._add_issue('medium', f'CICD-004: Job "{job_name}" missing timeout-minutes (can run indefinitely)', file_path, 0)

        # CICD-005: Check self-hosted runner security for PRs
        for job_name, job in jobs.items():
            runs_on = job.get('runs-on', '')
            if isinstance(runs_on, str) and 'self-hosted' in runs_on:
                on_events = workflow.get('on', {})
                if 'pull_request' in on_events or 'pull_request_target' in on_events:
                    self._add_issue('critical', f'CICD-005: Job "{job_name}" uses self-hosted runner for PRs (security risk from untrusted code)', file_path, 0)

        # CICD-006: Check for pull_request_target without checkout safety
        if 'pull_request_target' in str(workflow.get('on', {})):
            checkout_found = False
            for i, line in enumerate(lines, 1):
                if 'actions/checkout' in line:
                    # Check if ref is set to PR head
                    next_10_lines = lines[i:i+10]
                    if any('ref:' in l and 'github.event.pull_request.head.sha' in l for l in next_10_lines):
                        checkout_found = True

            if not checkout_found:
                self._add_issue('critical', 'CICD-006: pull_request_target without safe checkout (ref: github.event.pull_request.head.sha)', file_path, 0)

        # CICD-007: Check for missing concurrency control
        if 'concurrency' not in workflow:
            self._add_issue('low', 'CICD-007: Missing concurrency control (parallel runs may conflict)', file_path, 0)

        # CICD-008: Check for dangerous commands (curl | sh)
        for i, line in enumerate(lines, 1):
            if re.search(r'curl.*\|.*sh|wget.*\|.*sh|curl.*\|.*bash', line):
                self._add_issue('high', 'CICD-008: Dangerous pipe to shell detected (curl | sh)', file_path, i)

        # CICD-009: Check for missing if conditions on deployment jobs
        for job_name, job in jobs.items():
            if 'deploy' in job_name.lower() or 'push' in str(job.get('steps', [])).lower():
                if 'if' not in job and 'environment' not in job:
                    self._add_issue('medium', f'CICD-009: Deployment job "{job_name}" missing conditional execution or environment protection', file_path, 0)

        # CICD-010: Check for missing continue-on-error for non-critical jobs
        for job_name, job in jobs.items():
            if any(keyword in job_name.lower() for keyword in ['lint', 'format', 'scan']):
                if 'continue-on-error' not in job:
                    self._add_issue('low', f'CICD-010: Non-critical job "{job_name}" should have continue-on-error: true', file_path, 0)

    def _review_dockerfile(self, content: str, lines: List[str], file_path: str):
        """Review Dockerfile for security and best practices"""

        # CICD-021: Check for vulnerable base images
        for i, line in enumerate(lines, 1):
            if line.strip().startswith('FROM'):
                for vulnerable in self.vulnerable_images:
                    if vulnerable in line:
                        self._add_issue('high', f'CICD-021: Vulnerable base image "{vulnerable}" detected', file_path, i)

        # CICD-022: Check for missing USER directive (running as root)
        has_user = any('USER' in line for line in lines if line.strip() and not line.strip().startswith('#'))
        if not has_user:
            self._add_issue('high', 'CICD-022: Dockerfile missing USER directive (will run as root)', file_path, 0)

        # CICD-023: Check for exposed secrets in layers
        for i, line in enumerate(lines, 1):
            if re.search(r'(password|secret|token|api[_-]?key)\s*=\s*["\'][^"\']+["\']', line, re.IGNORECASE):
                if 'ARG' not in line and 'example' not in line.lower():
                    self._add_issue('critical', 'CICD-023: Hardcoded secret in Dockerfile layer', file_path, i)

        # CICD-024: Check for missing multi-stage build
        from_count = sum(1 for line in lines if line.strip().startswith('FROM'))
        if from_count == 1:
            self._add_issue('medium', 'CICD-024: Single-stage build detected (consider multi-stage for smaller images)', file_path, 0)

        # CICD-025: Check for missing HEALTHCHECK
        has_healthcheck = any('HEALTHCHECK' in line for line in lines)
        if not has_healthcheck:
            self._add_issue('low', 'CICD-025: Missing HEALTHCHECK instruction', file_path, 0)

        # CICD-026: Check for using latest tag
        for i, line in enumerate(lines, 1):
            if line.strip().startswith('FROM') and ':latest' in line:
                self._add_issue('medium', 'CICD-026: Using :latest tag (use specific version for reproducibility)', file_path, i)

        # CICD-027: Check for apt-get install without --no-install-recommends
        for i, line in enumerate(lines, 1):
            if 'apt-get install' in line and '--no-install-recommends' not in line:
                self._add_issue('low', 'CICD-027: apt-get install without --no-install-recommends (bloated images)', file_path, i)

        # CICD-028: Check for missing layer cleanup
        for i, line in enumerate(lines, 1):
            if 'apt-get update' in line or 'apk add' in line:
                # Check if cleanup is in same RUN or next few lines
                next_5_lines = lines[i:i+5]
                has_cleanup = any('rm -rf' in l or 'clean' in l for l in next_5_lines)
                if not has_cleanup:
                    self._add_issue('low', 'CICD-028: Package manager cache not cleaned up', file_path, i)

        # CICD-029: Check for COPY before dependency installation (cache busting)
        copy_found = False
        install_found = False
        for line in lines:
            if line.strip().startswith('COPY') and not 'package.json' in line and not 'requirements.txt' in line:
                copy_found = True
            if any(cmd in line for cmd in ['apt-get install', 'npm install', 'pip install', 'apk add']):
                install_found = True
                if copy_found:
                    self._add_issue('medium', 'CICD-029: COPY before dependency installation (breaks layer cache)', file_path, 0)
                    break

        # CICD-030: Check for privileged or dangerous capabilities
        for i, line in enumerate(lines, 1):
            if '--privileged' in line or '--cap-add=ALL' in line:
                self._add_issue('critical', 'CICD-030: Privileged flag or ALL capabilities detected', file_path, i)

    def _review_kubernetes(self, content: str, lines: List[str], file_path: str):
        """Review Kubernetes manifest files"""

        try:
            manifests = list(yaml.safe_load_all(content))
        except yaml.YAMLError:
            self._add_issue('high', 'Invalid YAML syntax in Kubernetes manifest', file_path, 0)
            return

        for manifest in manifests:
            if not manifest:
                continue

            kind = manifest.get('kind', '')
            metadata = manifest.get('metadata', {})
            spec = manifest.get('spec', {})

            # CICD-011: Check for missing resource limits
            if kind in ['Deployment', 'StatefulSet', 'DaemonSet', 'Pod']:
                containers = spec.get('template', {}).get('spec', {}).get('containers', [])
                for container in containers:
                    resources = container.get('resources', {})
                    if 'limits' not in resources:
                        self._add_issue('medium', f'CICD-011: Container "{container.get("name")}" missing resource limits', file_path, 0)
                    if 'requests' not in resources:
                        self._add_issue('medium', f'CICD-011: Container "{container.get("name")}" missing resource requests', file_path, 0)

            # CICD-012: Check for missing health probes
            if kind in ['Deployment', 'StatefulSet', 'DaemonSet', 'Pod']:
                containers = spec.get('template', {}).get('spec', {}).get('containers', [])
                for container in containers:
                    if 'livenessProbe' not in container:
                        self._add_issue('high', f'CICD-012: Container "{container.get("name")}" missing livenessProbe', file_path, 0)
                    if 'readinessProbe' not in container:
                        self._add_issue('high', f'CICD-012: Container "{container.get("name")}" missing readinessProbe', file_path, 0)

            # CICD-013: Check for privileged containers
            if kind in ['Deployment', 'StatefulSet', 'DaemonSet', 'Pod']:
                containers = spec.get('template', {}).get('spec', {}).get('containers', [])
                for container in containers:
                    security_context = container.get('securityContext', {})
                    if security_context.get('privileged') == True:
                        self._add_issue('critical', f'CICD-013: Container "{container.get("name")}" running in privileged mode', file_path, 0)

            # CICD-014: Check for missing security context (runAsNonRoot)
            if kind in ['Deployment', 'StatefulSet', 'DaemonSet', 'Pod']:
                pod_security = spec.get('template', {}).get('spec', {}).get('securityContext', {})
                if not pod_security.get('runAsNonRoot'):
                    self._add_issue('medium', 'CICD-014: Missing runAsNonRoot security context', file_path, 0)

            # CICD-015: Check for missing tenant isolation labels
            if kind in ['Deployment', 'StatefulSet', 'Service']:
                labels = metadata.get('labels', {})
                if 'tenant-id' not in labels and 'tenantId' not in labels:
                    if 'saasforge' in file_path:  # Only for SaaS app, not infra
                        self._add_issue('low', 'CICD-015: Missing tenant-id label for multi-tenancy', file_path, 0)

            # CICD-016: Check for hardcoded values instead of ConfigMaps
            for i, line in enumerate(lines, 1):
                if re.search(r'(DATABASE_URL|REDIS_URL|API_KEY)\s*:\s*["\'][^"\']+["\']', line):
                    if 'secretKeyRef' not in line and 'configMapKeyRef' not in line:
                        self._add_issue('high', 'CICD-016: Hardcoded configuration value (use ConfigMap/Secret)', file_path, i)

            # CICD-017: Check for missing namespace
            if kind not in ['Namespace', 'ClusterRole', 'ClusterRoleBinding']:
                if 'namespace' not in metadata:
                    self._add_issue('medium', 'CICD-017: Resource missing namespace specification', file_path, 0)

            # CICD-018: Check for hostPath volumes (security risk)
            if kind in ['Deployment', 'StatefulSet', 'DaemonSet', 'Pod']:
                volumes = spec.get('template', {}).get('spec', {}).get('volumes', [])
                for volume in volumes:
                    if 'hostPath' in volume:
                        self._add_issue('high', f'CICD-018: hostPath volume "{volume.get("name")}" detected (security risk)', file_path, 0)

            # CICD-019: Check for missing pod disruption budget for critical services
            if kind == 'Deployment':
                replicas = spec.get('replicas', 1)
                if replicas > 1:
                    # Note: PDB would be in separate file, this is just a reminder
                    self._add_issue('low', 'CICD-019: Multi-replica deployment should have PodDisruptionBudget', file_path, 0)

            # CICD-020: Check for missing image pull policy
            if kind in ['Deployment', 'StatefulSet', 'DaemonSet', 'Pod']:
                containers = spec.get('template', {}).get('spec', {}).get('containers', [])
                for container in containers:
                    if 'imagePullPolicy' not in container:
                        self._add_issue('low', f'CICD-020: Container "{container.get("name")}" missing imagePullPolicy', file_path, 0)

    def _review_docker_compose(self, content: str, lines: List[str], file_path: str):
        """Review docker-compose.yml files"""

        try:
            compose = yaml.safe_load(content)
        except yaml.YAMLError:
            self._add_issue('high', 'Invalid YAML syntax in docker-compose', file_path, 0)
            return

        services = compose.get('services', {})

        for service_name, service in services.items():
            # CICD-031: Check for privileged mode
            if service.get('privileged') == True:
                self._add_issue('critical', f'CICD-031: Service "{service_name}" running in privileged mode', file_path, 0)

            # CICD-032: Check for hardcoded secrets in environment
            env = service.get('environment', [])
            if isinstance(env, list):
                for i, var in enumerate(env):
                    if '=' in str(var) and re.search(r'(password|secret|token|key)', str(var), re.IGNORECASE):
                        if '${' not in str(var):
                            self._add_issue('high', f'CICD-032: Service "{service_name}" has hardcoded secret in environment', file_path, 0)

            # CICD-033: Check for missing health checks
            if 'healthcheck' not in service:
                self._add_issue('medium', f'CICD-033: Service "{service_name}" missing healthcheck', file_path, 0)

            # CICD-034: Check for using latest tag
            image = service.get('image', '')
            if ':latest' in image or ':' not in image:
                self._add_issue('medium', f'CICD-034: Service "{service_name}" using latest tag or no tag', file_path, 0)

    def _review_kustomize(self, content: str, lines: List[str], file_path: str):
        """Review Kustomize configuration files"""

        try:
            kustomize = yaml.safe_load(content)
        except yaml.YAMLError:
            self._add_issue('high', 'Invalid YAML syntax in kustomization', file_path, 0)
            return

        # CICD-035: Check for missing namespace
        if 'namespace' not in kustomize:
            self._add_issue('medium', 'CICD-035: Kustomization missing namespace specification', file_path, 0)

        # CICD-036: Check for missing common labels
        if 'commonLabels' not in kustomize:
            self._add_issue('low', 'CICD-036: Kustomization missing commonLabels', file_path, 0)

        # CICD-037: Check for image tag mutations without digest
        images = kustomize.get('images', [])
        for image in images:
            if 'digest' not in image and 'newTag' in image:
                if not re.match(r'^[a-f0-9]{64}$', image.get('newTag', '')):
                    self._add_issue('medium', f'CICD-037: Image "{image.get("name")}" using tag instead of digest', file_path, 0)

    def _add_issue(self, severity: str, message: str, file_path: str, line: int):
        """Add an issue to the report"""
        self.issues.append((severity, message, f"{file_path}:{line}"))

    def report(self):
        """Generate final report"""
        if not self.issues:
            print("✅ No CI/CD issues found!")
            return 0

        severity_icons = {
            'critical': '🔴',
            'high': '🟠',
            'medium': '🟡',
            'low': '🟢'
        }

        severity_order = {'critical': 0, 'high': 1, 'medium': 2, 'low': 3}
        sorted_issues = sorted(self.issues, key=lambda x: severity_order[x[0]])

        print("\n" + "="*80)
        print("CI/CD REVIEW REPORT")
        print("="*80 + "\n")

        counts = {'critical': 0, 'high': 0, 'medium': 0, 'low': 0}
        for severity, message, location in sorted_issues:
            counts[severity] += 1
            icon = severity_icons[severity]
            print(f"{icon} [{severity.upper()}] {message}")
            print(f"   Location: {location}\n")

        print("="*80)
        print(f"Summary: {counts['critical']} critical, {counts['high']} high, "
              f"{counts['medium']} medium, {counts['low']} low")
        print("="*80 + "\n")

        # Determine exit code
        if self.config.get('fail_on_critical', True):
            if counts['critical'] > 0 or counts['high'] > 0:
                print("❌ CI/CD review failed (critical or high severity issues found)")
                return 1

        print("✅ CI/CD review passed (warnings only)")
        return 0


def main():
    if len(sys.argv) < 2:
        print("Usage: python review.py <directory>")
        sys.exit(1)

    root_dir = sys.argv[1]
    agent = CICDReviewAgent()

    # Find relevant files
    ci_files = []
    for root, dirs, files in os.walk(root_dir):
        # Skip hidden directories except .github
        dirs[:] = [d for d in dirs if d == '.github' or not d.startswith('.')]

        for file in files:
            file_path = os.path.join(root, file)

            # GitHub Actions workflows
            if '.github/workflows' in file_path and file.endswith(('.yml', '.yaml')):
                ci_files.append(file_path)

            # Dockerfiles
            elif 'Dockerfile' in file or file.endswith('.Dockerfile'):
                ci_files.append(file_path)

            # Kubernetes manifests
            elif file.endswith(('.yml', '.yaml')) and ('k8s/' in file_path or 'kubernetes/' in file_path):
                ci_files.append(file_path)

            # Docker Compose
            elif 'docker-compose' in file:
                ci_files.append(file_path)

            # Kustomize
            elif file == 'kustomization.yaml':
                ci_files.append(file_path)

    if not ci_files:
        print("No CI/CD files found to review")
        sys.exit(0)

    print(f"Found {len(ci_files)} CI/CD files to review\n")

    for file_path in ci_files:
        agent.review_file(file_path)

    exit_code = agent.report()
    sys.exit(exit_code)


if __name__ == '__main__':
    main()
