#!/usr/bin/env python3
"""
Testing Coverage Agent
Validates test coverage of critical paths and security flows
"""
import re
import sys
import os
import yaml
from typing import List


class TestingCoverageAgent:
    def __init__(self, config_path=".agents/config.yaml"):
        with open(config_path) as f:
            self.config = yaml.safe_load(f)['testing']
        self.issues = []
        self.test_categories = {
            'auth': False,
            'multi_tenant': False,
            'error_handling': False,
            'edge_cases': False,
            'integration': False
        }

    def review_test_file(self, file_path: str):
        """Review test file for coverage"""
        with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()

        # Identify test categories covered
        if 'auth' in file_path.lower() or 'login' in content.lower():
            self.test_categories['auth'] = True
            self._check_auth_coverage(content, file_path)

        if 'tenant' in content.lower():
            self.test_categories['multi_tenant'] = True
            self._check_multi_tenant_tests(content, file_path)

        if 'error' in content.lower() or 'exception' in content.lower():
            self.test_categories['error_handling'] = True

        if 'edge' in content.lower() or 'boundary' in content.lower():
            self.test_categories['edge_cases'] = True

        if 'integration' in file_path.lower():
            self.test_categories['integration'] = True

    def _check_auth_coverage(self, content: str, file_path: str):
        """Verify authentication test coverage"""
        critical_tests = {
            'login': 'test.*login|login.*test',
            'logout': 'test.*logout|logout.*test',
            'refresh': 'test.*refresh.*token',
            'expired_token': 'test.*expir|expir.*test',
            'invalid_token': 'test.*invalid.*token',
            'blacklist': 'test.*blacklist|test.*revok'
        }

        missing = []
        for name, pattern in critical_tests.items():
            if not re.search(pattern, content, re.IGNORECASE):
                missing.append(name)

        if missing:
            self._add_issue('high', f'Missing critical auth tests: {", ".join(missing)}', file_path, 0)

    def _check_multi_tenant_tests(self, content: str, file_path: str):
        """Verify multi-tenancy isolation tests"""
        # Check for cross-tenant access tests
        if 'different.*tenant' not in content.lower() and 'other.*tenant' not in content.lower():
            self._add_issue('high', 'Missing cross-tenant isolation test', file_path, 0)

        # Check for tenant_id validation tests
        if 'tenant_id' in content.lower():
            if 'invalid.*tenant' not in content.lower() and 'wrong.*tenant' not in content.lower():
                self._add_issue('medium', 'Missing invalid tenant_id test', file_path, 0)

    def _add_issue(self, severity: str, message: str, file_path: str, line: int):
        """Add a testing issue"""
        self.issues.append((severity, message, f"{file_path}:{line}"))

    def report(self):
        """Generate testing coverage report"""
        print(f"\n🧪 Testing Coverage Report")
        print("=" * 80)

        # Report on test categories
        print("Test Category Coverage:")
        for category, covered in self.test_categories.items():
            icon = "✅" if covered else "❌"
            print(f"  {icon} {category.replace('_', ' ').title()}")
        print()

        # Report specific issues
        if self.issues:
            for severity, message, location in self.issues:
                icon = {'high': '🟠', 'medium': '🟡', 'low': '🟢'}[severity]
                print(f"{icon} [{severity.upper()}] {message}")
                print(f"   Location: {location}")
                print()
            print(f"Total: {len(self.issues)} coverage gaps")
        else:
            print("✅ No specific coverage issues found!")

        # Check minimum coverage
        covered_count = sum(1 for c in self.test_categories.values() if c)
        coverage_pct = (covered_count / len(self.test_categories)) * 100
        print(f"\nCategory Coverage: {coverage_pct:.0f}%")

        return 1 if coverage_pct < 60 or any(s[0] == 'high' for s in self.issues) else 0


def main():
    if len(sys.argv) < 2:
        print("Usage: python testing/review.py <test_directory>")
        sys.exit(1)

    path = sys.argv[1]
    agent = TestingCoverageAgent()

    if os.path.isfile(path):
        agent.review_test_file(path)
    elif os.path.isdir(path):
        for root, dirs, files in os.walk(path):
            for file in files:
                if 'test' in file.lower() and file.endswith(('.py', '.ts', '.cpp')):
                    agent.review_test_file(os.path.join(root, file))

    sys.exit(agent.report())


if __name__ == "__main__":
    main()
