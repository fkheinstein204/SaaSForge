#!/usr/bin/env python3
"""
Database Schema Review Agent
Validates migrations, indexes, and multi-tenancy compliance
"""
import re
import sys
import os
import yaml


class DatabaseReviewAgent:
    def __init__(self, config_path=".agents/config.yaml"):
        with open(config_path) as f:
            self.config = yaml.safe_load(f)['database']
        self.issues = []

    def review_migration(self, file_path: str):
        """Review SQL migration file"""
        with open(file_path, 'r') as f:
            content = f.read()
            lines = content.split('\n')

        # Check tenant_id in all tables
        self._check_tenant_id(content, lines, file_path)

        # Check for missing indexes
        self._check_indexes(content, lines, file_path)

        # Check foreign key constraints
        self._check_foreign_keys(content, lines, file_path)

        # Check for reversibility
        self._check_reversibility(content, file_path)

        # Check data retention compliance
        self._check_retention(content, file_path)

    def _check_tenant_id(self, content: str, lines: List[str], file_path: str):
        """Ensure all tables have tenant_id"""
        create_table_pattern = r'CREATE TABLE\s+(\w+)'
        tables = re.findall(create_table_pattern, content, re.IGNORECASE)

        # Exceptions: these tables don't need tenant_id
        exceptions = ['tenants', 'sessions', 'migrations', 'alembic_version']

        for table in tables:
            if table.lower() not in exceptions:
                # Check if table definition includes tenant_id
                table_def_pattern = f'CREATE TABLE\\s+{table}.*?\\);'
                match = re.search(table_def_pattern, content, re.IGNORECASE | re.DOTALL)
                if match:
                    table_def = match.group(0)
                    if 'tenant_id' not in table_def.lower():
                        self._add_issue('critical', f'Table {table} missing tenant_id column', file_path, 0)

    def _check_indexes(self, content: str, lines: List[str], file_path: str):
        """Check for missing indexes on foreign keys and common filters"""
        # Find all foreign key columns
        fk_pattern = r'(\w+)\s+UUID.*?REFERENCES'
        foreign_keys = re.findall(fk_pattern, content, re.IGNORECASE)

        for fk in foreign_keys:
            if f'idx_{fk}' not in content and f'INDEX.*{fk}' not in content:
                self._add_issue('high', f'Missing index on foreign key: {fk}', file_path, 0)

        # Check for tenant_id indexes
        if 'tenant_id' in content.lower():
            if 'idx_.*tenant_id' not in content.lower() and 'index.*tenant_id' not in content.lower():
                self._add_issue('high', 'Missing index on tenant_id', file_path, 0)

    def _check_foreign_keys(self, content: str, lines: List[str], file_path: str):
        """Validate foreign key constraints"""
        # Check for ON DELETE clauses
        references_pattern = r'REFERENCES\s+\w+\([^)]+\)(?!\s*ON\s+DELETE)'
        matches = list(re.finditer(references_pattern, content, re.IGNORECASE))

        if matches:
            for match in matches[:3]:  # Limit to first 3 to avoid spam
                self._add_issue('medium', 'Foreign key missing ON DELETE clause', file_path,
                              content[:match.start()].count('\n') + 1)

    def _check_reversibility(self, content: str, file_path: str):
        """Check if migration can be reversed"""
        if 'DROP TABLE' in content.upper():
            if 'IF EXISTS' not in content.upper():
                self._add_issue('medium', 'DROP TABLE without IF EXISTS (not reversible)', file_path, 0)

        if 'ALTER TABLE' in content.upper() and 'DROP COLUMN' in content.upper():
            self._add_issue('low', 'ALTER TABLE DROP COLUMN is not reversible', file_path, 0)

    def _check_retention(self, content: str, file_path: str):
        """Check data retention compliance"""
        # Look for tables with audit/financial data
        if any(keyword in content.lower() for keyword in ['payment', 'invoice', 'transaction', 'audit']):
            if 'created_at' not in content.lower() and 'timestamp' not in content.lower():
                self._add_issue('medium', 'Financial/audit table missing timestamp columns', file_path, 0)

    def _add_issue(self, severity: str, message: str, file_path: str, line: int):
        """Add a database issue"""
        self.issues.append((severity, message, f"{file_path}:{line}"))

    def report(self):
        """Generate database review report"""
        if not self.issues:
            print("✅ No database schema issues found!")
            return 0

        print(f"\n🗄️  Database Schema Review Report")
        print("=" * 80)

        for severity, message, location in self.issues:
            icon = {'critical': '🔴', 'high': '🟠', 'medium': '🟡', 'low': '🟢'}[severity]
            print(f"{icon} [{severity.upper()}] {message}")
            print(f"   Location: {location}")
            print()

        print(f"Total: {len(self.issues)} issues")
        critical_count = sum(1 for s, _, _ in self.issues if s == 'critical')
        return 1 if critical_count > 0 else 0


def main():
    if len(sys.argv) < 2:
        print("Usage: python database/review.py <migration_file>")
        sys.exit(1)

    path = sys.argv[1]
    agent = DatabaseReviewAgent()

    if os.path.isfile(path):
        agent.review_migration(path)
    elif os.path.isdir(path):
        for root, dirs, files in os.walk(path):
            for file in files:
                if file.endswith('.sql'):
                    agent.review_migration(os.path.join(root, file))

    sys.exit(agent.report())


if __name__ == "__main__":
    main()
