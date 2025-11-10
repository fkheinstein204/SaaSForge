# Agent Quick Start Guide

## 🚀 Running Agents Locally

### All Agents at Once
```bash
python3 .agents/run_all.py <file_or_directory>

# Examples
python3 .agents/run_all.py services/cpp/auth/
python3 .agents/run_all.py api/routers/payment.py
python3 .agents/run_all.py db/migrations/001_initial_schema.sql
```

### Individual Agents

**Security Review:**
```bash
python3 .agents/security/review.py services/cpp/auth/
# Checks: JWT validation, tenant isolation, SSRF, secrets
```

**Architecture Compliance:**
```bash
python3 .agents/architecture/review.py api/routers/
# Checks: SRS requirements, error codes, idempotency
```

**Performance Review:**
```bash
python3 .agents/performance/review.py api/
# Checks: N+1 queries, missing indexes, caching
```

**Database Schema:**
```bash
python3 .agents/database/review.py db/migrations/001_initial_schema.sql
# Checks: tenant_id, indexes, foreign keys, reversibility
```

**Testing Coverage:**
```bash
python3 .agents/testing/review.py tests/
# Checks: Auth tests, multi-tenant tests, edge cases
```

## 🔧 Configuration

Edit `.agents/config.yaml` to customize:
- Severity thresholds
- Enable/disable specific checks
- Performance targets
- Coverage requirements

## 📊 Understanding Output

**Icons:**
- 🔴 CRITICAL - Must fix before merge
- 🟠 HIGH - Should fix before merge
- 🟡 MEDIUM - Fix when possible
- 🟢 LOW - Nice to have

**Exit Codes:**
- 0 = No issues or only low/medium severity
- 1 = Critical or high severity issues found

## 🤖 CI/CD Integration

Agents automatically run on all PRs via `.github/workflows/agents.yml`

Failed agents will:
- ❌ Block PR merge (security, architecture critical issues)
- ⚠️ Show warnings (performance, testing coverage)

## 💡 Tips

1. **Run before committing:**
   ```bash
   python3 .agents/run_all.py $(git diff --name-only --cached)
   ```

2. **Add to git hooks:**
   ```bash
   ln -s ../../.agents/pre-commit.sh .git/hooks/pre-commit
   ```

3. **Focus on changed files:**
   ```bash
   git diff --name-only main | xargs -I {} python3 .agents/run_all.py {}
   ```

## 📚 Learn More

See individual agent READMEs in `.agents/<agent>/README.md` for detailed documentation on each check.
