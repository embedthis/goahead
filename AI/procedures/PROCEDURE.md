# GoAhead Development Procedures

## Table of Contents

1. [Development Environment Setup](#development-environment-setup)
2. [Building GoAhead](#building-goahead)
3. [Testing Procedures](#testing-procedures)
4. [Code Modification Procedures](#code-modification-procedures)
5. [Documentation Procedures](#documentation-procedures)
6. [Release Procedures](#release-procedures)
7. [Git Workflow](#git-workflow)

## Development Environment Setup

### Prerequisites

#### All Platforms
- C compiler (GCC, Clang, or MSVC)
- Make (GNU Make or compatible)
- Git for version control
- SSL library: OpenSSL (preferred) or MbedTLS
- TestMe test framework (for running tests)
- Bun runtime (for JavaScript/TypeScript tests)

#### Platform-Specific Requirements

**Linux**:
```bash
# Ubuntu/Debian
sudo apt-get install build-essential libssl-dev git

# Fedora/RHEL
sudo dnf install gcc make openssl-devel git
```

**macOS**:
```bash
# Install Xcode Command Line Tools
xcode-select --install

# OpenSSL via Homebrew
brew install openssl
```

**Windows**:
- Visual Studio 2022 or later
- [Git for Windows](https://gitforwindows.org/) (provides bash)
- [vcpkg](https://vcpkg.io/) for dependencies
- Windows PowerShell

Windows vcpkg setup:
```cmd
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg integrate install
.\vcpkg install openssl
```

### Installing TestMe

TestMe is required for running the test suite:

```bash
# Install TestMe globally
npm install -g testme

# Or using Bun
bun install -g testme
```

### Workspace Setup

```bash
# Clone repository
git clone https://github.com/embedthis/goahead.git
cd goahead

# Verify prerequisites
make help
```

## Building GoAhead

### Quick Build

**Linux/macOS**:
```bash
make                    # Build with defaults
make SHOW=1             # Show build commands
```

**Windows**:
```cmd
make                    # Invokes projects/windows.bat to setup environment
```

### Build Configurations

#### Debug vs Release
```bash
make OPTIMIZE=debug     # Debug build with symbols
make OPTIMIZE=release   # Optimized release build
```

#### Development vs Production
```bash
make PROFILE=dev        # Development profile
make PROFILE=prod       # Production profile
```

#### TLS Stack Selection
```bash
# Use MbedTLS (default)
make ME_COM_MBEDTLS=1 ME_COM_OPENSSL=0

# Use OpenSSL (recommended)
make ME_COM_OPENSSL=1 ME_COM_MBEDTLS=0
```

#### Feature Toggles
```bash
make ME_COM_CGI=1       # Enable CGI support
make ME_COM_SSL=1       # Enable SSL/TLS
```

### Build Targets

```bash
make                    # Build binaries
make clean              # Remove build artifacts
make install            # Install to system (requires sudo)
make uninstall          # Uninstall from system
make run                # Build and run test server
make test               # Run unit tests
make doc                # Generate documentation
make package            # Create distribution package
make help               # Show all build options
```

### IDE Builds

#### Visual Studio
1. Open `projects/goahead-windows-default.sln`
2. Select Build → Solution
3. For debugging:
   - Right-click "goahead" project → Set as Startup Project
   - Project Properties → Debugging:
     - Working Directory: `$(ProjectDir)\..\..\test`
     - Command Arguments: `-v`

#### Xcode
1. Open `projects/goahead-macosx-default.sln`
2. Product → Scheme → Edit Scheme
3. Build tab: Add all targets
4. Run/Debug tab:
   - Executable: goahead
   - Arguments: `-v`
   - Working Directory: `./test` (absolute path)
5. Product → Build

## Testing Procedures

### Running Tests

#### Run All Tests
```bash
cd test
tm                      # Run entire test suite
```

#### Run Specific Tests
```bash
cd test
tm basic/get.tst.ts     # Run single test
tm basic/               # Run all tests in directory
tm -v                   # Verbose output
```

#### Test from Root
```bash
make test               # Build and run all tests
```

### Test Development

#### Creating a New Test

**TypeScript Test** (preferred for HTTP testing):
```typescript
/* test/basic/example.tst.ts */
import {fetch, test} from 'testme'

test('example test', async function() {
    let response = await fetch('http://127.0.0.1:4100/')
    ttrue(response.ok)
    tequal(response.status, 200)
})
```

**C Test** (for low-level API testing):
```c
/* test/basic/example.tst.c */
#include "testme.h"

static void test_example(void) {
    int result = someFunction();
    ttrue(result == 0);
}

int main(int argc, char **argv) {
    test("example test", test_example);
    return 0;
}
```

**Shell Test** (for simple command testing):
```bash
#!/bin/bash
# test/basic/example.tst.sh

source testme.sh

test "example test" "
    curl -s http://127.0.0.1:4100/ | grep 'Expected'
"
```

#### Test Design Guidelines

1. **Portability**: Tests must run on Windows, macOS, and Linux
2. **Parallel Safety**: Use `getpid()` for unique filenames
3. **Cleanup**: Remove all temporary files
4. **Isolation**: Don't depend on other tests
5. **Assertions**: Use TestMe assertions for clear failure messages

#### Test File Naming
- C tests: `*.tst.c`
- Shell tests: `*.tst.sh`
- JavaScript tests: `*.tst.js`
- TypeScript tests: `*.tst.ts`

### Test Server

The test server (`goahead-test`) is automatically managed by TestMe:
- HTTP Port: 4100
- HTTPS Port: 4443
- Web Root: `./test/`
- Configuration: `./test/testme.json5`

Manual test server control:
```bash
# Start manually
./build/*/bin/goahead-test -v --home test

# Stop
kill $(cat test/goahead.pid)
```

## Code Modification Procedures

### Before Modifying Code

1. **Read existing code** to understand current implementation
2. **Check CLAUDE.md** for project-specific guidelines
3. **Review DESIGN.md** for architectural context
4. **Run tests** to establish baseline

### During Modification

1. **Follow coding conventions**:
   - 4-space indentation
   - 120-character line limit
   - camelCase for functions/variables
   - One blank line between functions
   - Use `/* */` for multi-line comments
   - Use `//` for single-line comments

2. **Use runtime functions**:
   - `slen()` not `strlen()`
   - `scopy()` not `strcpy()`
   - `scmp()` not `strcmp()`
   - `walloc()` not `malloc()`
   - `wfree()` not `free()`

3. **Memory management**:
   - Use `walloc()`, `wrealloc()`, `wfree()`
   - Don't check allocation results (global handler)
   - `wfree(NULL)` is safe

4. **Variable declarations**:
   - Declare at top of function (C89 style)
   - Avoid mixed declarations and code

### After Modification

1. **Format code**: Ensure consistent style
2. **Update comments**: Document all public APIs
3. **Run tests**: `make test` or `cd test && tm`
4. **Update documentation**:
   - Update function documentation if API changed
   - Update DESIGN.md if architecture changed
   - Update README.md if user-facing changes
5. **Test documentation build**: `make doc`

### Code Review Checklist

- [ ] Code follows style conventions
- [ ] All functions are documented
- [ ] Memory management is correct
- [ ] No new compiler warnings
- [ ] Tests pass on all platforms
- [ ] Documentation is updated
- [ ] Security considerations addressed

## Documentation Procedures

### API Documentation

GoAhead uses Doxygen-style comments:

```c
/**
    Brief description of function
    @param name Parameter description
    @param value Parameter description
    @return Return value description
    @stability Stable|Evolving|Prototype
 */
PUBLIC int exampleFunction(char *name, int value);
```

**Important**:
- Don't use `@return Void` (omit @return for void functions)
- Don't use `@defgroup`
- Use `@stability` to indicate API maturity

### Building Documentation

```bash
make doc                # Generate HTML documentation
open doc/index.html     # View documentation (macOS)
```

### Updating Project Documentation

After significant changes, update:

1. **AI/designs/DESIGN.md**: Architectural changes
2. **AI/plans/PLAN.md**: Project status, roadmap updates
3. **AI/logs/CHANGELOG.md**: Record of changes
4. **README.md**: User-facing changes
5. **CLAUDE.md**: AI/developer context updates

## Release Procedures

### Pre-Release Checklist

1. **Code freeze**: No new features, bug fixes only
2. **Full test pass**: Run complete test suite on all platforms
3. **Documentation review**: Ensure all docs are current
4. **Version bump**: Update version numbers in:
   - `main.me` (version field)
   - `src/goahead.h` (VERSION defines)
   - `package.json`

5. **Changelog update**: Update AI/logs/CHANGELOG.md

### Building Release

```bash
make clean
make PROFILE=prod OPTIMIZE=release
make package
```

### Release Artifacts

Generated in `build/*/pkg/`:
- Source package: `goahead-{version}-src.tgz`
- Binary packages: Platform-specific binaries
- Documentation: `goahead-{version}-doc.tgz`

### Publishing Release

```bash
make publish            # Publish to local registry
make promote            # Promote to public (requires credentials)
```

### Post-Release

1. **Tag release**: `git tag -a v{version} -m "Version {version}"`
2. **Push tag**: `git push origin v{version}`
3. **GitHub release**: Create release on GitHub with artifacts
4. **Update documentation site**: Deploy updated docs
5. **Announce**: Post release notes

## Git Workflow

### Commit Message Format

Use prefixes to categorize commits:

- `FIX:` - Bug fixes
- `DEV:` - Features, refactoring
- `CHORE:` - Build, formatting, infrastructure
- `TEST:` - Test-related changes
- `DOC:` - Documentation changes

Examples:
```
FIX: Correct header parsing for whitespace tolerance
DEV: Add IPv6 support to socket layer
CHORE: Update build scripts for Xcode 15
TEST: Convert auth tests to TestMe format
DOC: Update API documentation for websOpen
```

### Commit Best Practices

1. **Atomic commits**: One logical change per commit
2. **Clear messages**: Describe what and why
3. **Test before commit**: Ensure tests pass
4. **No generated files**: Don't commit build artifacts

### Branch Strategy

**Main Branch**: `master`
- Stable, production-ready code
- Only merge tested, reviewed changes

**Feature Branches**: For significant changes
```bash
git checkout -b fix/issue-description
# Make changes
git commit -m "FIX: description"
git push origin fix/issue-description
# Create pull request
```

### Pull Request Process

1. **Create branch** from `master`
2. **Make changes** following procedures
3. **Run tests** to verify
4. **Create PR** with clear description
5. **Code review** (if applicable)
6. **Merge** to master after approval

## Component Procedures

For detailed procedures for specific components, see:
- (Additional component procedure documents to be added as needed)

---

**Last Updated**: 2025-10-27
