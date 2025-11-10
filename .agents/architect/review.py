#!/usr/bin/env python3
"""
Software Architect Agent
Reviews high-level design decisions, system architecture, and scalability patterns
Specific to SaaSForge's three-tier architecture (React → FastAPI → C++ gRPC)
"""
import re
import sys
import os
from pathlib import Path
from typing import List, Dict
import yaml


class ArchitectReviewAgent:
    def __init__(self, config_path=".agents/config.yaml"):
        with open(config_path) as f:
            config = yaml.safe_load(f)
            self.config = config.get('architect', {})
        self.issues = []
        self.architecture_violations = []

    def review_codebase(self, path: str):
        """Review entire codebase for architectural compliance"""
        if os.path.isfile(path):
            self._review_file(path)
        else:
            self._review_directory(path)

    def _review_directory(self, path: str):
        """Review directory structure and architectural boundaries"""
        for root, dirs, files in os.walk(path):
            # Skip non-code directories
            dirs[:] = [d for d in dirs if d not in ['build', 'node_modules', '__pycache__', 'dist', '.git']]

            for file in files:
                file_path = os.path.join(root, file)
                if file.endswith(('.cpp', '.h', '.hpp', '.py', '.ts', '.tsx')):
                    self._review_file(file_path)

    def _review_file(self, file_path: str):
        """Review individual file for architectural patterns"""
        with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()
            lines = content.split('\n')

        # Determine layer
        layer = self._identify_layer(file_path)

        # Layer-specific reviews
        if layer == 'ui':
            self._review_ui_layer(content, lines, file_path)
        elif layer == 'bff':
            self._review_bff_layer(content, lines, file_path)
        elif layer == 'service':
            self._review_service_layer(content, lines, file_path)

        # Cross-cutting concerns
        self._review_separation_of_concerns(content, lines, file_path, layer)
        self._review_communication_patterns(content, lines, file_path, layer)
        self._review_resilience_patterns(content, lines, file_path)
        self._review_observability(content, lines, file_path)

    def _identify_layer(self, file_path: str) -> str:
        """Identify which architectural layer the file belongs to"""
        if '/ui/' in file_path or file_path.startswith('ui/'):
            return 'ui'
        elif '/api/' in file_path or file_path.startswith('api/'):
            return 'bff'
        elif '/services/cpp/' in file_path:
            return 'service'
        return 'unknown'

    def _review_ui_layer(self, content: str, lines: List[str], file_path: str):
        """Review UI layer architectural patterns"""
        # UI should NOT directly call gRPC services
        if 'grpc' in content.lower():
            self._add_issue('critical',
                          'UI layer bypassing BFF - direct gRPC call detected',
                          file_path, 0, 'ARCH-001')

        # UI should NOT have business logic
        if any(keyword in content.lower() for keyword in ['calculate_', 'validate_payment', 'process_']):
            # Check if it's in a component (not a utility)
            if 'component' in file_path.lower() or 'page' in file_path.lower():
                self._add_issue('high',
                              'Business logic in UI component - move to BFF',
                              file_path, 0, 'ARCH-002')

        # Check state management patterns
        if 'useState' in content:
            # Count useState calls
            count = content.count('useState')
            if count > 5:
                self._add_issue('medium',
                              f'Component has {count} useState calls - consider Zustand store',
                              file_path, 0, 'ARCH-003')

        # Check for proper API client usage
        if 'fetch(' in content or 'axios.' in content:
            # Should use centralized API client
            if 'api/client' not in content and 'apiClient' not in content:
                self._add_issue('medium',
                              'Direct fetch/axios call - use centralized API client',
                              file_path, 0, 'ARCH-004')

    def _review_bff_layer(self, content: str, lines: List[str], file_path: str):
        """Review BFF (Backend for Frontend) layer patterns"""
        # BFF should NOT have business logic - delegate to gRPC services
        if 'def process_' in content or 'async def calculate_' in content:
            if 'grpc' not in content.lower() or 'stub' not in content.lower():
                self._add_issue('high',
                              'Business logic in BFF - delegate to gRPC service',
                              file_path, 0, 'ARCH-005')

        # BFF should NOT directly access database (use gRPC services)
        if 'SELECT' in content or 'execute(' in content or 'query(' in content:
            if 'test' not in file_path.lower():
                self._add_issue('critical',
                              'BFF directly accessing database - use gRPC service',
                              file_path, 0, 'ARCH-006')

        # Check for proper gRPC client usage
        if 'router' in file_path.lower():
            if 'grpc' not in content.lower() and 'stub' not in content.lower():
                if 'not implemented' not in content.lower():
                    self._add_issue('medium',
                                  'Router endpoint not calling gRPC service',
                                  file_path, 0, 'ARCH-007')

        # Check for aggregation pattern (BFF's primary role)
        if 'async def' in content:
            # Good - async for parallel gRPC calls
            pass
        elif 'def ' in content and 'grpc' in content.lower():
            self._add_issue('low',
                          'BFF endpoint not async - consider parallelizing gRPC calls',
                          file_path, 0, 'ARCH-008')

    def _review_service_layer(self, content: str, lines: List[str], file_path: str):
        """Review C++ gRPC service layer patterns"""
        # Services should be stateless
        if 'static' in content and 'state' in content.lower():
            if 'thread_local' not in content:
                self._add_issue('high',
                              'Service has shared mutable state - violates stateless design',
                              file_path, 0, 'ARCH-009')

        # Check for proper tenant context propagation
        if 'grpc::ServerContext' in content:
            if 'tenant' not in content.lower():
                self._add_issue('high',
                              'gRPC handler missing tenant context extraction',
                              file_path, 0, 'ARCH-010')

        # Services should use repository pattern for data access
        if 'SELECT' in content or 'INSERT' in content:
            if 'repository' not in file_path.lower() and 'dao' not in file_path.lower():
                self._add_issue('medium',
                              'Direct SQL in service - use repository pattern',
                              file_path, 0, 'ARCH-011')

        # Check for proper error handling
        if 'return grpc::Status' in content:
            # Good - proper gRPC status codes
            pass
        elif 'throw' in content and 'service' in file_path.lower():
            self._add_issue('medium',
                          'Service throwing exceptions - return grpc::Status instead',
                          file_path, 0, 'ARCH-012')

    def _review_separation_of_concerns(self, content: str, lines: List[str], file_path: str, layer: str):
        """Review separation of concerns across layers"""
        # Check for cross-layer dependencies
        violations = {
            'ui': ['psycopg', 'sqlalchemy', 'libpqxx'],  # UI shouldn't access DB
            'bff': ['libpqxx'],  # BFF shouldn't use C++ DB libs
            'service': ['react', 'fastapi']  # Services shouldn't know about UI/BFF
        }

        if layer in violations:
            for forbidden in violations[layer]:
                if forbidden in content.lower():
                    self._add_issue('critical',
                                  f'{layer.upper()} layer using {forbidden} - violates separation',
                                  file_path, 0, 'ARCH-013')

    def _review_communication_patterns(self, content: str, lines: List[str], file_path: str, layer: str):
        """Review inter-service communication patterns"""
        # Check for synchronous vs asynchronous patterns
        if layer == 'bff' and 'grpc' in content.lower():
            # Count sequential gRPC calls
            grpc_calls = re.findall(r'stub\.(\w+)\(', content)
            if len(grpc_calls) >= 3:
                # Check if they're parallelized
                if 'asyncio.gather' not in content and 'await.*await.*await' in content:
                    self._add_issue('medium',
                                  f'{len(grpc_calls)} sequential gRPC calls - parallelize with asyncio.gather',
                                  file_path, 0, 'ARCH-014')

        # Check for event-driven patterns where appropriate
        if 'notification' in file_path.lower() or 'webhook' in file_path.lower():
            if 'queue' not in content.lower() and 'async' not in content.lower():
                self._add_issue('low',
                              'Notification/webhook should be async/queued',
                              file_path, 0, 'ARCH-015')

    def _review_resilience_patterns(self, content: str, lines: List[str], file_path: str):
        """Review resilience and fault tolerance patterns"""
        # Check for retry logic
        if 'grpc' in content.lower() or 'http' in content.lower():
            if 'retry' not in content.lower() and 'backoff' not in content.lower():
                if 'client' in file_path.lower():
                    self._add_issue('medium',
                                  'External call without retry/backoff - add resilience',
                                  file_path, 0, 'ARCH-016')

        # Check for circuit breaker pattern
        if 'stripe' in content.lower() or 'twilio' in content.lower():
            if 'circuit' not in content.lower() and 'breaker' not in content.lower():
                self._add_issue('low',
                              'External service call without circuit breaker',
                              file_path, 0, 'ARCH-017')

        # Check for timeout configuration
        if 'grpc.insecure_channel' in content or 'requests.get' in content:
            if 'timeout' not in content.lower():
                self._add_issue('high',
                              'Network call without timeout - can cause thread exhaustion',
                              file_path, 0, 'ARCH-018')

    def _review_observability(self, content: str, lines: List[str], file_path: str):
        """Review observability patterns (logging, metrics, tracing)"""
        # Check for structured logging
        if 'logger' in content.lower() or 'log' in content.lower():
            if 'correlation_id' not in content.lower() and 'trace_id' not in content.lower():
                if 'middleware' not in file_path.lower():
                    self._add_issue('low',
                                  'Logging without correlation_id - add for request tracing',
                                  file_path, 0, 'ARCH-019')

        # Check for metrics in critical paths
        if any(keyword in file_path.lower() for keyword in ['auth', 'payment', 'upload']):
            if 'metric' not in content.lower() and 'prometheus' not in content.lower():
                if 'service.cpp' in file_path or 'router.py' in file_path:
                    self._add_issue('low',
                                  'Critical path missing metrics instrumentation',
                                  file_path, 0, 'ARCH-020')

    def _add_issue(self, severity: str, message: str, file_path: str, line: int, code: str):
        """Add an architectural issue"""
        self.issues.append((severity, code, message, f"{file_path}:{line}"))

    def report(self):
        """Generate architect review report"""
        if not self.issues:
            print("✅ No architectural issues found!")
            print("\n🏗️  Architecture Review: PASSED")
            print("   Three-tier architecture maintained")
            print("   Separation of concerns verified")
            print("   Communication patterns compliant")
            return 0

        print(f"\n🏗️  Software Architect Review Report")
        print("=" * 80)
        print("SaaSForge Architecture: React → FastAPI BFF → C++ gRPC Services")
        print("=" * 80)
        print()

        severity_order = {'critical': 0, 'high': 1, 'medium': 2, 'low': 3}
        sorted_issues = sorted(self.issues, key=lambda x: severity_order[x[0]])

        for severity, code, message, location in sorted_issues:
            icon = {'critical': '🔴', 'high': '🟠', 'medium': '🟡', 'low': '🟢'}[severity]
            print(f"{icon} [{code}] [{severity.upper()}] {message}")
            print(f"   Location: {location}")
            print()

        # Category breakdown
        categories = {}
        for _, code, _, _ in self.issues:
            category = code.split('-')[0]
            categories[category] = categories.get(category, 0) + 1

        print(f"Summary by Category:")
        print(f"  ARCH: {categories.get('ARCH', 0)} architectural violations")
        print()

        critical_count = sum(1 for s, _, _, _ in self.issues if s == 'critical')
        high_count = sum(1 for s, _, _, _ in self.issues if s == 'high')

        print(f"Total: {len(self.issues)} issues ({critical_count} critical, {high_count} high)")
        print()

        # Architectural guidance
        if critical_count > 0:
            print("❌ ARCHITECTURE REVIEW FAILED")
            print("\nCritical violations detected:")
            print("- Review three-tier architecture boundaries")
            print("- Ensure proper layer separation (UI → BFF → Services)")
            print("- Verify no direct database access from UI/BFF")
        elif high_count > 0:
            print("⚠️  ARCHITECTURE REVIEW: WARNINGS")
            print("\nHigh-priority items to address:")
            print("- Review business logic placement")
            print("- Check communication patterns")
            print("- Verify resilience patterns")

        return 1 if critical_count > 0 or high_count > 0 else 0


def main():
    if len(sys.argv) < 2:
        print("Usage: python architect/review.py <file_or_directory>")
        print("\nSoftware Architect Agent - Reviews system architecture and design patterns")
        print("Specific to SaaSForge's three-tier architecture:")
        print("  - UI Layer (React): Presentation only, no business logic")
        print("  - BFF Layer (FastAPI): Aggregation, no business logic")
        print("  - Service Layer (C++ gRPC): Business logic, stateless")
        sys.exit(1)

    path = sys.argv[1]
    agent = ArchitectReviewAgent()

    print(f"🏗️  Reviewing architecture of: {path}")
    print()

    agent.review_codebase(path)
    sys.exit(agent.report())


if __name__ == "__main__":
    main()
