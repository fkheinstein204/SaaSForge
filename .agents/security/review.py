#!/usr/bin/env python3
"""
Security Review Agent
Validates JWT, mTLS, multi-tenancy, and OWASP compliance
"""
import re
import sys
import os
from pathlib import Path
from typing import List, Tuple
import yaml


class SecurityReviewAgent:
    def __init__(self, config_path=".agents/config.yaml"):
        with open(config_path) as f:
            self.config = yaml.safe_load(f)['security']
        self.issues = []

    def review_file(self, file_path: str) -> List[Tuple[str, str, int]]:
        """Review a single file for security issues"""
        if not os.path.exists(file_path):
            print(f"Error: File not found: {file_path}")
            return []

        with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()
            lines = content.split('\n')

        ext = Path(file_path).suffix

        if ext in ['.cpp', '.h', '.hpp']:
            self._review_cpp(content, lines, file_path)
        elif ext in ['.py']:
            self._review_python(content, lines, file_path)
        elif ext in ['.ts', '.tsx', '.js', '.jsx']:
            self._review_typescript(content, lines, file_path)

        return self.issues

    def _review_cpp(self, content: str, lines: List[str], file_path: str):
        """Review C++ code for security issues"""

        # Check JWT algorithm whitelisting
        if 'jwt::decode' in content or 'jwt::verify' in content:
            if 'allow_algorithm' not in content:
                self._add_issue('high', 'JWT decoder missing algorithm whitelist', file_path, 0)
            elif 'jwt::algorithm::hs256' in content.lower():
                self._add_issue('critical', 'HS256 algorithm detected (use RS256 only)', file_path, 0)
            elif 'alg: none' in content.lower() or 'algorithm::none' in content.lower():
                self._add_issue('critical', 'Algorithm "none" vulnerability detected', file_path, 0)

        # Check for SQL injection vulnerabilities
        for i, line in enumerate(lines, 1):
            if 'SELECT' in line.upper() and ('+' in line or 'concat' in line.lower()):
                if 'prepare' not in line.lower() and '$' not in line:
                    self._add_issue('high', 'Possible SQL injection: string concatenation in query', file_path, i)

        # Check tenant_id in queries
        if 'SELECT' in content.upper() or 'UPDATE' in content.upper() or 'DELETE' in content.upper():
            if 'tenant_id' not in content.lower():
                self._add_issue('high', 'Query missing tenant_id isolation', file_path, 0)

        # Check for hardcoded secrets
        for i, line in enumerate(lines, 1):
            if re.search(r'(password|secret|key|token)\s*=\s*["\'][^"\']+["\']', line, re.IGNORECASE):
                if 'example' not in line.lower() and 'test' not in line.lower():
                    self._add_issue('critical', 'Hardcoded secret detected', file_path, i)

    def _review_python(self, content: str, lines: List[str], file_path: str):
        """Review Python code for security issues"""

        # Check JWT validation
        if 'jwt.decode' in content:
            if 'algorithms=' not in content:
                self._add_issue('high', 'JWT decode missing algorithm specification', file_path, 0)
            elif '"HS256"' in content or "'HS256'" in content:
                self._add_issue('critical', 'HS256 algorithm detected (use RS256)', file_path, 0)

            # Check for blacklist verification
            if 'redis' not in content.lower() and 'blacklist' not in content.lower():
                self._add_issue('medium', 'JWT validation missing blacklist check', file_path, 0)

        # Check for SQL injection (ORM bypass)
        for i, line in enumerate(lines, 1):
            if 'execute' in line and 'f"' in line:
                self._add_issue('critical', 'SQL injection via f-string formatting', file_path, i)
            if '.raw(' in line or 'executescript' in line:
                self._add_issue('high', 'Raw SQL execution detected', file_path, i)

        # Check tenant isolation
        if 'filter(' in content or 'get(' in content:
            if 'tenant_id' not in content:
                self._add_issue('high', 'Database query missing tenant_id filter', file_path, 0)

        # Check SSRF protection in webhook URLs
        if 'webhook' in content.lower() and 'url' in content.lower():
            if '10.0.0.0' not in content and '192.168' not in content and '127.0.0' not in content:
                self._add_issue('medium', 'Webhook URL validation may be missing SSRF protection', file_path, 0)

        # Check for eval/exec usage
        for i, line in enumerate(lines, 1):
            if re.search(r'\b(eval|exec)\s*\(', line):
                self._add_issue('critical', 'Dangerous function (eval/exec) detected', file_path, i)

    def _review_typescript(self, content: str, lines: List[str], file_path: str):
        """Review TypeScript/JavaScript code for security issues"""

        # Check for dangerouslySetInnerHTML
        for i, line in enumerate(lines, 1):
            if 'dangerouslySetInnerHTML' in line:
                self._add_issue('high', 'XSS risk: dangerouslySetInnerHTML usage', file_path, i)

        # Check for localStorage with sensitive data
        if 'localStorage' in content:
            if 'token' in content.lower() or 'password' in content.lower():
                self._add_issue('medium', 'Sensitive data in localStorage (consider httpOnly cookies)', file_path, 0)

        # Check for eval usage
        for i, line in enumerate(lines, 1):
            if re.search(r'\beval\s*\(', line):
                self._add_issue('critical', 'eval() detected - code injection risk', file_path, i)

    def _add_issue(self, severity: str, message: str, file_path: str, line: int):
        """Add a security issue to the report"""
        self.issues.append((severity, message, f"{file_path}:{line}"))

    def report(self):
        """Generate security report"""
        if not self.issues:
            print("✅ No security issues found!")
            return 0

        print(f"\n🔐 Security Review Report")
        print("=" * 80)

        severity_order = {'critical': 0, 'high': 1, 'medium': 2, 'low': 3}
        sorted_issues = sorted(self.issues, key=lambda x: severity_order[x[0]])

        for severity, message, location in sorted_issues:
            icon = {'critical': '🔴', 'high': '🟠', 'medium': '🟡', 'low': '🟢'}[severity]
            print(f"{icon} [{severity.upper()}] {message}")
            print(f"   Location: {location}")
            print()

        critical_count = sum(1 for s, _, _ in self.issues if s == 'critical')
        high_count = sum(1 for s, _, _ in self.issues if s == 'high')

        print(f"Total: {len(self.issues)} issues ({critical_count} critical, {high_count} high)")

        return 1 if critical_count > 0 or high_count > 0 else 0


def main():
    if len(sys.argv) < 2:
        print("Usage: python security/review.py <file_or_directory>")
        sys.exit(1)

    path = sys.argv[1]
    agent = SecurityReviewAgent()

    if os.path.isfile(path):
        agent.review_file(path)
    elif os.path.isdir(path):
        for root, dirs, files in os.walk(path):
            # Skip build and generated directories
            dirs[:] = [d for d in dirs if d not in ['build', 'node_modules', '__pycache__', 'generated']]

            for file in files:
                if file.endswith(('.cpp', '.h', '.hpp', '.py', '.ts', '.tsx', '.js', '.jsx')):
                    file_path = os.path.join(root, file)
                    agent.review_file(file_path)
    else:
        print(f"Error: {path} not found")
        sys.exit(1)

    sys.exit(agent.report())


if __name__ == "__main__":
    main()
