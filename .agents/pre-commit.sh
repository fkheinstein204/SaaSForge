#!/bin/bash
# Git pre-commit hook to run agents on staged files

echo "🤖 Running SaaSForge review agents on staged files..."

# Get list of staged files
STAGED_FILES=$(git diff --cached --name-only --diff-filter=ACM)

if [ -z "$STAGED_FILES" ]; then
  echo "No files to review"
  exit 0
fi

# Run security and architect agents (most critical)
python3 .agents/security/review.py $STAGED_FILES
SECURITY_EXIT=$?

python3 .agents/architect/review.py $STAGED_FILES
ARCHITECT_EXIT=$?

if [ $SECURITY_EXIT -ne 0 ] || [ $ARCHITECT_EXIT -ne 0 ]; then
  echo ""
  echo "❌ Pre-commit check failed!"
  echo "Fix critical issues or use 'git commit --no-verify' to bypass"
  exit 1
fi

echo "✅ Pre-commit checks passed!"
exit 0
