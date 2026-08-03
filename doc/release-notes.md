# GoAhead v6.1.1 Release Notes

**Release Date**: 2026-08-02

## Overview

GoAhead v6.1.1 is a security patch release. It closes an authentication bypass, several TLS and mTLS
certificate-validation defects, CGI argument- and environment-injection defects, HTTP request-smuggling
vectors, and the findings of an in-house source audit of the 6.1.1 tree. Upgrading is recommended for
all deployments.

## Security Fixes


- **Critical**: Fixed BF1 Basic-auth password bypass in `websVerifyPasswordFromFile()`.
  Any password would authenticate any user whose stored password used the Blowfish
  (`BF1:`) salted hash prefix. The bcrypt result was computed correctly then discarded
  by a subsequent constant-time comparison of the stored hash against itself.
  (Advisory GOAHEAD-SA-2026-0001.)

  Operators are strongly advised to upgrade immediately if Basic authentication is
  enabled against any BF1-stored users. Rotating BF1 user passwords after upgrade is
  recommended on general principle.

- **Critical**: Fixed OpenSSL certificate-revocation checking being silently disabled.
  The OpenSSL mTLS backend loaded a configured CRL via `X509_STORE_load_locations()`
  but never set `X509_V_FLAG_CRL_CHECK`, so OpenSSL's `check_revocation()`
  short-circuited and **revoked client certificates were accepted on every mTLS
  handshake**. The fix sets the flag in `src/openssl.c`. MbedTLS builds were not
  affected. (Advisory GOAHEAD-SA-2026-0006.)

- **High**: Fixed mTLS client-certificate bypass via the depth-guard fragility in
  `verifyClientCertificate()`. Deployments with `ME_GOAHEAD_SSL_VERIFY_PEER=1`
  and `ME_GOAHEAD_SSL_VERIFY_ISSUER=0` built against LibreSSL 3.2.x or
  3.3.2–3.3.6 accepted attacker-controlled client-certificate chains that
  exceeded `VERIFY_DEPTH` (10). The depth check was conditional on `error == 0`,
  so when the legacy LibreSSL verifier surfaced an over-depth chain as
  `X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY` (error 20) instead of
  `X509_V_ERR_CERT_CHAIN_TOO_LONG` (error 22), the depth guard was skipped and
  the switch accepted the chain under `VERIFY_ISSUER=0`. The fix enforces the
  configured chain-depth limit unconditionally based on
  `X509_STORE_CTX_get_error_depth()`, removing dependence on backend-specific
  error mapping. OpenSSL, BoringSSL, AWS-LC, and LibreSSL 4.x preserve error 22
  and were not vulnerable in practice; the fragility is fixed for defence in
  depth on all backends. (Advisory GOAHEAD-SA-2026-0004.)

  Operators running mTLS with `VERIFY_ISSUER=0` against an affected LibreSSL
  release should upgrade as a priority.

- **High**: Fixed a pre-authentication out-of-bounds read in the Digest
  `Authorization` parser. A quoted-string value ending in a single backslash caused
  the unescape loop to read one byte past the end of the buffer (CWE-125). The fix
  bounds-checks the trailing escape in `src/auth.c`. Memory-safety defect, found
  internally; no separate advisory.

- **Medium**: Fixed Digest authentication nonce leak. The `WWW-Authenticate`
  Digest nonce previously embedded the server's `masterSecret` in cleartext
  inside its base64-encoded payload, exposing the secret on the wire. Replaced
  with a non-reversible MD5-keyed construction
  (`base64(MD5(masterSecret:realm:time:counter):time:counter)`) that never
  transmits the master secret. Verification reconstructs the keyed hash from
  the transmitted time and counter and compares it constant-time. Existing
  Digest clients are unaffected — the change is server-side only and the wire
  format remains a base64-encoded opaque token.
  (Advisory GOAHEAD-SA-2026-0002.)

- **Medium**: Fixed CGI `HTTP_PROXY` injection (httpoxy, CVE-2016-5385 family) and
  repaired the non-functional `envBlackList[]` loop in `cgiHandler()`. A request
  `Proxy:` header was previously mirrored to the CGI environment as `HTTP_PROXY`,
  which outbound HTTP clients invoked by CGI scripts (libcurl, wget,
  Python `requests`, Ruby `Net::HTTP`, Perl `LWP`, PHP Guzzle, …) read to
  determine the outbound proxy. A single attacker request could route all
  cleartext outbound HTTP from a CGI script through an attacker-controlled proxy
  (credential theft, response forgery, SSRF). Concurrently, a long-standing
  `continue` bug placed inside the per-blacklist iteration loop made the entire
  `envBlackList[]` array (`PATH`, `IFS`, `LD_PRELOAD`, `BASH_ENV`, `PYTHONPATH`,
  …) inert: matches advanced the iterator only, and the variable was still added
  to the CGI environment unconditionally. Both defects are now closed.
  (Advisory GOAHEAD-SA-2026-0003.)

  Operators with CGI enabled (`ME_GOAHEAD_CGI=1`, the default) and CGI scripts
  that perform outbound HTTP — for firmware update checks, license validation,
  telemetry push, or "phone home" flows — should upgrade. Custom handlers that
  reflect `wp->vars` into their own spawned processes must apply the same
  filter; the env-build path in `cgiHandler()` is the only place the in-tree
  filter runs.

- **High (conditional)**: Fixed CGI ISINDEX argument / option injection
  (CWE-88) in `cgiHandler()`. When a CGI query string contained no literal
  `=`, GoAhead treated it as an RFC 3875 ISINDEX search, URL-decoded it, split
  it on spaces, and appended every token to the launched CGI program's `argv[]`.
  A request such as `GET /cgi-bin/interp?-e+<payload>` therefore delivered
  attacker-controlled command-line **options** — not just search terms — to the
  target on every platform (POSIX `execve`, Windows `CreateProcess`, VxWorks
  `taskSpawn`). Against an argv-driven interpreter or option-sensitive tool this
  is option injection that can reach RCE. The raw-`=` gate also ran *before*
  URL-decoding, so an encoded `=` (`%3D`) slipped through and smuggled
  `key=value` option forms (e.g. `php-cgi -d name=value`) into argv. The
  ISINDEX branch now rejects any `-`-leading decoded token (**400**), rejects a
  decoded `=` (**400**, closing the `%3D` bypass), and prefixes surviving search
  terms with a POSIX `--` end-of-options marker so a conformant target treats
  them as positional operands only. The ISINDEX feature itself is retained.
  (Advisory GOAHEAD-SA-2026-0009.) This is the cross-platform option-injection
  layer that the Windows-shell / argv-quoting fix GOAHEAD-SA-2026-0007
  explicitly left unaddressed; the two fixes compose — SA-0007 delivers the
  `argp` array faithfully, SA-0009 ensures that array carries no injected options.

  Operators with CGI enabled (`ME_GOAHEAD_CGI=1`, the default) that route
  requests to an argv-driven interpreter or option-sensitive CGI target should
  upgrade. A stock install ships no such target and is not directly exposed.
  ISINDEX search queries whose terms legitimately begin with `-` will now be
  rejected with 400.

- **Medium**: Fixed HTTP request smuggling (CWE-444) via Content-Length /
  Transfer-Encoding desync. GoAhead's HTTP/1.1 parser previously accepted a
  request carrying **both** `Content-Length` and `Transfer-Encoding`, and
  recognised `Transfer-Encoding` only when its value was exactly `chunked`,
  silently falling back to `Content-Length` framing on any list or obfuscated
  value (e.g. `gzip, chunked`). Behind a front-end proxy that framed the same
  bytes differently, either desync (CL.TE or TE.CL) let a crafted request
  smuggle a second request across a keep-alive connection. `parseHeaders()`
  now enforces RFC 9112 §6.1: a request with both framing headers is rejected
  **400**; an unsupported transfer coding is rejected **501** (previously
  silently treated as Content-Length-framed) rather than downgraded;
  `Transfer-Encoding` on an HTTP/1.0 message is rejected **400**; and
  `Content-Length` is parsed strictly (digits-only, duplicates rejected).
  The connection is closed on every rejection.
  (Advisory GOAHEAD-SA-2026-0008.)

  Operators deploying GoAhead behind a front-end proxy, load balancer, or WAF
  should upgrade. A directly-exposed GoAhead with no intermediary is not
  meaningfully affected. Clients that relied on the old silent fall-back for a
  non-chunked `Transfer-Encoding` value now receive a 501; no conforming client
  sends a transfer coding GoAhead cannot decode.

- **Medium**: Fixed two further HTTP request-smuggling vectors (CWE-444; CWE-190
  for the chunk-size wrap) not covered by GOAHEAD-SA-2026-0008. A `HEAD` request
  that declared a `Content-Length` had its body-length bookkeeping skipped
  (`parseHeaders()` gated the `rxRemaining` assignment on the method not being
  HEAD), so the declared body was left in the receive buffer and re-parsed as the
  next request on a keep-alive connection; GoAhead now accounts for and drains a
  declared HEAD body like any other method. Separately, `filterChunkData()` parsed
  the chunk-size line with `hextoi()`, whose 32-bit accumulator wrapped on
  overflow — a 9-digit size such as `100000000` (2^32) became `0` and was mistaken
  for the terminating chunk, completing the body early and re-servicing the trailing
  bytes. `hextoi()` now saturates to `UINT_MAX` on overflow, and `filterChunkData()`
  rejects any chunk size above the configured request-body limit with **400** and
  closes the connection. The same fix also closes a related resource-exhaustion gap
  (CWE-770): a chunked body assembled from many in-range chunks previously bypassed
  the cumulative `postLimit` the `Content-Length` path enforces up front — streamed
  unbounded to a CGI target — so `filterChunkData()` now tracks the running body
  total and rejects it with **413** + close once it crosses the limit (`postLimit`
  for POST, `ME_GOAHEAD_LIMIT_PUT` for PUT).
  (Advisory GOAHEAD-SA-2026-0010.)

  Operators deploying GoAhead behind a front-end proxy, load balancer, or WAF
  should upgrade. A directly-exposed GoAhead with no intermediary is not
  meaningfully affected.

- **Medium**: Fixed a MbedTLS Diffie-Hellman parameter truncation that destroyed
  forward secrecy. On TLS 1.2 DHE-RSA, `sizeof(dhm_g)` was passed as the prime
  length, so the `ServerKeyExchange` shipped a 1-byte DH modulus instead of the
  configured 2048-bit MODP prime. The fix passes the correct prime length
  (`sizeof(dhm_p)`). OpenSSL builds, TLS 1.3, and ECDHE were not affected.
  (Advisory GOAHEAD-SA-2026-0005.)

- **Low**: Fixed session fixation — the session id is now regenerated on login.
  `websGetSession()` previously adopted the client-supplied `-goahead-session-`
  cookie value verbatim as the id of a newly-created session, and a successful
  form login did not rotate it, so a session id planted in a victim's browser
  before login survived into the authenticated session. `websGetSession()` now
  always allocates a new, server-generated id (`makeSessionID()`) when creating a
  session; the incoming cookie is still honoured to look up a live session but can
  no longer seed a new one. Rotation now happens at every privilege transition, not
  just form login: `websAuthenticate()` previously stored the authenticated username
  with `websGetSession(wp, 1)`, which adopts a live client-presented id, so Basic and
  Digest login wrote the victim's identity into whatever session the client arrived
  carrying. Both paths now call the new `websRotateSession()`, which destroys the old
  session server-side and issues a fresh id. Pre-login session variables do not survive
  the transition. The fix is independent of the TLS stack and password store.
  (Advisory GOAHEAD-SA-2026-0011.)

  Rated Low / primarily defense-in-depth: exploitation requires the attacker to
  plant a session cookie in the victim's browser before login, which in its common
  (on-path / plaintext-HTTP) form already grants equivalent access. After upgrade,
  invalidate active sessions (or restart the server) so any pre-upgrade fixed id is
  discarded.

## Security Fixes — July 2026 Audit

An in-house multi-specialist source audit of the 6.1.1 tree closed a further 31 defects. No advisory
is issued for these: they were found internally, not reported, and no release carried them to a
customer under a CVE. They are grouped by class below.

### Authorization and routing

- **Critical**: A case-variant URI selected a different route than the file the kernel opened, so on a
  case-insensitive file system (macOS, Windows, vfat) the variant fell through to the unauthenticated
  catch-all while `open()` still served the protected document. That single compare gates route auth,
  abilities, form login and TLS enforcement. Route matching now folds consistently with the file
  system. The same fix covers the Unicode NFC/NFD and non-ASCII case-folding variants, and the
  Windows trailing-dot / trailing-space / alternate-data-stream filename folding that bypassed
  extension-gated routes and disclosed template source.
- **Critical**: `websPump` re-dispatched the protected route's handler after the authentication gate
  had already emitted its 401, so the guarded handler ran anyway.
- **Critical**: An unbounded re-route loop — the route rescan wrapped to index zero and spun
  `websPump`, wedging the single-threaded server.

### Memory safety

- **Critical**: Use-after-free when a request timeout freed the `Webs` object while its CGI child was
  still registered; `websCgiPoll` then used it.
- **Critical**: Use-after-free of `Webs.session`, which dangled across the body read and could be
  freed by a concurrent logout or session prune.
- **Critical**: Double free of `wp->putname` when the PUT staging file could not be opened.
- **High**: Heap use-after-free in the JST interpreter — a bare `var NAME` freed an attacker-supplied
  query or form value and then read it.
- **High**: Failed Digest authentication orphaned the cloned username, leaking roughly 1.8 KB of heap
  per unauthenticated request.
- **Critical**: Repeated `Content-Disposition filename=` parameters each leaked an open file
  descriptor and a deleted-but-open temporary file.

### Format strings

- **Critical**: A CGI response's `Content-Type` value was passed as the printf *format* argument to
  `websWriteHeader`, making a CGI-controlled string a format string.
- **Critical**: GoAhead's own `sprintfCore` implemented `%n`, which turns any reachable format-string
  defect into an arbitrary write. `%n` is now rejected.
- **High**: 13 of 14 printf-family wrappers lacked `PRINTF_ATTRIBUTE`, so `-Wformat-security` could
  never fire on them; the attributes are now present and the MSVC build gained equivalent diagnostics.

### Availability

- **Critical**: A blocking `send()` with no send timeout in `websFlush` let one slow-reading client
  freeze the event loop indefinitely. The flush is now **bounded** by a deadline that is tested on
  every iteration, with the per-send timeout a slice of the budget rather than the whole of it.
  **This bounds the stall; it does not eliminate it** — a slow-reading client can still hold the event
  loop for up to `ME_GOAHEAD_LIMIT_TIMEOUT` (60 seconds by default) before the peer is dropped. That
  residual is tracked as an open defect and disclosed under Known Limitations in the user information.
- **Critical**: No file-type gate before `open()`, so a request naming a FIFO (or a Windows device
  name) blocked the single-threaded server in the kernel. Only regular files are opened now.
- **Medium**: Failed form logins allocated a session each, so roughly 512 anonymous requests exhausted
  the session table and locked out form login.

### CGI

- **Critical**: A chunked request body was forwarded to the CGI with `CONTENT_LENGTH=-1`; the standard
  CGI idioms turn that into `malloc(0)` plus an out-of-bounds write, or a `read()` length of
  `SIZE_MAX`. Body-less requests published `-1` too. Both now publish the decoded byte count.
- **High**: CGI header pass-through emitted the entire remaining header block as a single header
  value, bypassing the key sanitizer.
- **High**: Headerless CGI output was spliced into the response header block and the end-of-headers
  marker was never written.
- **High**: The 408 request-timeout response was never emitted for a detached CGI.
- **Medium**: CGI control headers (`Status`, `Location`, `Content-Type`, `Content-Length`) were dropped
  when they followed a pass-through header.
- **Medium**: On Windows, `launchCgi` closed `INVALID_HANDLE_VALUE` and an uninitialised
  `procinfo.hThread`.

### HTTP framing

- **Medium**: Request smuggling via `Content-Length` integer overflow (CWE-190 feeding CWE-444), a
  residual of GOAHEAD-SA-2026-0008. The all-digit value was validated and then converted with
  `atoi()`, whose `int` return truncated a large value to a small non-negative number — so
  `Content-Length: 4294967297` framed a 1-byte body and the undrained remainder was serviced as a
  second, attacker-framed request on a keep-alive connection. The value is now accumulated at 64-bit
  width and tested against the applicable body limit on every digit, so any out-of-range declaration
  is rejected with **413** and the connection closed.

### Sessions

- **Critical**: Basic and Digest login adopted a live client-presented session id without rotating it
  (session fixation, the authenticated-path sibling of GOAHEAD-SA-2026-0011).

### Configuration and supply chain

- **High**: The committed `dist/` amalgamation — which pak consumers compile — could go stale and ship
  code that `src/` had already fixed. The amalgamation is now regenerated and diffed against the
  committed copy in CI on every push and pull request.
- **Low**: The shipped `route.txt` was a single unauthenticated catch-all with no authenticated route
  anywhere, so the out-of-box configuration demonstrated nothing about access control.

### Further high-severity fixes

Landed after the initial audit sweep and included in 6.1.1. With these, **no High or Critical finding
remains open**.

- **High**: No concurrent-connection limit, combined with a fully resettable idle timeout, made
  Slowloris a whole-server denial of service. A per-peer connection limit and an absolute deadline on
  the pre-header phase now bound it.
- **High**: `ME_GOAHEAD_AUTH=0` failed open — `auth=` and `abilities=` route directives were parsed
  and installed but never enforced, so a build that disabled authentication silently served protected
  routes rather than refusing to start.
- **High**: A CGI-declared `Content-Length` was echoed to the client but never enforced against the
  bytes the CGI actually wrote.
- **High**: Every `Set-Cookie` leaked its cookie string, and an unauthenticated
  `GET /action/logout` leaked one per request — a remote memory-exhaustion path
  needing no credentials.
- **High**: 401 authentication challenges and error responses were marked publicly
  cacheable for 24 hours, so a shared cache could serve one user's challenge or
  error body to another.
- **High (VxWorks)**: The no-fork CGI implementation freed the argv backing store
  while the spawned task was still reading it.
- **High (VxWorks)**: CGI temporary files were created inside the served document
  root under predictable `tmp-N.tmp` names whose counter resets on restart, so CGI
  output containing another user's response data could be fetched over HTTP by an
  unauthenticated client.
- **Medium**: The response `Content-Length` was emitted through an `(int)` cast,
  splitting responses for files of 2 GiB or more. The two emit sites are fixed; a
  remaining 32-bit narrowing in `WebsFileInfo.size` requires a public-header change
  and is deferred.
- **Medium**: Leading-CRLF stripping before the request line was unbounded, so a
  peer could send CRLF pairs indefinitely without ever crossing the header-size
  limit. The prologue strip is now capped; enforcing `PARSE_TIMEOUT` as an absolute
  pre-header deadline is deferred and folds into the connection-limit work.

### Known residuals

The audit filed every finding it raised. 174 defects remain open — **none critical and none high**.
Every remaining finding is Medium or Low, and none is a disclosure, remote-code-execution or
authentication-bypass path in a default build. They are formally risk-accepted for 6.1.1 under a
class-level rule recorded in the release vulnerability disposition, which is retained by the
manufacturer as part of the technical documentation, and each stays open with its reproduction for a
subsequent release.

Three findings reached Medium by re-assessment rather than by a code change — the JST execution
budget, the absence of a privilege-drop mechanism, and the bounded slow-reader stall. Their exposure
is unchanged; the disposition record holds both the argument and its caveats for each, and all three
are disclosed under Known Limitations in the user information.

## Authentication Hardening

- **Plaintext password max length now independent of username and realm**.
  `WEBS_MAX_PASSWORD` (32 bytes) previously bounded the formatted
  `"username:realm:password"` string, so the effective plaintext ceiling
  depended on username and realm length — a 13-char username with realm
  `example.com` left only 6 bytes of plaintext headroom, and any longer
  password silently failed to authenticate (or silently failed to be
  created by `gopass`). The check is now applied to plaintext only, at
  both the hash-creation (`gopass`) and runtime verification paths, so
  any username can use a full 32-char password. A separate constant
  `WEBS_MAX_PASSWORD_BUFFER` (99) sizes the internal crypt buffer for
  defense-in-depth.
- **BF1 + Digest combination rejected explicitly**. A user with a BF1
  stored hash authenticating via Digest is now rejected up-front with a
  clear trace message rather than silently failing through the BF1
  branch. Digest never transmits plaintext, so bcrypt verification could
  never succeed. Branch selection in `websVerifyPasswordFromFile` was
  reordered to test `wp->digest` first (request-driven), with the BF1
  stored-hash combination rejected before formatting.

## Testing

- The full TestMe suite passes **86/86 tests, 12249/12249 assertions** on macOS
  (2026-08-01, after `make clean && make`). Every fix above carries a regression
  test; the suite grew from 39 tests / 757 assertions at the 6.1.1 draft.
- Strengthened authentication unit-test coverage with direct server-side parser
  robustness cases.
- New `test/regress/` coverage added for the audit fixes, including
  `content-length-overflow` (the smuggling payload must yield one 413),
  `cgi-bat-gate` (the Windows `.bat` CGI gate at the HTTP level),
  `slow-reader` (the bounded flush), `uri-case-variant` and `uri-unicode-fold`
  (route-authorization folding), `cgi-header-injection`, `cgi-headerless-output`,
  `cgi-content-type-format`, `format-attribute`, `file-type-gate`,
  `reroute-loop`, and `upload-duplicate-filename`.
- CI verifies that the committed `dist/` amalgamation matches `src/`.
- New `test/auth/gopass.tst.ts` verifies the plaintext-length ceiling at the
  `gopass` CLI.
- Digest tests strengthened to exercise server-side parser robustness
  directly (raw `Authorization` header crafting): stale and future nonce
  timestamps, `qop="auth-int"` rejection, malformed and oversize fields,
  challenge-then-empty-field cases.

## Compatibility

### Breaking Changes

None for deployments following the documented workflow. Passwords created with
`gopass --cipher blowfish` continue to verify correctly because the verify path now
bcrypts `username:realm:plaintext`, matching what `gopass` stores.

BF1 hashes produced by calling `websMakePassword()` directly on raw plaintext
(bypassing `gopass`) will no longer verify and must be regenerated via `gopass`.
No in-tree code does this; the risk applies only to out-of-tree tooling.

### Behavior Changes

- Usernames previously unable to authenticate with passwords longer than
  `32 - (len(username) + len(realm) + 2)` can now use the full 32-char
  plaintext limit. No action required; existing shorter passwords continue
  to verify unchanged.
- `gopass` rejects a plaintext password longer than 32 characters with
  exit code 3 and a clear error message. Previously, hash creation would
  succeed for short combined strings and silently fail for longer ones.

## Upgrading

Drop-in replacement for 6.1.0. No configuration changes required.

---

**Full Changelog**: https://github.com/embedthis/goahead/compare/v6.1.0...v6.1.1
