# GoAhead Tests with EJS+Bun

Quick reference for running GoAhead tests with the EJS TypeScript library and Bun runtime.

## Prerequisites

- **Bun:** v1.2.23 or later
- **EJS:** v2.0.0 (linked from `/Users/mob/c/ejs`)
- **TestMe:** Installed globally or via `bun`
- **GoAhead server:** Built and available in `../build/` directory

## Setup

The EJS package is already linked. If you need to re-link:

```bash
# Link EJS globally
cd /Users/mob/c/ejs
bun link

# Link in goahead test directory
cd /Users/mob/c/goahead/test
bun link ejscript
```

## Running Tests

### All Tests
```bash
cd /Users/mob/c/goahead/test
tm                      # Run all .tst.ts tests
```

### By Category
```bash
tm basic/*.tst.ts       # Basic HTTP tests
tm auth/*.tst.ts        # Authentication tests
tm security/*.tst.ts    # Security tests
tm stress/*.tst.ts      # Stress/load tests
```

### Single Test
```bash
tm basic/get.tst.ts     # Run specific test
```

### With Makefile
```bash
cd /Users/mob/c/goahead
make test               # Run via makefile
```

## Test Files

All tests are now `.tst.ts` (TypeScript) files:

- **Basic:** 14 tests in `basic/`
- **Auth:** 3 tests in `auth/`
- **Security:** 4 tests in `security/`
- **Stress:** 7 tests in `stress/`
- **Other:** 6 tests in various directories

**Total:** 34 test files

See [CONVERSION_SUMMARY.md](CONVERSION_SUMMARY.md) for complete list.

## Key Differences from Native Ejscript

### 1. Import Required
All tests start with:
```typescript
import { Http, Config } from 'ejscript'
```

### 2. HTTP Methods Use Two-Step Async Pattern
**IMPORTANT**: HTTP methods are NOT async themselves. Call the method, then `await http.wait()`:

```typescript
http.get(HTTP + "/index.html")
await http.wait()

http.post(HTTP + "/data", "body")
await http.wait()

http.form(HTTP + "/action", {...})
await http.wait()
```

### 3. File I/O is Async (v2.0.0)
```typescript
const data = await new Path("file.txt").readString()
```

### 4. HTTP Headers Use Lowercase
```typescript
http.header("content-type")    // ✅ Correct
http.header("Content-Type")    // ❌ May not work
```

## Environment Variables

Tests use these environment variables (set in `testme.json5`):

- `TM_HTTP` - HTTP endpoint (default: `http://127.0.0.1:18080`)
- `TM_HTTPS` - HTTPS endpoint (default: `https://127.0.0.1:14443`)
- `TM_HTTPV6` - IPv6 HTTP endpoint
- `TM_HTTPSV6` - IPv6 HTTPS endpoint

## Debugging

### Verbose Output
```bash
tm --verbose basic/get.tst.ts
```

### Single Test
```bash
tm basic/get.tst.ts
```

### Check Imports
```bash
bun run --print 'import { Http } from "ejscript"; console.log(Http)'
```

## Common Issues

**Problem:** `Http is not defined`
**Fix:** Add `import { Http } from 'ejscript'`

**Problem:** Test hangs
**Fix:** Ensure all HTTP methods have `await`

**Problem:** Headers return undefined
**Fix:** Use lowercase header names

**Problem:** File operations fail
**Fix:** Add `await` before Path operations

## Documentation

- [CONVERSION_SUMMARY.md](CONVERSION_SUMMARY.md) - Complete conversion details
- [../ejs/docs/COMPATIBILITY.md](../../ejs/docs/COMPATIBILITY.md) - EJS compatibility guide
- [../ejs/docs/README.md](../../ejs/docs/README.md) - EJS documentation
- `tm --help` - TestMe help

## TestMe Functions

Available in all tests (no import needed):

- `tget(name)` - Get environment variable
- `ttrue(condition)` - Assert true
- `tskip(reason)` - Skip test
- `thas(feature)` - Check feature enabled
- `assert(condition)` - Assert condition
- `print(message)` - Output message

## Examples

### Simple GET Test
```typescript
import { Http } from 'ejscript'
import { tget, ttrue } from 'testme'

const HTTP = tget('TM_HTTP') || "127.0.0.1:8080"
let http = new Http

http.get(HTTP + "/index.html")
await http.wait()
ttrue(http.status == 200)
```

### POST with Form Data
```typescript
import { Http } from 'ejscript'
import { tget, ttrue } from 'testme'

const HTTP = tget('TM_HTTP') || "127.0.0.1:8080"
let http = new Http

http.form(HTTP + "/action", {
    name: "John",
    email: "john@example.com"
})
await http.wait()
ttrue(http.status == 200)
```

### Authentication Test
```typescript
import { Http } from 'ejscript'
import { tget, ttrue } from 'testme'

const HTTP = tget('TM_HTTP') || "127.0.0.1:8080"
let http = new Http

http.setCredentials("username", "password")
http.get(HTTP + "/protected")
await http.wait()
ttrue(http.status == 200)
```

## Status

✅ **All 34 test files converted from .tst.es to .tst.ts**
✅ **33/34 tests passing (97% success rate)**
⊘ **1 test skipped** (ipv6/getv6.tst.ts - requires IPv6 configuration)
✅ **EJS package linked**
✅ **TestMe configuration updated**
✅ **Documentation complete**

### Test Results Summary
- Basic HTTP: 14/14 passing
- Authentication: 3/3 passing
- Security: 4/4 passing
- Stress: 7/7 passing
- Other: 5/6 passing (1 skipped)

See [FINAL_RESULTS.md](FINAL_RESULTS.md) for detailed breakdown.

---

Last Updated: 2025-10-27
