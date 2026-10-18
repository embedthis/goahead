# GoAhead v6.2.0 Release Notes


**Release Date**: [TODO: release date]

## Overview

GoAhead v6.2.0 improves security, request handling and reliability. It addresses findings from the internal security audit and externally reported defects.

The main changes are stricter route checks, stronger authentication, safer file access, opt-in uploads, POST-only actions and tighter HTTP, CGI and TLS validation. Applications can also drop privileges after opening listeners.

Upgrading is recommended for all deployments. Several defaults have changed: review [Breaking and Behavior Changes](#breaking-and-behavior-changes) before upgrading. The new `SECURITY-DEPLOYMENT.md` guide at the root of the distribution explains secure deployment and configuration.

## What's New

### Authentication and sessions

- **Privilege drop:** `--user`, `--group`, `ME_GOAHEAD_USER`, `ME_GOAHEAD_GROUP` and `websDropPrivileges()` switch to an unprivileged account after opening listeners. The change is verified as irreversible; failure stops the server. CGI programs use the same account. Running as root produces a warning.
- **Stronger password hashes:** new `BF2` hashes use a fixed-width digest of `username:realm:password`, preventing long usernames or realms from excluding the password. Unsafe legacy `BF1` hashes are rejected, and `websAddUser()` reports accounts that need new passwords. Oversized password inputs are rejected instead of truncated.
- **Digest validation:** credentials must match the request target and realm. Nonce counts must increase and cannot be reused, including on 32-bit and Windows builds. Only MD5 password hashes are accepted; Digest with PAM is rejected when routes load. Verification uses the current credentials rather than a user left by an earlier route.
- **Digest parsing and expiry:** full-length credential inputs, quoted pairs and empty nonces are handled correctly. Challenges no longer advertise `domain="null"`. Nonces must match exactly, expire after 300 seconds and return `stale="TRUE"` so clients can retry. Replay records are capped at 512 (`ME_GOAHEAD_LIMIT_NONCES`) and pruned by a timer.
- **Time handling:** session and Digest nonce expiry no longer depends on the width of the platform `time_t`, so they keep expiring after 2038 on 32-bit targets. If the system clock fails or cannot represent the current time, sessions and Digest nonces are refused instead of living forever, and `websOpen()` refuses to start. A build whose `time_t` is narrower than 64 bits logs a startup warning that sessions and Digest authentication stop working in 2038. 32-bit Linux builds with glibc 2.34 or later now select a 64-bit `time_t` by default; define `_TIME_BITS=32` to opt out.
- **Cross-site protection:** every request other than GET or HEAD returns 403 when `Origin` names another site, on any route, so login POSTs and client-certificate deployments are protected as well as authenticated routes. Without `Origin`, a `Sec-Fetch-Site` value other than `same-origin` or `none` marks the request as cross-site. Requests with neither header are unaffected. The route option `cross-site=allow` accepts cross-site requests on one route. Login reads credentials only from the body and redirects only to a local referrer path.
- **Reverse proxies:** `ME_GOAHEAD_TRUSTED_PROXIES`, `websSetTrustedProxies()` and the `--proxy` option name proxies whose right-most `X-Forwarded-Proto` value is trusted. That scheme decides the session cookie `secure` attribute, HSTS, the redirect scheme, `ME_GOAHEAD_SECURE_ONLY`, `protocol=` matching, `REQUEST_TRANSPORT` and the cross-site check, so same-site requests behind a TLS-terminating proxy are no longer refused. Forwarded headers from other peers are ignored. Trusted proxies are exempt from the per-address connection cap. None are trusted by default.
- **Route authentication:** authentication gates without `methods=` check every method. Rewritten requests are decoded, normalized and checked again from the first route. Authentication failures return a 401 challenge when no redirect is configured.
- **Login correctness:** failed logins clear the claimed identity; repeated `websLoginUser()` calls verify their own passwords and safely handle aliased arguments. Basic login timing no longer reveals whether a Blowfish account exists. Removing a user clears references from active requests.
- **Session isolation:** sessions apply only to the authentication protocol that created them. Basic and Digest reuse an existing client session rather than creating sessions for clients that ignore cookies.
- **Session limits:** each account can hold eight sessions by default; a new login retires its least recently used session. Expired sessions are rejected at lookup and pruned before refusing a login because the table is full. Allocation failures and repeated retirement no longer corrupt session counts. Random-source failure rejects the login. Values returned by `websGetSessionVar()` stay valid until the request ends even when the session is rotated or destroyed, and concurrent logouts of one session no longer leak it.
- **Cookies:** session cookies are host-only, expiry dates use GMT, and invalid cookie attributes are rejected. Empty cookie segments no longer crash the server. `websGetSessionID()` always returns an allocated copy.
- **Configuration and tools:** custom password verifiers apply to routes loaded by `websOpen()`. `abilities=a|b` accepts either ability, and `abilities=none` adds no requirement. PAM uses the expected allocator. `gopass` rejects empty passwords, checks confirmation and safely replaces the auth file even when `/tmp` is on another file system. Test accounts are labelled as unsafe for deployment.

### HTTP requests and responses

- **Request validation:** request lines require exactly three parts separated by single spaces. Methods must be valid tokens within the length limit (400 otherwise), retain their case and return 501 if unknown. Standard methods and methods listed in route configuration are recognized. Well-formed versions other than HTTP/1.0 and HTTP/1.1 return 505; malformed versions return 400. HTTP/1.1 requests without `Host` return 400.
- **Header validation:** duplicate `Host`, `Authorization` and `Content-Type` headers are rejected; repeated cookies are joined with `;`. Whitespace is limited to spaces and tabs. Headers named by `Connection` are withheld from handlers and CGI, except protected headers such as `Origin` and `Sec-Fetch-Site`, matched by their variable name. Nonportable header names cannot create conflicting `HTTP_` variables.
- **Body framing:** chunk sizes and extensions follow RFC 9112, independent of TCP segmentation, with overflow checks. Chunked bodies stream to consumers instead of being buffered whole. A `Content-Length` body larger than the input buffer returns 413. `Content-Length` and chunk sizes are parsed without overflow at any configured limit; an over-limit length returns 413. In legacy builds, chunked bodies no longer replace the query string.
- **Connection handling:** keep-alive is recalculated for each request. `Connection: close` on any header line closes the connection, even if another line asks for keep-alive. Early responses, 401 responses and form-login redirects close the connection. Requests cannot append a second response or mistake the end of a chunk for the end of the body. Lingering close lets clients receive errors before disconnection.
- **Response framing:** unknown-length HTTP/1.0 responses use connection close. HEAD, 1xx, 204 and 304 responses have no body or chunked framing; HEAD declares the corresponding GET length. TRACE declares its echoed body length. Header lines are written whole or omitted.
- **Request targets:** absolute URLs supply their own authority and must begin with `http://` or `https://`. Missing hosts and `userinfo@` are rejected. A `://` inside a path cannot bypass the Origin check. Pathless absolute URLs route to `/`; unsupported target forms are rejected. A `#` fragment in the request target returns 400 instead of becoming part of the query.
- **Path validation:** `+` stays literal in paths. Control characters, invalid UTF-8 and residual encoded separators or dots (`%2e`, `%2f`, `%5c`) are rejected. On case-insensitive file systems (`ME_GOAHEAD_CASE_INSENSITIVE_FS`), Unicode characters that fold to ASCII are rejected to prevent authentication bypass and template-source disclosure.
- **Forms and parameters:** query and form parameters are capped at 512 by default (413 when exceeded). Media types are matched exactly and case-insensitively for forms, JSON and multipart bodies. Repeated form keys are joined in linear time. Query parameters, form fields and multipart text fields cannot create, replace or extend `HTTP_` header variables, upload variables or other server-set variables.
- **Timeouts:** requests have a 300-second deadline by default. `ME_GOAHEAD_LIMIT_TIMEOUT` covers total blocking write time, preventing slow readers from resetting it on each flush. TLS traffic without request data cannot extend the idle timeout; connections that never start a request close without a 408.
- **Event-loop responsiveness:** error responses drain asynchronously. Socket cleanup bounds the input it drains, preventing a continuously sending peer from holding the server. Platforms without `SO_SNDTIMEO` log a warning once.
- **Caching:** public caching is opt-in and limited to static GET/HEAD responses with status 200 or 304 and no authentication, session or cookie. Other responses use `Cache-Control: no-store, private` and `Vary: Authorization, Cookie`.
- **Response headers:** responses with bodies include `Content-Type` (default `application/octet-stream`) and `X-Content-Type-Options: nosniff`. Use `ME_GOAHEAD_RESPONSE_HEADERS` or `websAddResponseHeader()` for additional headers. `ME_GOAHEAD_HSTS` and `ME_GOAHEAD_SECURE_ONLY` enable HSTS and HTTP-to-HTTPS redirects.
- **Conditional requests:** static files have weak ETags. `If-None-Match` takes precedence over `If-Modified-Since`; invalid dates are ignored. Date calculations use checked fields and 64-bit arithmetic, including dates from 2038 onward.
- **Large files:** files over 4 GB have correct response lengths on 32-bit builds. Lengths come from the open file descriptor; a short read closes the connection.
- **Redirects:** directory paths are encoded, header control bytes are rejected, and scheme matching starts at the beginning of the target. Redirect hosts use `websSetHostUrl()` or the accepting interface's address. HTTPS redirects handle absolute-form paths and ports correctly when TLS terminates elsewhere.

### Files and routing

- **Strict routes by default:** the first `websListen()` checks the complete route table, including application-added routes and routes from the auth file. Bypassable tables prevent startup. An entirely unauthenticated table warns but is allowed. Shipped examples pass the check.
- **Route validation:** invalid authentication keywords and selectors are rejected when loading routes. Checks detect unauthenticated routes that could bypass protected routes, including later routes nested under a gate whose `methods=`, `protocol=` or `extensions=` do not cover them, and routes behind a `handler=continue` gate whose `dir=` does not protect them. A `methods=` list that admits GET also admits HEAD. Rewrites with a query replace both the query and its variables.
- **Route removal:** `websRemoveRoute()` lets requests already using the route finish on it; the route is freed when the last one ends.
- **Action isolation:** an action named by a route runs only through that route. The generic `/action` route cannot reach actions protected by their own routes.
- **Template protection:** the file handler returns 404 for `.jst` and `.asp`. Template routes cover the document root so templates outside `/jst` are not served as source.
- **File protection:** hidden and backup files, and everything under backup directories, return 404 by default. On POSIX, files resolving outside the document or route directory, including through symbolic links, return 404, as do paths that cannot be resolved for any reason other than not existing. Empty document roots are rejected at startup.
- **Windows files:** GET, PUT and DELETE reject reserved device names, including alternate streams and superscript COM/LPT digits. File metadata describes the same object that is opened.
- **ROM files:** concurrent downloads have independent read positions. `webcomp` records source modification times and content hashes, so changed pages of the same size no longer return stale 304 responses.

### CGI

- **Child isolation:** POSIX children close descriptors above stderr and restore default signals. Windows children inherit only stdin and stdout, run without a console window and send stderr to the server's stderr.
- **Environment safety:** only portable variable names are exported. Interpreter-variable blacklists are expanded, including Windows-specific names. Logging records variable names at level 5 rather than values at level 0. `AUTH_TYPE` reflects the protocol that authenticated the request.
- **Request data:** `REQUEST_URI` preserves the raw target. Rerouted requests deliver their bodies on stdin; `CONTENT_LENGTH` matches stdin, including zero on `upload=true` routes.
- **Program selection:** programs resolve under the matched route prefix and must satisfy its `extensions=`. Relative `dir=` paths resolve before changing directory. ISINDEX arguments must be plain words. Overlong paths return 414.
- **Response parsing:** header parsing handles LF and CRLF boundaries and empty header blocks, rejects bare CR and control bytes, and drops hop-by-hop and framing headers. NPH status lines are honored; `Location` alone produces 302. Content type comes from the program or defaults to `text/plain`.
- **Launch and cleanup:** failed exec returns 502; failed `dup2` returns 500. Launch errors no longer leak descriptors, memory or staging files, leave the working directory changed, or flush the embedding application's buffered stdout. Reaped children and Windows programs with no output no longer stall requests. On POSIX, each CGI program runs in its own process group and a timeout kills the whole group.
- **Working directory:** each CGI child is started in its route directory; the server no longer changes its own working directory. A route `dir=` that does not exist returns 404.
- **Lengths and platforms:** CGI keep-alive works with declared content lengths, and length accounting is safe on 32-bit targets. Windows command-line builders take destination sizes. VxWorks keeps pointer-sized task IDs and derives the entry symbol from the program name.
- **VxWorks:** a CGI module stays loaded while any task runs it and is reloaded once none does. Tasks have unique names and are verified by ID and name, the server owns and closes their standard descriptors, and they run at `ME_GOAHEAD_CGI_PRIORITY` or, by default, one step below the server.

### TLS

- **Library minimums:** builds require OpenSSL 3.0 or later, or MbedTLS 3.6.7 or later. Use a maintained TLS library release.
- **Certificates:** HTTPS endpoints require a loaded certificate and key. Full certificate chains and OpenSSL-supported key types, including EC, load correctly; invalid chain certificates fail the load.
- **Client verification:** OpenSSL trusts only `ME_GOAHEAD_SSL_AUTHORITY`, without falling back to system CAs. CRL checks cover the full client chain. Both backends support `ME_GOAHEAD_SSL_VERIFY_DEPTH`.
- **Ciphers and sessions:** configured ciphers apply to TLS 1.2 and 1.3, excluding static RSA and weak ciphers. Client-certificate authentication disables session resumption. OpenSSL applies `ME_GOAHEAD_SSL_TIMEOUT` and rotates ticket keys at that interval.
- **MbedTLS:** runtime certificates and keys, client-certificate policy, version switches, partial writes, writable events, seeded key parsing and error reporting are corrected. Obsolete TLS settings are removed. MbedTLS now uses the same default cipher list and TLS 1.2 cipher floor as OpenSSL, refuses a client chain through a CA with no CRL in `ME_GOAHEAD_SSL_REVOKE`, and bounds an over-long cipher list. OpenSSL sends a close_notify alert when it closes a healthy connection.
- **Keys and configuration:** MbedTLS refuses to start with a certificate and key that do not match. An encrypted private key is reported and refused instead of prompting on the console. Building with `ME_GOAHEAD_SECURE_ONLY` but no TLS, or with both TLS 1.2 and 1.3 disabled, fails. OpenSSL sets minimum and maximum protocol versions like MbedTLS. Basic, Digest and form authentication on a build without TLS log a warning per route, and a client-verification setup that names an authority or CRL without issuer verification logs a warning.
- **Client certificates:** OpenSSL accepts valid client certificates that carry only a subject alternative name.
- **Failure handling:** fatal writes close the connection. Writes on a connection whose handshake has not finished are refused and the connection is closed, so they can never block the server. Failure to read startup randomness prevents startup.

### JST templates

- Evaluations have step (`ME_GOAHEAD_LIMIT_SCRIPT_STEPS`), result-size (`ME_GOAHEAD_LIMIT_SCRIPT_RESULT`) and recursion (`ME_GOAHEAD_LIMIT_SCRIPT_RECURSE`) limits. Invalid tokens in brace blocks no longer hang the parser.
- New `writeHtml`, `escape` and `escapeUri` builtins support output escaping. The new `escapeJs` builtin and `websEscapeJs()` escape values for JavaScript strings, including inline scripts and event handlers. The documentation names the safe context for each escaper.
- Conditions in `if`, `for`, `||` and `&&` follow one numeric truth rule: empty is false, a number is true when non-zero, and any other value is a script error.
- Numeric comparisons use numeric values. Mixing numbers with non-numbers, overflowing arithmetic and invalid shifts produce script errors, including `INT_MIN / -1`.
- Script errors return 500 and log details without exposing template source. Loop, return and increment memory leaks are fixed.

### Uploads and PUT

- Multipart uploads require `upload=true` and are parsed after route authentication.
- Empty part names return 400. Repeated names and content types release earlier allocations and staging files. Fields split across reads are delivered whole.
- Missing closing delimiters return 400; a closing delimiter needs no final CRLF. Names and values remain literal. Boundaries are parsed as case-insensitive Content-Type parameters, accept token or quoted-string syntax, and are limited to 70 characters. A delimiter is recognized only as a complete delimiter line; boundary text elsewhere is data, and a delimiter line with any other suffix returns 400. Large text fields are scanned once rather than repeatedly.
- Uploads are capped at 16 files and 512 parts by default; exceeding either limit returns 413.
- PUT returns 201 for a new file and 204 for a replaced one. A PUT to a directory returns 409 before the body is staged.
- Only the file handler stages PUT bodies, after path checks. Failed or rejected requests leave no document or staging file. Temporary-directory containment respects file-system case rules and compares directories by their resolved absolute form, so `./web` and `WEB` name the same directory as `web`.
- A client upload filename is checked exactly as sent. Names with dot segments, a trailing dot or, on Windows, a reserved device name return 400 instead of being rewritten or accepted.

### Memory and runtime safety

- Fixed memory leaks in response headers, buffers and CGI cleanup; out-of-bounds Base64 and formatter access; signed-shift undefined behavior in hashing; and route-array bounds errors.
- Formatting caps width and precision at `ME_GOAHEAD_LIMIT_FORMAT_FIELD`, bounds float output, supports `%z` and 64-bit `%ll`, handles `INT64_MIN`, and cannot stall on a full buffer. Ring buffers are made contiguous before flat reads; buffer limits use `ssize`.
- Character classification and socket-mask shifts use unsigned values. Accept lengths use the correct type, interrupted reads retry, and empty resolver results are rejected.
- Timeouts use 64-bit arithmetic and recover after backward clock changes. Descriptor exhaustion no longer spins the listener; failed accepts and listens clean up correctly. A failed `select()` is no longer treated as readiness, and macOS accepts descriptors at or above `FD_SETSIZE`.
- Shutdown no longer reads past the connection and socket tables, and it now frees the TLS state of open connections.
- `SIGHUP`, `SIGINT` and `SIGTERM` stop the server cleanly at any time, including during startup. The `goahead` program sets a umask of 027. `--background` keeps the working directory so relative paths still resolve, and warns that it discards logging to stdout or stderr.
- `wrealloc()` preserves its original allocation on failure. The replacement allocator (`ME_GOAHEAD_REPLACE_MALLOC`) enables its fallback during lazy initialization and rejects requests over 1 GiB. Allocation failures call the memory notifier and then abort; custom notifiers must not return.
- Critical assertions are runtime checks. `ME_GOAHEAD_ASSERT_ABORT` can enable aborts for debug assertions. Empty hash names are rejected, and an unresolved local hostname no longer aborts `websOpen()`.
- `websOpen()` handles SIGPIPE unless the application has its own handler; `websClose()` restores the default. On POSIX, closed standard descriptors are reserved on `/dev/null`.
- Log control bytes are escaped as `\xHH`, including messages passed to custom handlers, preventing forged log lines.

### Platforms, builds and documentation

- Windows uses the dynamic C runtime and binary-mode logs. Empty-string indexing in event logging and MbedTLS debugging is fixed. VxWorks rejects unresolved names and non-IPv4 host addresses. `ME_ROM` builds compile again. On Windows, `websHardenProcess()`, called first by the shipped programs, restricts the DLL search path to the system and application directories. `ME_GOAHEAD_AUTO_LOGIN` without `ME_GOAHEAD_LOGGING` fails the build.
- Release builds add stack, control-flow and executable protections where supported. POSIX builds use `_FORTIFY_SOURCE=3` and `-fstack-protector-strong`; Windows uses Control Flow Guard. FreeBSD adds RELRO, BIND_NOW, a non-executable stack and PIE; Linux marks the stack non-executable; macOS release binaries are stripped.
- More compiler diagnostics are enabled or treated as errors. CI checks release builds, runs the sanitized suite on every push, rejects skipped default-on tests and checks that `dist/` matches `src/`. CI static analysis fails on any finding not in a reviewed baseline.
- `make test-linux` runs tests in a Linux container from macOS. `make test-asan` runs AddressSanitizer and UndefinedBehaviorSanitizer tests on Linux.
- Source distributions omit private keys; tests generate temporary keys. Release archives omit host extended attributes and include minisign signatures.
- The source archive ships only the current test certificates (`ca.crt`, `ec.crt`, `self.crt`, `test.crt`); the expired EC test certificate is replaced and the unused root CA bundles are no longer included. Packaging fails if a shipped certificate expires within a year. The SBOM is corrected for this release, adding the optional PAM library and file hashes.
- The new deployment guide covers strict mode, containment, template escaping, slow readers and front-end proxies. Security settings, API documentation and command-line help are expanded or corrected. Route examples clarify authentication and method checks; sample handlers escape request data.

## Breaking and Behavior Changes

Review these changes before upgrading. Rebuild embedding applications against the 6.2.0 headers.

### Routes and requests

- **Strict route checks are on.** Fix each bypassable route reported at startup. To opt out, use `--no-strict`, `websSetStrict(0)` or `ME_GOAHEAD_STRICT=0`. After changing an `ME_GOAHEAD_*` define, run `make clean` before rebuilding. `--verbose` confirms "Strict mode on".
- **Correct invalid route settings.** Invalid authentication keywords, protocols other than `http` or `https`, lowercase method names, and extensions with uppercase letters or leading dots prevent route loading. `abilities=a|b` means either ability; use `,` for both.
- **Authentication gates check every method.** A gate without `methods=` still serves GET, HEAD and POST, but also authenticates requests that continue to later PUT or DELETE routes.
- **Actions default to POST.** Other methods return 405 with `Allow: POST` unless explicitly configured. Login credentials must be in the body. Actions protected by their own route must be called through that route.
- **Cross-site requests return 403.** Any request other than GET or HEAD, on any route, returns 403 when `Origin` names another site, or when `Origin` is absent and `Sec-Fetch-Site` is neither `same-origin` nor `none`. Requests with neither header are unaffected. Add `cross-site=allow` to a route that must accept cross-site posts, such as an identity provider callback. Behind a TLS-terminating proxy, list the proxy in `ME_GOAHEAD_TRUSTED_PROXIES` or with `--proxy` and have it set `X-Forwarded-Proto`.
- **`methods=` that admit GET also admit HEAD.** An anonymous HEAD can no longer step around a gate that lists GET. Strict route checks also refuse gates that do not cover later nested routes.
- **Methods are case-sensitive.** Unknown methods return 501. Declare extension methods such as `PATCH` in a route's `methods=`.
- **Uploads require `upload=true`.** Other routes return 415 and close; CGI routes receive the unparsed body. Multipart values are no longer URL-decoded. Boundaries must follow RFC 2046. Defaults allow 16 files and 512 parts (`ME_GOAHEAD_LIMIT_UPLOAD_FILES` and `ME_GOAHEAD_LIMIT_PARAMS`).
- **Request parsing is stricter.** Invalid request lines, duplicate sensitive headers, malformed chunks, invalid paths and excessive parameters are rejected. Bodies larger than the input buffer return 413. Requests must finish within `ME_GOAHEAD_LIMIT_REQUEST_TIMEOUT` (default 300 seconds). Form bodies require the correct media type. A `+` in a path is literal. HTTP/1.1 requests without `Host` and request targets containing `#` return 400; unsupported HTTP versions return 505. `Connection: close` on any header line closes the connection. Request parameters can no longer set variables named `HTTP_*`, upload variables or server-set variables.
- **PUT status codes changed.** A new file returns 201 and a replaced file 204. A PUT to a directory returns 409.

### Authentication and TLS

- **Passwords use BF2.** Earlier releases cannot verify newly generated hashes; do not share a regenerated auth file with a 6.1 server. Regenerate unsafe BF1 accounts reported by `websAddUser()`. `gopass` rejects empty passwords with exit code 3.
- **Digest requires MD5 hashes.** PAM and accounts without an MD5 hash cannot use Digest. Use Basic over TLS or form login with PAM. Clients must supply a matching `uri=` and increasing nonce count. Nonces expire after 300 seconds (`ME_GOAHEAD_NONCE_DURATION`); clients should handle `stale="TRUE"`.
- **Sessions are isolated and limited.** Sessions cannot cross authentication protocols. Basic and Digest do not create a session unless the client already has one. Each account has at most eight sessions by default (`ME_GOAHEAD_LIMIT_SESSION_USER`); a new login retires the least recently used.
- **TLS minimums have increased.** Require OpenSSL 3.0 or later, or MbedTLS 3.6.7 or later. Remove obsolete TLS settings from build configurations.
- **HTTPS requires certificates.** Supply `--cert` and `--key` for HTTPS endpoints or remove those endpoints. Private keys must be unencrypted, and with MbedTLS the certificate and key must match, or the server does not start.
- **Contradictory settings fail the build.** `ME_GOAHEAD_SECURE_ONLY` without TLS, disabling both TLS 1.2 and TLS 1.3, and `ME_GOAHEAD_AUTO_LOGIN` without `ME_GOAHEAD_LOGGING` are compile errors.
- **Client trust is explicit.** Set `ME_GOAHEAD_SSL_AUTHORITY` when verifying client issuers; OpenSSL no longer uses system CAs. CRL checking needs a CRL for every CA in the chain. Required client certificates disable session resumption. TLS 1.2 cipher configuration is enforced. With MbedTLS and TLS 1.2 enabled, static RSA, SHA-1 CBC, CCM and DHE suites are no longer offered by default, and static RSA never.
- **The temporary directory must exist.** The server refuses to start if it does not. On Windows `ME_GOAHEAD_TEMP_DIR` is now empty by default, meaning the system temporary directory from `GetTempPath` (`TMP`, then `TEMP`), and that directory is the one audited against the served directories.
- **Privilege drops must succeed.** If using `--user`/`--group` or the corresponding build settings, ensure the account can read documents, auth files and certificates, and write logs and uploads.

### Files, responses and handlers

- **File access is restricted.** Hidden and backup files return 404 by default (`ME_GOAHEAD_DENY_HIDDEN`). POSIX symbolic links outside the document or route directory return 404; `ME_GOAHEAD_CONTAIN_DOCUMENTS=0` restores the previous behavior.
- **Templates need JST routes.** The file handler returns 404 for `.jst` and `.asp`. Add a preceding `handler=jst` route for templates.
- **Caching is restricted.** Only eligible static responses can be publicly cached. Other responses use `no-store, private`. Responses with bodies have a content type and `nosniff`; static files have weak ETags. Unknown-length HTTP/1.0 responses close the connection. Session cookies are host-only.
- **CGI inputs and outputs are stricter.** `REQUEST_URI` is raw: decode it once if needed. Programs resolve under their route prefix and must match `extensions=`. Environment names must be portable; ISINDEX arguments must be plain words. Children do not inherit server descriptors or ignored signals. Failed exec returns 502; program-supplied hop-by-hop and framing headers are dropped. Legacy action callbacks receive only the URL query, never a chunked body. A route `dir=` that does not exist returns 404. On POSIX, a CGI timeout kills the program's whole process group, including background processes it started.
- **JST errors return 500.** Templates no longer expose source on errors. Evaluation limits apply; mixed numeric/non-numeric comparisons are errors. A non-numeric condition, such as `if (name)` on a string, is now an error instead of taking the else branch.
- **Windows needs the Visual C++ Redistributable.** The library and programs now use the dynamic C runtime.
- **Build warnings may fail compilation.** Applications using the project compiler flags may need changes for newly enforced diagnostics.
- **The `goahead` program changed.** It sets a umask of 027, so files it creates are not world-readable. `--background` keeps the working directory instead of changing to `/`.
- **The source archive ships fewer certificates.** Only the test certificates `ca.crt`, `ec.crt`, `self.crt` and `test.crt` ship; take root CA bundles from your platform or another source.

### API and ABI

Rebuild all embedding applications and review these changes:

- `WebsFileInfo.size`, `Webs.txLen`, and the lengths passed to `websWriteHeaders()` and `websSetTxLength()` use `Offset`.
- `bufCreate()` takes `ssize` limits. `appendWinArg()` and `buildWinCmdLine()` take a destination size. `websSetFormVars()` and `websSetQueryVars()` return `int`.
- `websGetSessionID()` always returns an allocated string that the caller must free. A memory notifier installed with `websSetMemNotifier()` must not return.
- `WebsTime` is always a signed 64-bit integer, independent of `time_t`. On targets where `time_t` is narrower, this changes the layout of `Webs`, `WebsSession` and `WebsFileInfo` and the type of the `websParseDateTime()` and `websParseHttpDate()` out-parameter. Source that uses `WebsTime` still compiles but must be rebuilt, and code that passes the address of a `WebsTime` to a C library function expecting a `time_t *` must use a `time_t` instead. Use `websTime()` for the current time.
- `WebsRomIndex.pos` is removed. Hand-written four-field initializers must drop the fourth field. `WebsFileInfo` gains `hash`; `WebsRomIndex` gains `mtime` and `hash`; `WebsSession` gains `used`; `Webs` gains upload counters, `retiredSession` and `boundaryScan`. Regenerate `rom.c` with `webcomp`.
- `websServiceEvents()` takes a `volatile int *` and no longer resets the flag: initialize it before the call, as the function returns at once if it is already set.

New APIs:

- Authentication and sessions: `websLoginSession`, `websGetSessionCount`, `websReleaseUser`.
- Routing and requests: `websSetStrict`, `websGetStrict`, `websParseContentType`, `websOriginForm`, `websKnownMethod`, `websRouteServesAction`, `websSameOrigin`, `websSetTrustedProxies`, `websIsTrustedProxy`.
- Files and paths: `websStatOpenFile`, `websPageAllowed`, `websPageContained`, `websAbsolutePath`, `websIsReservedDevice`, `websGetTempDir`.
- Responses and runtime: `websTime`, `websDropPrivileges`, `websAddResponseHeader`, `websParseHttpDate`, `websEscapeControl`, `websEscapeJs`, `websHardenProcess`, `logGetPath`, `sslHasCertificate`, `socketLinger`.
- The header now declares `websSetHostUrl`.

## Security

This release addresses these advisories:

| Advisory | Severity | Fix |
| --- | --- | --- |
| GOAHEAD-SA-2026-0012 | Medium | Memory leak in `websWriteHeader()` when a connection breaks during a header write. |
| GOAHEAD-SA-2026-0013 | Low | Undefined signed left-shift in `hashIndex()` for high-bit key bytes. |
| GOAHEAD-SA-2026-0014 | High | Staging-file and memory leak when a multipart field name repeats. |
| GOAHEAD-SA-2026-0015 | Critical | Unvalidated rewrites could escape the document root, expose template source or bypass authentication. Sessions could also satisfy routes using another authentication protocol. |

Mahmoud Jadaan (@masjadaan) privately reported the `If-Modified-Since` date overflow. This correctness fix has no security impact on standard builds and has no advisory.

Other internally discovered fixes address password and authentication bypasses, Digest replay, cross-site requests, CGI and response-header injection, descriptor inheritance, request-variable overwrites, malformed-input crashes, unsafe PUT staging, unbounded scripts, slow-reader stalls and writes on unfinished TLS handshakes. These fixes have no separate advisories.

[TODO: confirm severities and advisory list against the published advisories before release.]

## Vulnerability Disposition

[TODO: confirm after disposition review] No known exploitable High or Critical vulnerability remains open in this release. Remaining Medium and Low findings are risk-accepted in the manufacturer's release vulnerability disposition. Known limitations are disclosed in the accompanying user information.

## Compatibility

Applications using the documented 6.1 API must be rebuilt against the 6.2.0 headers and adjusted for the API and ABI changes above. Review route files, build settings and clients against [Breaking and Behavior Changes](#breaking-and-behavior-changes).

Existing `auth.txt` files remain usable except for unsafe BF1 accounts reported by `websAddUser()` and Digest accounts without MD5 hashes. Newly generated BF2 hashes cannot be used by 6.1 servers.

---

**Full Changelog**: [v6.1.1 to v6.2.0](https://github.com/embedthis/goahead/compare/v6.1.1...v6.2.0)
