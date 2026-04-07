# 👨‍💻 Developer Guide

## Conventional Commits & Semantic Versioning

This project uses [Conventional Commits](https://www.conventionalcommits.org/) for automatic versioning and CHANGELOG generation.

### Commit Types

Each commit must start with one of the following types:

#### 📝 **feat** — new feature
Adds new functionality. → **minor version** (0.1.0)
```bash
git commit -m "feat(ws-server): add support for custom message handlers"
git commit -m "feat(api): implement subscription filtering"
```

#### 🔧 **fix** — bug fix
Fixes a bug. → **patch version** (0.0.1)
```bash
git commit -m "fix(session): prevent memory leak in connection cleanup"
git commit -m "fix(protocol): handle malformed JSON gracefully"
```

#### 💥 **BREAKING CHANGE** — breaking change
Introduces incompatibility with the previous version. → **major version** (1.0.0)
```bash
git commit -m "refactor(api): change message format

BREAKING CHANGE: The 'query' field is renamed to 'path' in all WebSocket messages"
```

#### 🎨 **refactor** — code refactoring
Restructures code without changing functionality. → **patch version**
```bash
git commit -m "refactor(game-reader): optimize field lookup performance"
```

#### 📚 **docs** — documentation
Documentation-only changes. → **patch version**
```bash
git commit -m "docs: add API examples to PROTOCOL.md"
git commit -m "docs(installation): update setup instructions"
```

#### 🧹 **chore** — maintenance
Dependency updates, config changes, build tasks, etc. → **no version**
```bash
git commit -m "chore: update CMake to 3.25.1"
git commit -m "chore(deps): update Boost to 1.84"
```

#### 🧪 **test** — tests
Adding tests. → **no version**
```bash
git commit -m "test: add unit tests for FieldRegistry"
```

#### ⚡ **perf** — performance improvement
Performance improvements. → **patch version**
```bash
git commit -m "perf(broadcast): improve message serialization speed"
```

#### 🔒 **security** — security fixes
Security vulnerability fixes. → **patch version**
```bash
git commit -m "security: validate input data in message router"
```

### Scopes

Use scopes to clarify which part of the project is affected:

```
feat(ws-server): add message batching
     ^^^^^^^^^ scope
```

**Recommended scopes:**
- `ws-server` — WebSocket server
- `session` — session management
- `protocol` — protocol
- `game-reader` — reading game state
- `game-writer` — modifying game state
- `broadcast` — message broadcasting
- `build` — CMake and build
- `ci` — CI/CD
- `deps` — dependencies

### Good Commit Examples

```bash
# New feature with scope
git commit -m "feat(session): add connection timeout configuration"

# Fix with description
git commit -m "fix(ws-session): handle WebSocket close frames correctly"

# Refactoring
git commit -m "refactor(message-router): simplify message dispatch logic"

# Breaking change
git commit -m "feat(protocol): redesign subscription API

BREAKING CHANGE: 'subscribe' command now requires 'fields' array instead of 'field' string"

# Documentation
git commit -m "docs(protocol): add examples for authentication flow"

# Maintenance
git commit -m "chore(cmake): enable C++23 features for all targets"
```

### ❌ Incorrect

```bash
# ❌ No type
git commit -m "updated server code"

# ❌ Wrong format
git commit -m "Feature: added something"

# ❌ Too generic
git commit -m "fixed bugs"

# ❌ Mixed types
git commit -m "feat: added X and fixed Y"
```

## 🚀 Automatic Versioning

### Locally

When needed, you can create a release manually:

```bash
# See what version will be created
npm run release:dry-run

# Create release (updates version in CMakeLists.txt, creates CHANGELOG, tags)
npm run release
```

**What happens:**
1. ✅ Analyzes all commits since the last release
2. ✅ Determines new version (major, minor, patch)
3. ✅ Updates `CMakeLists.txt` (PROJECT VERSION)
4. ✅ Updates `CHANGELOG.md` with change descriptions
5. ✅ Creates git tag (e.g., `v1.2.3`)
6. ✅ Commits changes automatically

### In CI/CD Pipeline

Pull requests are checked for correct commit format.
Releases are created automatically when merging to main/master.

## 🔍 Pre-commit Verification

Tools automatically verify the format:

```bash
# Detects error and suggests fix
git commit -m "updated stuff"

# ❌ commit-msg hook will reject this commit

# ✅ Must rewrite according to Conventional Commits
```

## 📖 Useful Links

- [conventionalcommits.org](https://www.conventionalcommits.org/)
- [Semantic Versioning](https://semver.org/)
- [Commitizen — interactive commit creation](http://commitizen.github.io/cz-cli/)
- [Standard-version — release automation](https://github.com/conventional-changelog/standard-version)

## 💡 Git Hooks

The project includes pre-commit and commit-msg hooks for commit verification.
They are automatically installed after running `npm install`.

If they are not installed, run:
```bash
npx husky init
```

If you need to skip verification (only in critical cases):
```bash
git commit --no-verify -m "some commit"
```

---

**Thank you for following the standards! 🙌**
