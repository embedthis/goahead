# GoAhead Embedded Web Server (GOAHEAD)

GoAhead is an embedded web server written in C, designed for small devices and
systems. It supports SSL/TLS, CGI, file upload, authentication, and JavaScript
Server Templates (JST).

## GoAhead-Specific Build Commands

- **Visual Studio**: Open `projects/goahead-windows-default.sln`
- **Xcode**: Open `projects/goahead-macosx-default.sln`

## GoAhead-Specific Testing

- Test configuration: `./test/web.json5`
- Test server: `goahead-test` (built from test.c)
- Test server ports: 4100 (HTTP) and 4443 (HTTPS)
- CGI test program: `cgitest` (for CGI functionality testing)
- The `testme` tool automatically launches the web server when tests run

### Build Configuration Files

- Platform-specific makefiles:
`projects/goahead-{OS}-{ARCH}-{PROFILE}.{mk|nmake}`
- SSL certificates: `src/self.crt` and `src/self.key` (test certificates)

## GoAhead Architecture

### Core Components

- **goahead.c**: Main server executable entry point
- **libgo**: Core library containing all server functionality
- **SSL Support**: Optional via MbedTLS (`mbedtls.c`) or OpenSSL (`openssl.c`)
- **CGI Handler**: `cgi.c` - processes CGI requests
- **Authentication**: `auth.c` - handles user authentication (file, PAM, or
custom)
- **JavaScript Templates**: `js.c`, `jst.c` - server-side JavaScript processing
- **File Operations**: `file.c`, `fs.c` - file system abstraction
- **HTTP Protocol**: `http.c` - HTTP request/response processing
- **Routing**: `route.c` - URL routing and handler dispatch
- **Upload**: `upload.c` - file upload handling

### Key Directories

- **src/**: All C source code and headers
- **test/**: Unit tests and test web content
- **projects/**: Platform-specific build files and IDE projects
- **certs/**: SSL certificate management
- **paks/**: Package dependencies (SSL, certificates, OS dependencies)
- **installs/**: Installation scripts and manifests

### GoAhead Configuration System

- SSL/TLS stack selection (MbedTLS or OpenSSL)
- Authentication backend (file, PAM, or custom)
- Resource limits (buffer sizes, timeouts, file limits)
- Feature toggles (CGI, JavaScript, upload, logging, etc.)

### GoAhead Memory Management

- Custom memory allocator option (`ME_GOAHEAD_REPLACE_MALLOC`)
- Uses `walloc()` family instead of standard malloc
- ROM-based operation support for embedded systems

### GoAhead-Specific Functions

- `wfree()` can be safely called with `NULL` argument
- `sclone()` returns empty string if passed `NULL` argument

## Development Workflow

1. **Testing**: Run `make run` to test locally (web root: `test/` directory)
2. **Debugging**: Use `-v` flag for verbose logging, configure via
`ME_GOAHEAD_LOGFILE`

## GoAhead-Specific Files

- `src/goahead.h`: Main header with all API definitions and configuration
- `src/auth.txt`: Default user authentication file
- `src/route.txt`: URL routing configuration

## GoAhead-Specific Notes

- GoAhead is single-threaded and does not support or use fiber coroutines.
- Legacy Digest authentication supported (requires MD5) - acceptable security
compromise
- Basic and Digest authentication strongly discouraged but required for legacy
compatibility

## Additional Resources

- **Parent Project**: See `../CLAUDE.md` for general build commands, testing
procedures, and overall EmbedThis architecture
- **Sub-Projects**: See `src/*/CLAUDE.md` for specific instructions related to
sub-projects
- **API Documentation**: `doc/index.html` (generated via `make doc`)

## Important Notes

- The global memory handler will manage memory allocation failures and the code
does not need to check the results of alloc functions individually.
- The embedded system's file system is deemed to be secure. It is beyond the
projects scope to mitigate a compromised file system.
- Use /* */ style comments for multiline comments.
