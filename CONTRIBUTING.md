# Contributing to SaaSForge

Thank you for your interest in contributing to SaaSForge!

## Development Setup

1. Clone the repository:
   ```bash
   git clone https://github.com/your-org/SaaSForge.git
   cd SaaSForge
   ```

2. Run the setup script:
   ```bash
   make setup
   ```

3. Start development services:
   ```bash
   make docker-up
   make migrate
   ```

## Project Structure

- `services/cpp/` - C++ gRPC microservices
- `api/` - FastAPI backend for frontend (BFF)
- `ui/` - React TypeScript frontend
- `proto/` - gRPC protocol buffer definitions
- `k8s/` - Kubernetes manifests
- `db/` - Database migrations and seeds
- `tests/` - Integration, E2E, and load tests

## Development Workflow

1. Create a feature branch:
   ```bash
   git checkout -b feature/your-feature-name
   ```

2. Make your changes and write tests

3. Run tests:
   ```bash
   make test
   ```

4. Format code:
   ```bash
   make format
   ```

5. Commit your changes:
   ```bash
   git add .
   git commit -m "feat: your feature description"
   ```

6. Push and create a pull request

## Commit Message Convention

Follow [Conventional Commits](https://www.conventionalcommits.org/):

- `feat:` - New feature
- `fix:` - Bug fix
- `docs:` - Documentation changes
- `test:` - Test additions/changes
- `refactor:` - Code refactoring
- `chore:` - Build/tooling changes

## Code Style

- **C++**: Follow Google C++ Style Guide
- **Python**: Follow PEP 8, use Black formatter
- **TypeScript**: Follow Airbnb style, use ESLint + Prettier

## Testing

- Write unit tests for all new functionality
- Integration tests for cross-service interactions
- E2E tests for critical user flows
- Load tests for performance-sensitive endpoints

## Pull Request Guidelines

- Keep PRs focused and small
- Include tests
- Update documentation
- Ensure CI passes
- Request review from maintainers

## Questions?

Open an issue or discussion on GitHub!
