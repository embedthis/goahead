# GoAhead Embedded Web Server - Design Overview

## Table of Contents

1. [Introduction](#introduction)
2. [Architecture Overview](#architecture-overview)
3. [Core Components](#core-components)
4. [Memory Management](#memory-management)
5. [Request Processing Flow](#request-processing-flow)
6. [Security Model](#security-model)
7. [Build System](#build-system)
8. [Testing Architecture](#testing-architecture)

## Introduction

GoAhead is a lightweight, embedded web server designed for resource-constrained devices. It is written in ANSI C for maximum portability and provides essential web server capabilities including SSL/TLS, CGI, file upload, authentication, and JavaScript Server Templates (JST).

**Current Status**: GoAhead is in maintenance mode - actively supported with security updates but no new features planned. Users planning new devices should consider the [Ioto Device Agent](https://www.embedthis.com/ioto/).

**Version**: 6.0.5

## Architecture Overview

### Design Philosophy

- **Single-threaded**: No threading or fiber coroutines (unlike newer EmbedThis modules)
- **Event-driven**: Uses select/poll for asynchronous I/O
- **Modular**: Core library (libgo) with optional components
- **Portable**: ANSI C with OS abstraction layer
- **Embeddable**: Designed to be integrated into device firmware

### Threading Model

GoAhead is strictly single-threaded and uses an event loop for concurrency:
- Non-blocking I/O via select() or poll()
- All operations execute in a single event loop
- No mutexes, locks, or thread synchronization required
- Simplified memory management due to single-threaded nature

### Layered Architecture

```
┌─────────────────────────────────────────────┐
│         Application Layer                   │
│     (goahead.c - main executable)           │
├─────────────────────────────────────────────┤
│         Handler Layer                       │
│  (file, CGI, JST, action, upload)           │
├─────────────────────────────────────────────┤
│         HTTP Protocol Layer                 │
│  (http.c, route.c, auth.c)                  │
├─────────────────────────────────────────────┤
│         Socket/TLS Layer                    │
│  (socket.c, mbedtls.c/openssl.c)            │
├─────────────────────────────────────────────┤
│         Platform Abstraction                │
│  (osdep.c - OS abstraction)                 │
└─────────────────────────────────────────────┘
```

## Core Components

### Main Executable
- **goahead.c**: Main server entry point
  - Command-line parsing
  - Server initialization and shutdown
  - Event loop execution
  - Signal handling

### Core Library (libgo)

#### HTTP Protocol
- **http.c**: HTTP request/response processing
  - Request parsing
  - Header management
  - Response generation
  - Keep-alive connection management
  - Chunked transfer encoding

- **route.c**: URL routing and handler dispatch
  - Pattern matching for URLs
  - Handler selection
  - Route configuration via route.txt
  - Redirect handling

#### Content Handlers
- **file.c**: Static file serving
  - MIME type detection
  - Range request support
  - Directory listings
  - File permissions checking

- **cgi.c**: CGI request processing
  - Environment variable setup
  - Process spawning and management
  - Input/output streaming
  - Non-blocking CGI execution

- **jst.c**: JavaScript Server Templates
  - Template parsing
  - JavaScript expression evaluation
  - Server-side scripting

- **js.c**: JavaScript runtime for JST
  - Expression parser
  - Function execution
  - Variable binding

- **action.c**: Action handler for form processing
  - Form parameter parsing
  - Action routing
  - C function callbacks

- **upload.c**: File upload handling
  - Multipart form data parsing
  - Temporary file management
  - Upload size limits

#### Security
- **auth.c**: Authentication and authorization
  - Basic authentication (legacy support)
  - Digest authentication (legacy, MD5-based)
  - File-based user database
  - PAM integration (optional)
  - Role-based access control

- **crypt.c**: Cryptographic utilities
  - Password hashing
  - Message digest functions
  - Base64 encoding/decoding

- **mbedtls.c / openssl.c**: SSL/TLS support
  - Certificate management
  - TLS handshake
  - Encrypted I/O
  - Cipher suite configuration

#### Socket Layer
- **socket.c**: Network socket management
  - Connection handling
  - Non-blocking I/O
  - IPv4 and IPv6 support
  - Listen socket management (up to WEBS_MAX_LISTEN endpoints)

#### File System
- **fs.c**: File system abstraction
  - Open/read/write/close operations
  - ROM-based file system support
  - Custom file system handlers
  - Path validation and security

- **rom.c**: ROM-based file system
  - Compiled-in web content
  - Zero-copy serving
  - Minimal memory footprint

#### Utilities
- **runtime.c**: Core runtime functions
  - String manipulation (sclone, slen, scmp, etc.)
  - Buffer management
  - Time utilities
  - Error handling

- **alloc.c**: Memory allocation
  - walloc() family (wrapper for malloc)
  - Optional custom allocator replacement
  - Memory tracking in debug builds

- **options.c**: Configuration management
  - Option parsing
  - Default values
  - Runtime configuration

## Memory Management

### Allocation Strategy
- Uses `walloc()`, `wrealloc()`, `wfree()` instead of standard malloc functions
- Custom allocator can replace default via ME_GOAHEAD_REPLACE_MALLOC
- Global memory handler catches allocation failures
- No explicit NULL checks after allocation (handled globally)

### Memory Safety
- `wfree(NULL)` is safe (no-op)
- `sclone(NULL)` returns empty string, never NULL
- Null-tolerant APIs throughout
- Defensive programming practices

### ROM Support
- Can operate with web content compiled into ROM
- Zero-copy serving from ROM
- Minimal heap usage for static sites

## Request Processing Flow

### Incoming Request Flow

```
1. Socket Accept
   └─> socket.c: websAccept()

2. Request Parsing
   └─> http.c: websGetInput()
       ├─> Parse request line (method, URI, version)
       ├─> Parse headers
       └─> Validate request

3. Route Matching
   └─> route.c: websRouteRequest()
       ├─> Match URI pattern
       ├─> Check authentication requirements
       └─> Select handler

4. Authentication (if required)
   └─> auth.c: websAuthenticate()
       ├─> Check credentials
       └─> Verify permissions

5. Handler Dispatch
   └─> route.c: Call selected handler
       ├─> file.c: websFileHandler() for static files
       ├─> cgi.c: websCgiHandler() for CGI
       ├─> jst.c: websJstHandler() for JST
       ├─> action.c: websActionHandler() for actions
       └─> upload.c: websUploadHandler() for uploads

6. Response Generation
   └─> http.c: websResponse()
       ├─> Set status code
       ├─> Set headers
       └─> Send body

7. Connection Management
   └─> Keep-alive or close based on headers
```

### Event Loop

```c
while (serverRunning) {
    // Wait for I/O events (select/poll)
    websSelect(timeout);

    // Process ready sockets
    for (each ready socket) {
        if (listening socket) {
            websAccept();
        } else if (readable) {
            websGetInput();
            websRouteRequest();
        } else if (writable) {
            websFlush();
        }
    }

    // Run periodic tasks
    websRunEvents();
}
```

## Security Model

### Authentication Mechanisms
1. **Basic Authentication**: Legacy support, credentials in clear text (base64)
2. **Digest Authentication**: Legacy support, MD5-based challenge/response
3. **Form Authentication**: Custom form-based login

**Note**: Basic and Digest are discouraged but maintained for legacy compatibility. Modern deployments should use TLS + form authentication.

### Authorization
- Role-based access control (RBAC)
- Route-level permission checking
- User database (auth.txt format)

### TLS/SSL
- MbedTLS or OpenSSL support
- Configurable cipher suites
- Certificate validation
- SNI support (Server Name Indication)

### Input Validation
- URL path traversal prevention
- Header validation
- Content-length enforcement
- Upload size limits
- CGI parameter sanitization

### Security Assumptions
- **SECURITY Acceptable**: MD5 used only for legacy Digest auth
- **SECURITY Acceptable**: Basic/Digest auth maintained for backward compatibility
- **SECURITY Acceptable**: Test certificates (self.crt/self.key) for development only
- **SECURITY Acceptable**: Debug builds may log sensitive data via trace functions
- File system integrity is out of scope (assumed secure)
- DNS integrity is out of scope
- Developers responsible for validating inputs to embedded APIs
- Developers responsible for secure configuration

## Build System

### MakeMe-Based Build
- Makefiles generated by MakeMe internal tool
- Located in projects/ directory
- Platform-specific: linux, macosx, windows, etc.
- Top-level makefile chains to platform-specific makefile

### Build Targets
```bash
make              # Build with defaults
make clean        # Clean build artifacts
make install      # Install to system
make run          # Run test server
make test         # Run unit tests
make doc          # Generate documentation
make package      # Create distribution package
```

### Configuration Options
- `OPTIMIZE=debug|release`: Optimization level
- `PROFILE=dev|prod`: Development vs production
- `SHOW=1`: Display build commands
- `ME_COM_MBEDTLS=1 ME_COM_OPENSSL=0`: TLS stack selection
- `ME_COM_CGI=1`: Enable CGI support

### IDE Support
- Visual Studio: `projects/goahead-windows-default.sln`
- Xcode: `projects/goahead-macosx-default.sln`

## Testing Architecture

### Test Framework
- Uses **TestMe** test runner (tm command)
- Test files located in `./test/` directory
- Test naming convention: `*.tst.{c,sh,js,ts}`

### Test Types
1. **C Unit Tests**: `*.tst.c`
   - Include `testme.h` header
   - Test HTTP protocol, handlers, utilities

2. **Shell Tests**: `*.tst.sh`
   - Bash-based functional tests
   - Server integration tests

3. **JavaScript/TypeScript Tests**: `*.tst.{js,ts}`
   - Client-side testing
   - HTTP request/response validation

### Test Infrastructure
- **Test Server**: `goahead-test` (built from test.c)
- **Test Ports**: 4100 (HTTP), 4443 (HTTPS)
- **Test Content**: `./test/` web root
- **CGI Test Program**: `cgitest`
- **Configuration**: `./test/testme.json5`

### Test Execution
```bash
cd test
tm              # Run all tests
tm NAME         # Run specific test
tm -v           # Verbose output
```

### Test Design Principles
- Tests must run in parallel (use getpid() for unique filenames)
- Tests must be portable (Windows, macOS, Linux)
- Prefer C tests for C code testing
- Use shell tests for simple command validation
- Clean up all temporary files

## Component Documentation

For detailed designs of specific components, see:
- (Additional component design documents to be added as needed)

---

**Last Updated**: 2025-10-27
**Version**: 6.0.5
