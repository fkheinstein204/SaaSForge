#!/usr/bin/env python3
"""
Run all review agents on a file or directory
"""
import sys
import subprocess
from pathlib import Path


AGENTS = [
    ("🔐 Security Review", ".agents/security/review.py"),
    ("🏛️  Architecture Compliance", ".agents/architecture/review.py"),
    ("🏗️  Software Architect", ".agents/architect/review.py"),
    ("⚡ Performance Review", ".agents/performance/review.py"),
    ("🗄️  Database Schema", ".agents/database/review.py"),
    ("🧪 Testing Coverage", ".agents/testing/review.py"),
]


def main():
    if len(sys.argv) < 2:
        print("Usage: python .agents/run_all.py <file_or_directory>")
        sys.exit(1)

    target = sys.argv[1]

    print("=" * 80)
    print("SaaSForge Multi-Agent Review")
    print("=" * 80)
    print(f"Target: {target}\n")

    exit_codes = []

    for name, script in AGENTS:
        print(f"\n{'=' * 80}")
        print(f"Running {name}")
        print(f"{'=' * 80}")

        try:
            result = subprocess.run(
                ["python3", script, target],
                capture_output=False,
                text=True
            )
            exit_codes.append(result.returncode)
        except Exception as e:
            print(f"❌ Error running {name}: {e}")
            exit_codes.append(1)

    # Summary
    print(f"\n{'=' * 80}")
    print("Review Summary")
    print(f"{'=' * 80}")

    for (name, _), code in zip(AGENTS, exit_codes):
        status = "✅ PASS" if code == 0 else "❌ FAIL"
        print(f"{status} - {name}")

    failed = sum(1 for code in exit_codes if code != 0)
    if failed > 0:
        print(f"\n❌ {failed}/{len(AGENTS)} agents reported issues")
        sys.exit(1)
    else:
        print(f"\n✅ All agents passed!")
        sys.exit(0)


if __name__ == "__main__":
    main()
