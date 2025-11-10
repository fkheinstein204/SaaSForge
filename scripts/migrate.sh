#!/bin/bash
# Run database migrations

set -e

DATABASE_URL="${DATABASE_URL:-postgresql://saasforge:dev_password@localhost:5432/saasforge}"

echo "Running database migrations..."
echo "Database: $DATABASE_URL"

# Check if database is ready
until PGPASSWORD=dev_password psql -h localhost -U saasforge -d saasforge -c '\q' 2>/dev/null; do
  echo "Waiting for PostgreSQL..."
  sleep 2
done

echo "Database ready. Running migrations..."

# Run migrations in order
for migration in db/migrations/*.sql; do
  if [ -f "$migration" ]; then
    echo "Running: $(basename $migration)"
    PGPASSWORD=dev_password psql -h localhost -U saasforge -d saasforge -f "$migration"
  fi
done

echo "Migrations complete!"
