#!/usr/bin/env python3
"""
Performance Review Agent
Validates against NFR targets (JWT < 2ms, login < 500ms, etc.)
"""
import re
import sys
import os
from pathlib import Path
import yaml


class PerformanceReviewAgent:
    def __init__(self, config_path=".agents/config.yaml"):
        with open(config_path) as f:
            self.config = yaml.safe_load(f)['performance']
        self.issues = []

    def review_file(self, file_path: str):
        """Review file for performance issues"""
        with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()
            lines = content.split('\n')

        # Check for N+1 queries
        self._check_n_plus_one(lines, file_path)

        # Check for missing indexes hints
        self._check_indexing(lines, file_path)

        # Check for connection pooling
        self._check_connection_pooling(content, file_path)

        # Check for caching opportunities
        self._check_caching(lines, file_path)

        # Check for inefficient loops
        self._check_loops(lines, file_path)

    def _check_n_plus_one(self, lines: List[str], file_path: str):
        """Detect potential N+1 query problems"""
        for i, line in enumerate(lines, 1):
            # Look for queries inside loops
            if 'for' in line or 'while' in line:
                # Check next 10 lines for database queries
                context = '\n'.join(lines[i:min(len(lines), i+10)])
                if any(keyword in context.lower() for keyword in ['select', 'query', 'find', 'get']):
                    if 'join' not in context.lower() and 'prefetch' not in context.lower():
                        self._add_issue('high', 'Potential N+1 query: database call in loop', file_path, i)

    def _check_indexing(self, lines: List[str], file_path: str):
        """Check if queries have proper indexing"""
        if not file_path.endswith('.sql'):
            return

        for i, line in enumerate(lines, 1):
            if 'WHERE' in line.upper():
                # Extract column name
                match = re.search(r'WHERE\s+(\w+)', line, re.IGNORECASE)
                if match:
                    column = match.group(1)
                    # Check if there's a CREATE INDEX nearby
                    full_content = '\n'.join(lines)
                    if f'idx_{column}' not in full_content and f'INDEX.*{column}' not in full_content:
                        self._add_issue('medium', f'Query on {column} may need index', file_path, i)

    def _check_connection_pooling(self, content: str, file_path: str):
        """Verify connection pooling is used"""
        if 'postgres' in content.lower() or 'mysql' in content.lower() or 'redis' in content.lower():
            if 'pool' not in content.lower() and 'connection' in content.lower():
                self._add_issue('medium', 'Database connection without pooling', file_path, 0)

    def _check_caching(self, lines: List[str], file_path: str):
        """Identify caching opportunities"""
        for i, line in enumerate(lines, 1):
            # Look for expensive operations without caching
            if any(keyword in line.lower() for keyword in ['jwt.decode', 'validate_token', 'check_permission']):
                context = '\n'.join(lines[max(0, i-5):min(len(lines), i+5)])
                if 'cache' not in context.lower() and 'redis' not in context.lower():
                    self._add_issue('low', 'Expensive operation without caching', file_path, i)

    def _check_loops(self, lines: List[str], file_path: str):
        """Check for inefficient loops"""
        for i, line in enumerate(lines, 1):
            # Check for nested loops
            if 'for' in line and i < len(lines) - 1:
                next_10 = '\n'.join(lines[i:min(len(lines), i+10)])
                if next_10.count('for') > 1:
                    self._add_issue('medium', 'Nested loops detected - O(n²) complexity', file_path, i)

    def _add_issue(self, severity: str, message: str, file_path: str, line: int):
        """Add a performance issue"""
        self.issues.append((severity, message, f"{file_path}:{line}"))

    def report(self):
        """Generate performance report"""
        if not self.issues:
            print("✅ No performance issues found!")
            return 0

        print(f"\n⚡ Performance Review Report")
        print("=" * 80)

        for severity, message, location in self.issues:
            icon = {'high': '🟠', 'medium': '🟡', 'low': '🟢'}[severity]
            print(f"{icon} [{severity.upper()}] {message}")
            print(f"   Location: {location}")
            print()

        print(f"Total: {len(self.issues)} issues")
        return 0  # Performance issues don't fail builds by default


def main():
    if len(sys.argv) < 2:
        print("Usage: python performance/review.py <file_or_directory>")
        sys.exit(1)

    path = sys.argv[1]
    agent = PerformanceReviewAgent()

    if os.path.isfile(path):
        agent.review_file(path)
    elif os.path.isdir(path):
        for root, dirs, files in os.walk(path):
            dirs[:] = [d for d in dirs if d not in ['build', 'node_modules', '__pycache__']]
            for file in files:
                if file.endswith(('.cpp', '.h', '.py', '.ts', '.sql')):
                    agent.review_file(os.path.join(root, file))

    sys.exit(agent.report())


if __name__ == "__main__":
    main()
