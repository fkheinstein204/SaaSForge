.PHONY: help setup build test clean docker-up docker-down proto migrate

help: ## Show this help message
	@echo "SaaSForge - Multi-tenant SaaS Boilerplate"
	@echo ""
	@echo "Available targets:"
	@grep -E '^[a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) | awk 'BEGIN {FS = ":.*?## "}; {printf "  %-20s %s\n", $$1, $$2}'

setup: ## Setup development environment
	@echo "Setting up development environment..."
	@./scripts/setup-dev.sh

proto: ## Generate gRPC stubs from proto files
	@echo "Generating proto stubs..."
	@./scripts/gen-proto.sh

certs: ## Generate self-signed mTLS certificates (dev only)
	@echo "Generating certificates..."
	@./scripts/gen-certs.sh

install-cpp-deps: ## Install C++ dependencies (jwt-cpp, redis++)
	@echo "Installing C++ dependencies..."
	@./scripts/install-cpp-deps.sh

build-cpp: proto ## Build C++ services
	@echo "Building C++ services..."
	cd services/cpp && \
	mkdir -p build && \
	cd build && \
	cmake .. && \
	make -j$$(nproc)

build-api: ## Build FastAPI (install dependencies)
	@echo "Installing Python dependencies..."
	cd api && pip install -r requirements.txt

build-ui: ## Build React UI
	@echo "Building React UI..."
	cd ui && npm install && npm run build

build: build-cpp build-api build-ui ## Build all components

test-cpp: ## Run C++ unit tests
	@echo "Running C++ tests..."
	cd services/cpp/build && ctest --output-on-failure

test-api: ## Run Python tests
	@echo "Running Python tests..."
	cd api && pytest -v --cov

test-ui: ## Run React tests
	@echo "Running React tests..."
	cd ui && npm test

test-integration: ## Run integration tests
	@echo "Running integration tests..."
	pytest tests/integration/ -v

test-e2e: ## Run E2E tests
	@echo "Running E2E tests..."
	cd ui && npm run test:e2e

test-load: ## Run load tests
	@echo "Running load tests..."
	k6 run tests/load/api_load_test.js

test: test-cpp test-api test-ui ## Run all unit tests

migrate: ## Run database migrations
	@echo "Running migrations..."
	@./scripts/migrate.sh

docker-up: ## Start all services with Docker Compose
	docker-compose up -d

docker-down: ## Stop all services
	docker-compose down

docker-logs: ## Show logs from all services
	docker-compose logs -f

docker-build: ## Build Docker images
	docker-compose build

clean: ## Clean build artifacts
	@echo "Cleaning build artifacts..."
	rm -rf services/cpp/build
	rm -rf services/cpp/generated
	rm -rf api/generated
	rm -rf api/__pycache__
	rm -rf ui/dist
	rm -rf ui/node_modules
	find . -type d -name "__pycache__" -exec rm -rf {} +
	find . -type f -name "*.pyc" -delete

dev-api: ## Start FastAPI in development mode
	cd api && source venv/bin/activate && uvicorn main:app --reload --port 8000

dev-ui: ## Start React in development mode
	cd ui && npm run dev

lint-api: ## Lint Python code
	cd api && black . && flake8 .

lint-ui: ## Lint TypeScript code
	cd ui && npm run lint

format: lint-api lint-ui ## Format all code

.DEFAULT_GOAL := help
