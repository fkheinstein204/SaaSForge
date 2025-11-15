#!/bin/bash
#
# @file        run-email-worker.sh
# @brief       Start the email queue worker
# @author      Heinstein F.
# @date        2025-11-15
#

# Change to API directory
cd "$(dirname "$0")/../api" || exit 1

# Activate virtual environment if it exists
if [ -d "venv" ]; then
    source venv/bin/activate
fi

# Set environment variables
export PYTHONPATH="${PYTHONPATH}:$(pwd)"
export REDIS_HOST="${REDIS_HOST:-localhost}"
export REDIS_PORT="${REDIS_PORT:-6379}"
export SENDGRID_API_KEY="${SENDGRID_API_KEY:-}"
export SENDGRID_FROM_EMAIL="${SENDGRID_FROM_EMAIL:-noreply@saasforge.com}"
export SENDGRID_FROM_NAME="${SENDGRID_FROM_NAME:-SaaSForge}"
export SENDGRID_DAILY_LIMIT="${SENDGRID_DAILY_LIMIT:-100}"

# Run the worker
python3 -m workers.email_worker
