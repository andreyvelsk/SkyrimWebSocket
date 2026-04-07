## 🚀 Quick Start

### 1. Install Tools

```bash
# Requires Node.js 16+
node --version

# Install dependencies
npm install
```

This will install:
- **commitlint** — commit format verification
- **husky** — git hooks (v9.x)
- **standard-version** — automatic versioning

### 2. Initialize Git Hooks

```bash
npx husky init
```

**What happens:**
- ✅ Each commit will be checked for Conventional Commits format
- ✅ Incorrect commits will be rejected with hints

### 3. Creating Commits

```bash
# ✅ Correct
git commit -m "feat(ws-server): add message batching support"
git commit -m "fix(session): prevent memory leak"

# ❌ Incorrect (will be rejected)
git commit -m "updated stuff"
```

### 4. Creating a Release

When you need to create a new release:

```bash
# Preview what will be changed (without writing)
npm run release:dry-run

# Create release
npm run release
```

**What happens:**
1. All commits since the last release are analyzed
2. New version is determined (major, minor, patch)
3. Files are updated:
   - `CMakeLists.txt` — project version
   - `vcpkg.json` — vcpkg package version (`version-string`)
   - `package.json` — package version
   - `CHANGELOG.md` — change entries are created
4. Git tag is created (e.g., `v1.2.3`)
5. Everything is automatically committed

### 5. Push the Release

```bash
git push --follow-tags
```

---

## 📚 For More Details

See [DEVELOPER.md](./DEVELOPER.md) for comprehensive examples of all commit types.

## 🔹 Commit Types Quick Reference

```
feat    — new feature               (minor version)
fix     — bug fix                   (patch version)
feat!   — breaking change           (major version)
perf    — performance improvement   (patch version)
refactor— code refactoring          (patch, hidden in CHANGELOG)
docs    — documentation            (patch, hidden in CHANGELOG)
test    — tests                     (hidden in CHANGELOG)
chore   — maintenance               (hidden in CHANGELOG)
ci/cd   — CI/CD config              (hidden in CHANGELOG)
```

---

**Done! Your project now uses semantic versioning. 🎉**
