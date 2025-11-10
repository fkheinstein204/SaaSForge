#!/usr/bin/env python3
"""
Architecture Compliance Agent
Validates adherence to SRS requirements and design patterns
"""
import re
import sys
import os
from pathlib import Path
from typing import List, Dict
import yaml


class ArchitectureReviewAgent:
    def __init__(self, config_path=".agents/config.yaml"):
        with open(config_path) as f:
            self.config = yaml.safe_load(f)['architecture']
        self.issues = []
        self.requirements = self._load_requirements()

    def _load_requirements(self) -> Dict[str, str]:
        """Load requirements from SRS document"""
        # Simplified requirement mapping
        return {
            'A-': 'Authentication & Authorization',
            'B-': 'Upload & File Processing',
            'C-': 'Payment & Subscriptions',
            'D-': 'Notifications & Communication',
            'E-': 'Email & Transaction Management'
        }

    def review_file(self, file_path: str):
        """Review file for architecture compliance"""
        with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()
            lines = content.split('\n')

        ext = Path(file_path).suffix

        # Check requirement traceability
        self._check_requirement_references(content, file_path)

        # Check error code format
        self._check_error_codes(lines, file_path)

        # Check idempotency key usage
        if 'payment' in file_path.lower() or 'subscription' in file_path.lower():
            self._check_idempotency(content, lines, file_path)

        # Check multi-tenancy patterns
        self._check_multi_tenancy(content, lines, file_path)

    def _check_requirement_references(self, content: str, file_path: str):
        """Check if code references SRS requirements"""
        # Look for requirement references in comments (e.g., Requirement A-22)
        req_pattern = r'(?:Requirement|Req\.?)\s+([A-E]-\d+)'
        matches = re.findall(req_pattern, content, re.IGNORECASE)

        if not matches and 'test' not in file_path.lower():
            if any(keyword in file_path.lower() for keyword in ['service', 'router', 'controller']):
                self._add_issue('low', 'Missing requirement traceability comments', file_path, 0)

    def _check_error_codes(self, lines: List[str], file_path: str):
        """Validate error code format (AUTH_001, UP_001, etc.)"""
        error_pattern = r'["\']([A-Z_]+_\d{3})["\']'

        for i, line in enumerate(lines, 1):
            if 'error' in line.lower() or 'exception' in line.lower():
                matches = re.findall(error_pattern, line)
                for code in matches:
                    # Validate format
                    valid_prefixes = ['AUTH_', 'UP_', 'PAY_', 'NOTIF_', 'EMAIL_']
                    if not any(code.startswith(prefix) for prefix in valid_prefixes):
                        self._add_issue('medium', f'Invalid error code format: {code}', file_path, i)

    def _check_idempotency(self, content: str, lines: List[str], file_path: str):
        """Check idempotency key implementation"""
        if 'def create' in content or 'def process' in content or 'async def create' in content:
            if 'idempotency' not in content.lower():
                self._add_issue('high', 'Payment endpoint missing idempotency key handling', file_path, 0)

            # Check for Redis caching of idempotency results
            if 'idempotency' in content.lower() and 'redis' not in content.lower():
                self._add_issue('medium', 'Idempotency implementation missing Redis cache', file_path, 0)

    def _check_multi_tenancy(self, content: str, lines: List[str], file_path: str):
        """Validate multi-tenancy patterns"""
        # Check for tenant context extraction
        if 'grpc' in content.lower() or 'request' in content.lower():
            if 'tenant_id' in content.lower():
                # Good - tenant_id is being used

                # Check if it's being validated
                for i, line in enumerate(lines, 1):
                    if 'tenant_id' in line.lower():
                        # Look for validation in surrounding lines
                        context = '\n'.join(lines[max(0, i-5):min(len(lines), i+5)])
                        if 'if' not in context and 'check' not in context.lower() and 'validate' not in context.lower():
                            self._add_issue('medium', 'tenant_id extracted but not validated', file_path, i)
                            break

        # Check for client-provided tenant_id (security issue)
        for i, line in enumerate(lines, 1):
            if 'request.' in line and 'tenant_id' in line.lower():
                if 'body' in line or 'json' in line or 'form' in line:
                    self._add_issue('critical', 'tenant_id from request body (security risk - use JWT)', file_path, i)

    def _add_issue(self, severity: str, message: str, file_path: str, line: int):
        """Add an architecture issue"""
        self.issues.append((severity, message, f"{file_path}:{line}"))

    def report(self):
        """Generate architecture compliance report"""
        if not self.issues:
            print("✅ No architecture compliance issues found!")
            return 0

        print(f"\n🏗️  Architecture Compliance Report")
        print("=" * 80)

        for severity, message, location in self.issues:
            icon = {'critical': '🔴', 'high': '🟠', 'medium': '🟡', 'low': '🟢'}[severity]
            print(f"{icon} [{severity.upper()}] {message}")
            print(f"   Location: {location}")
            print()

        print(f"Total: {len(self.issues)} issues")
        return 1 if any(s[0] in ['critical', 'high'] for s in self.issues) else 0


def main():
    if len(sys.argv) < 2:
        print("Usage: python architecture/review.py <file_or_directory>")
        sys.exit(1)

    path = sys.argv[1]
    agent = ArchitectureReviewAgent()

    if os.path.isfile(path):
        agent.review_file(path)
    elif os.path.isdir(path):
        for root, dirs, files in os.walk(path):
            dirs[:] = [d for d in dirs if d not in ['build', 'node_modules', '__pycache__']]
            for file in files:
                if file.endswith(('.cpp', '.h', '.py', '.ts', '.tsx')):
                    agent.review_file(os.path.join(root, file))

    sys.exit(agent.report())


if __name__ == "__main__":
    main()
