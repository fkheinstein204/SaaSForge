#!/bin/bash
# Setup local development environment

set -e

echo "Setting up SaaSForge development environment..."

# Check prerequisites
command -v docker >/dev/null 2>&1 || { echo "Docker is required but not installed. Aborting." >&2; exit 1; }
command -v docker-compose >/dev/null 2>&1 || { echo "Docker Compose is required but not installed. Aborting." >&2; exit 1; }

# Start infrastructure services
echo "Starting PostgreSQL and Redis..."
docker-compose up -d postgres redis

# Wait for services
echo "Waiting for services to be ready..."
sleep 5

# Run migrations
echo "Running database migrations..."
./scripts/migrate.sh

# Generate proto stubs
echo "Generating proto stubs..."
./scripts/gen-proto.sh

# Install Python dependencies
echo "Installing Python dependencies..."
cd api
python -m venv venv
source venv/bin/activate
pip install -r requirements.txt
cd ..

# Install Node dependencies
echo "Installing Node dependencies..."
cd ui
npm install
cd ..

echo ""
echo "Development environment setup complete!"
echo ""
echo "Next steps:"
echo "1. Build C++ services: cd services/cpp && mkdir build && cd build && cmake .. && make"
echo "2. Start FastAPI: cd api && source venv/bin/activate && uvicorn main:app --reload"
echo "3. Start React UI: cd ui && npm run dev"
echo ""
echo "Services will be available at:"
echo "- React UI: http://localhost:3000"
echo "- FastAPI: http://localhost:8000"
echo "- PostgreSQL: localhost:5432"
echo "- Redis: localhost:6379"
