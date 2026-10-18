# User Information

Product: Embedthis GoAhead
Version: 6.2.0
Release Date: [TODO: release date]

This document provides required user information per the EU Cyber Resilience Act (CRA) Annex II. It
accompanies the product and is distributed with every release archive.

## 1. Manufacturer Information

| Field | Value |
|-------|-------|
| Manufacturer | Embedthis Pty Ltd |
| Address | PO Box 6, Bentleigh VIC 3204, Australia (ABN 18700298317) |
| Contact | dev@embedthis.com (general) |
| Website | https://www.embedthis.com |
| Vulnerability reporting | security@embedthis.com |

## 2. Product Identification

| Field | Value |
|-------|-------|
| Product name | Embedthis GoAhead |
| Version | 6.2.0 |
| Product type | Software library (embeddable, single-threaded ANSI C web server library `libgoahead`) plus the `goahead` reference executable |
| Release date | [TODO: release date] |
| Lifecycle phase | Maintenance mode — security updates and critical bug fixes only; no new features. Successor product line: Ioto Device Agent (https://www.embedthis.com/ioto/). |
| License | Distributed under three licenses — GPL-2.0, the Embedthis Commercial License, and the Embedthis Evaluation License. See `LICENSE.md` in the distribution, `EVAL.md` for the evaluation terms, and https://www.embedthis.com/goahead/licensing.html. The GPL does not generally permit incorporating the software into non-open-source products; embedding in a proprietary product requires a commercial license. |
| Source repository | https://github.com/embedthis/goahead |

### 2.1 Conformity and Component Inventory

| Document | Where it is |
|----------|-------------|
| EU Declaration of Conformity | Accompanies this product: `doc/declaration-of-conformity.md` in the release archive. The declaration is issued electronically and the copy in the archive is the issued document. The full declaration accompanies the product, so no separate internet address is published for it. |
| Software Bill of Materials | Accompanies this product: `doc/sbom.json` in the release archive, in CycloneDX 1.5 format. It is supplied for integrators assembling their own SBOM, and is produced to market surveillance authorities on reasoned request. |
| Release notes | `doc/release-notes.md` in the release archive |
| Integrity checksums | `SHA256SUMS`, published beside the release archive on the download channel (see §7.1) |

The remaining technical documentation — conformity dossier, risk assessment, Annex I matrix,
vulnerability disposition and test report — is retained by the manufacturer under CRA Annex VII and
produced to market surveillance authorities on reasoned request. It is not distributed with the
product.

## 3. Intended Purpose

GoAhead is a lightweight embedded web server written in ANSI C, designed to be linked into device
firmware on resource-constrained hardware. It provides HTTP/1.1 service with keep-alive and chunked
transfer encoding, optional TLS, and optional handler components (CGI, file upload, JavaScript Server
Templates), with role-based access control and route-level permission checks.

The product is intended for **embedded-systems and device developers (integrators)** who build it into
a larger firmware image and are responsible for configuring its routes, authentication, TLS stack, and
deployment environment. It is designed for constrained, often headless, device environments rather than
as a general-purpose internet-facing web server. It is single-threaded and event-driven (non-blocking
I/O via `select()`), with no threads or fiber coroutines.

GoAhead is in maintenance mode. New device projects should evaluate the Ioto Device Agent, the active
successor product line for embedded device connectivity.

## 4. Security Properties

### 4.1 Data Protection

- **Confidentiality / integrity in transit**: provided by optional TLS (see 4.3). When TLS is enabled,
  request and response data are protected against eavesdropping and tampering on the network path.
- **Credential storage**: user credentials are stored as salted password hashes in the authentication
  file (`auth.txt`), not as plaintext. `gopass` writes Blowfish hashes in the current `BF2:` format. Legacy `BF1:` hashes from earlier releases still verify, except those whose username and realm leave too little of the hash key for the password: those are refused and reported at startup, and must be regenerated with `gopass`.
- **Availability**: the single-threaded event loop is bounded by configurable resource limits — receive
  buffer sizes, request timeouts, request-body limits (`ME_GOAHEAD_LIMIT_POST`, `ME_GOAHEAD_LIMIT_PUT`), and file
  limits — that constrain per-connection and per-request resource consumption. Because the server is
  single-threaded, a slow or abusive client that is not bounded by these limits can affect
  responsiveness for other clients; see Known Limitations (10.2) and Foreseeable Misuse (5).

### 4.2 Authentication and Access Control

- **Supported mechanisms**: Basic, Digest, Form-based login, and optional PAM.
- **Legacy caveat**: Basic and Digest authentication are legacy mechanisms and are **strongly
  discouraged**. They should be enabled only where required for legacy client compatibility. Basic
  transmits credentials in a trivially reversible encoding and MUST only ever be used over TLS. Digest
  is a legacy challenge-response scheme that requires MD5.
- **Access control**: role-based access control with route-level permission checks. Handlers and
  authorization requirements are bound to URL routes in the route configuration (`route.txt`).
- **Sessions**: Form-based login uses server-side sessions. Session identifiers are generated
  server-side (`makeSessionID()`) and are regenerated on login (session-fixation hardening since 6.1.1).

### 4.3 Cryptographic Features

- **TLS**: provided via OpenSSL (preferred) or MbedTLS, selected at build time. **GoAhead does not pin
  a TLS library version** — the integrator fixes the concrete OpenSSL/MbedTLS version at link time and
  is responsible for tracking CVEs and recording that version in their own SBOM.
- **Configurable policy**: cipher suites are configurable by the integrator at build time (`ME_GOAHEAD_SSL_CIPHERS`). GoAhead serves a single certificate and does not support SNI.
- **Client-certificate (mTLS)** verification is available; since 6.1.1 GoAhead enforces the configured
  certificate-chain depth limit unconditionally (mTLS depth-guard hardening).
- **Legacy hashing**: Digest authentication requires MD5; this is an accepted compromise retained only
  for legacy compatibility and should not be relied on for new deployments.

### 4.4 Logging and Monitoring

- GoAhead can emit an access log and diagnostic/trace output. Logging destination and verbosity are
  configurable (e.g. `ME_GOAHEAD_LOGFILE`, `-v` verbose flag).
- Operators should route logs to storage with bounded size and appropriate access controls, and should
  treat log fields derived from request data as untrusted when ingesting logs into downstream tooling
  (see Known Limitations 10.2).

### 4.5 Secure Defaults

- **Handlers are gated behind explicit route configuration.** The optional CGI, file-upload, and JST
  handlers are only reachable when a route in `route.txt` binds a URL to that handler; they are not
  served on arbitrary URLs. A stock configuration ships no exposed CGI target and is not directly
  exposed to the CGI attack surface.
- **HTTPS requires a certificate and key.** The default build includes TLS (`ME_COM_SSL`) and its default listen endpoints (`ME_GOAHEAD_LISTEN`) include `https://*:443`, but no certificate or key is configured. An https endpoint opens only when a certificate and key are given with `--cert` and `--key` or configured at build time (`ME_GOAHEAD_SSL_CERTIFICATE`, `ME_GOAHEAD_SSL_KEY`); otherwise GoAhead refuses to listen on it and does not start. No certificate or private key ships in the distribution. The certificates and keys the test suite generates are **development/test certificates only** and MUST NOT be used in production: generate production keys locally.
- Basic and Digest authentication are not appropriate defaults for internet-facing or untrusted
  networks; prefer Form-based authentication over TLS, or an external authenticator.

## 5. Foreseeable Misuse

The following deployment patterns are foreseeably unsafe and should be avoided. Each maps to a real
defect class relevant to this product:

- **Routing CGI to an argv-sensitive or option-driven interpreter.** Exposing a CGI route to a target
  that interprets leading-`-` command-line options (or that changes behavior based on argument
  ordering) creates an option-/argument-injection surface (CWE-88) that can escalate toward remote code
  execution. If CGI must be enabled, restrict it to fixed, non-option-sensitive programs and validate
  all inputs.
- **Deploying Basic or Digest authentication over plaintext HTTP.** These legacy mechanisms expose or
  weakly protect credentials on the wire and, combined with on-path attackers, can also enable session
  planting/fixation. Basic/Digest MUST only be used over TLS, and preferably not at all.
- **Placing GoAhead behind a front-end proxy, load balancer, or WAF that frames requests differently.**
  Divergent HTTP framing between an intermediary and GoAhead is the classic request-smuggling
  (CWE-444) surface. Ensure the intermediary and GoAhead agree on request framing, and keep both
  updated. A directly-exposed GoAhead with no intermediary is not meaningfully exposed to this class.
- **Relying on the single-threaded server to absorb high-concurrency or slow-client load without
  bounding limits.** Configure request timeouts and body/size limits appropriately, and place
  rate-limiting and connection controls in front of the server for exposed deployments.
- **Shipping the test suite's development/test certificates or default authentication files into
  production.** Replace all sample certificates and credentials before deployment.

## 6. Support Period

| Field | Value |
|-------|-------|
| Support period | 5 years (minimum) from release |
| Support start date | [TODO: release date] |
| Support end date | [TODO: release date + 5 years] |
| Security updates | Provided free of charge throughout the support period |
| Update availability | Published updates remain available for at least 10 years |

## 7. Secure Commissioning Guidance

### 7.1 Installation

1. Obtain the official source distribution for the release (`goahead-6.2.0-src.tgz`) from the
   authoritative release channel (see 8.1).
2. **Verify integrity before use**: every release is published with a `SHA256SUMS` file listing the
   SHA-256 of the source archive, generated from the official release build. Download both and verify:

   ```
   shasum -a 256 -c SHA256SUMS
   ```

   `SHA256SUMS` is published beside the archive on the download channel — it cannot travel inside
   the archive it checksums. The manufacturer retains the same file with the release records.
   **Checksums establish integrity, not authenticity**: they detect corruption or truncation in
   transit, but an attacker able to alter the archive on the distribution channel could alter the
   checksum listing with it. GoAhead release artifacts are not cryptographically signed — see §10.2.
3. Build with the project's `make` toolchain, selecting the TLS stack appropriate to your platform
   (OpenSSL preferred, or MbedTLS).
4. **Hardening flags for production builds.** The GoAhead project files and `make` compile with stack protection, `_FORTIFY_SOURCE=3` in the release build on Linux, macOS and FreeBSD, control-flow protection where the compiler supports it, position-independent executables, full RELRO and a non-executable stack, and on Windows with Control Flow Guard, ASLR, DEP and CET compatibility. If you compile GoAhead with your own toolchain, the "Building" section of `SECURITY-DEPLOYMENT.md` lists these flags and how to check a built binary for them. On constrained targets, weigh the code size cost against the exposure of the deployment.

### 7.2 Initial Configuration

1. Select and pin the TLS library version at link time; record that version in your own SBOM.
2. Enable TLS and install a production certificate and private key. Do not use the self-signed
   development certificates and keys the test suite generates.
3. Configure routes (`route.txt`) to expose only the required paths, and bind authorization
   requirements at the route level. Do not bind CGI, upload, or JST handlers to routes unless required.
4. Configure authentication (`auth.txt`). Prefer Form-based authentication over TLS. Do not enable
   Basic or Digest unless a legacy client strictly requires it; if enabled, require TLS.
5. Set resource limits (request timeouts, request-body limits, file/buffer limits) appropriate to the
   device and expected load.
6. No consolidated secure-configuration guide is published for GoAhead. The deployment guidance in
   this document, together with the known limitations in §10.2, is the authoritative set for this
   release.

### 7.3 Integration

- Link `libgoahead` into the firmware image and record the exact TLS library and version in the integrator's
  SBOM (GoAhead does not pin it).
- If deploying behind a proxy/load balancer, ensure request framing is consistent between the
  intermediary and GoAhead (see 5).
- Perform your own security validation appropriate to the target device. Upstream CI builds and runs
  the full test suite on Linux, macOS and Windows and runs static analysis (`scan-build`) and a
  dependency audit on every change, and runs the full suite under AddressSanitizer and UndefinedBehaviorSanitizer on Linux. It runs no fuzzing campaign, and it cannot exercise
  your configuration, your CGI programs or your platform. Upstream CI is not a substitute for your own
  security testing.

## 8. Update Installation Guidance

### 8.1 Obtaining Updates

GoAhead releases are distributed through two channels:

- **Builder site** — https://admin.embedthis.com. Subscribers download the source distribution here.
  Security releases are published to the Builder **first**.
- **Public repository** — https://github.com/embedthis/goahead, for releases, release notes and the
  issue tracker.

**Notification.** Subscribers receive a private email notification from the Builder when a security
update is published. Advisories are published separately as GitHub Security Advisories (see §10.3).
Questions about a security update should be raised through the Builder integrated support portal
rather than the public issue tracker.

### 8.2 Verifying Update Integrity

- Verify the SHA-256 checksum published with the official release before building or deploying an
  update, using the `SHA256SUMS` file distributed alongside the archive (see §7.1).
- 6.2.0 is not cryptographically signed. Run `shasum -a 256 -c SHA256SUMS` beside the downloaded archive, and obtain `SHA256SUMS` from the official download channel. A checksum detects corruption, not substitution by an attacker who controls that channel (see §10.2).

### 8.3 Applying Updates

- [TODO: GoAhead 6.2.0 is a minor release with behavior changes integrators must review before upgrading from 6.1.x. Summarise them from the release notes Breaking Changes section.]
- Rebuild and relink the firmware image against the updated `libgoahead`, then redeploy.
- After upgrading, if the session-fixation fix is relevant to your deployment, invalidate active
  sessions (or restart the server) so any pre-upgrade session identifiers are discarded.
- If Basic authentication is used against Blowfish (`BF1:`) stored users, upgrade promptly and rotate
  affected passwords as a precaution.
- **Rollback** is a property of your firmware update mechanism, not of GoAhead: the library is
  relinked into your image, so reverting means deploying the previously validated image. Credential files are not downgrade-safe: password hashes generated by 6.2.0 use the `BF2` format, which 6.1 servers cannot verify. Roll back the configuration and credential files validated with the earlier image together with it. Session state does not survive a restart.

### 8.4 Automatic Updates

- GoAhead is an embeddable library and does not itself provide an over-the-air or self-update
  mechanism; update delivery is the responsibility of the integrator's firmware update system.
- Confirmed for this release: GoAhead provides no self-update or OTA capability of its own, by
  design. If your device platform provides an update mechanism, GoAhead updates are delivered through
  it like any other firmware component.

## 9. Secure Decommissioning and Data Removal

### 9.1 Data Removal

- Remove authentication files (`auth.txt`) and any stored credential material.
- Remove TLS private keys and certificates.
- Remove configuration files (`route.txt`, custom configuration) and any log files that may contain
  request-derived data.
- Session state is held in memory and is discarded when the server process stops.
- GoAhead itself persists only the following: `auth.txt` (usernames, realm, password hashes),
  `route.txt` and any custom configuration, the TLS certificate and private key at their configured
  paths, upload temporary files under the configured upload directory, and log files at the
  configured `ME_GOAHEAD_LOGFILE` location. Session state is in memory only. Any state a device
  persists *beyond* this set is created by the integrator's application, not by GoAhead, and its
  removal is the integrator's responsibility.

### 9.2 Decommissioning Steps

1. Stop the GoAhead server process.
2. Remove the GoAhead binary/library and its configuration, credential, certificate, and key files from
   the device.
3. Securely wipe private keys and credential material per the device's storage-erase capabilities.
4. Secure-erase steps depend on the storage medium and the platform — an overwrite is not sufficient
   on flash with wear levelling, where the controller may retain prior copies. Use the erase facility
   your platform provides for key material. GoAhead cannot specify this: it does not know the storage
   medium, and prescribing an erase that the medium does not honour would be worse than stating the
   requirement plainly.

### 9.3 Data Transfer

- GoAhead does not manage application user data beyond its configuration, credentials, and session
  state; there is generally no product data to migrate to a successor system.
- If a deployment stores data through a GoAhead CGI program, JST template or custom handler, that
  data belongs to the integrator's application; its migration and secure transfer are outside
  GoAhead's scope and must be documented by the integrator.

## 10. Additional Security Information

### 10.1 Security Configuration Guide

- API and reference documentation: `doc/api/` and `doc/man/` in the release archive.
- Product documentation: https://www.embedthis.com/goahead/doc/
- Deployment and hardening guide: `SECURITY-DEPLOYMENT.md` at the top of the release archive. It covers build hardening, routes and authentication, TLS, network exposure, content and upgrading. Sections 7 and 10.2 of this document summarise the guidance for this release.

### 10.2 Known Limitations

- **Single-threaded architecture.** All requests are served on one event loop; exposed deployments
  should bound concurrency, timeouts, and request sizes and should front the server with rate-limiting
  and connection controls.
- **TLS library version is not pinned by GoAhead.** The integrator selects and must maintain the
  OpenSSL/MbedTLS version, including tracking and patching TLS-library CVEs.
- **Legacy authentication.** Basic and Digest are retained only for legacy compatibility; Digest
  depends on MD5. Neither should be used for new deployments.
- **Privilege drop is opt-in and Unix only.** Started as root, for example to bind ports 80 and 443 of the default listen configuration, GoAhead keeps root privilege unless it is given an account to run as. `--user` and `--group`, or the `ME_GOAHEAD_USER` and `ME_GOAHEAD_GROUP` build settings, make the `goahead` program switch to that account once it has opened its listen endpoints and loaded its configuration; an embedding application calls `websDropPrivileges()` itself. GoAhead refuses to start if the account cannot be found, the switch fails or is incomplete, or root privilege could be regained, and warns when it is left running as root. On non-Unix platforms, including Windows, the options are not offered and the build settings are an error. The drop does not sandbox the server: CGI programs and file access run as that account. Choosing a dedicated least-privilege account that can read the documents, authentication file and certificates and write logs and uploads, and any further confinement (for example systemd `User=` with `CAP_NET_BIND_SERVICE` instead of root, or binding an unprivileged port behind a redirect), remain the integrator's responsibility.
- **Server-side templates are trusted code.** JST templates run on the same event loop that serves every other request, bounded by a step budget (`ME_GOAHEAD_LIMIT_SCRIPT_STEPS`) and a nesting limit (`ME_GOAHEAD_LIMIT_SCRIPT_RECURSE`) that native callbacks do not share. Templates are integrator-authored content: never serve a template from a directory that accepts uploads, and take care with templates whose loop bounds derive from request parameters.
- **Slow-reading clients.** A client that requests a large dynamic response and stops reading holds
  the event loop until the send deadline expires — bounded by `ME_GOAHEAD_LIMIT_TIMEOUT` (60 seconds
  by default), not eliminated. Deployments exposed to untrusted networks should lower that limit at
  build time and front the server with connection controls.
- **Security updates.** Apply releases as they are published, and subscribe to the advisory channels
  in §10.3. Security fixes are delivered only in current releases; backports to earlier versions are
  not provided.
- **Release artifacts are not cryptographically signed.** Integrity verification rests on published
  SHA-256 checksums, which detect corruption but not substitution by an attacker who controls the
  distribution channel. This is a known gap, accepted for this release and tracked for a future one.
- **Upstream CI runs no fuzz campaign and no DAST.** Static analysis (`scan-build`) and a dependency
  audit run on every change, and the full test suite runs on Linux, macOS and Windows, and again on Linux under AddressSanitizer and UndefinedBehaviorSanitizer. The maintainers
  run in-process fuzz harnesses over several parsers, including the CGI argument, environment and
  response paths, outside CI; the harnesses are not part of the source distribution. Integrators must
  perform their own security validation.

### 10.3 Security Resources

- Security policy: https://www.embedthis.com/security
- Vulnerability reporting: security@embedthis.com
- Security advisories: published as GoAhead security advisories (GOAHEAD-SA-*) via GitHub Security
  Advisories at https://github.com/embedthis/goahead/security/advisories. Builder subscribers also
  receive private email notification of security updates; there is no separate advisory mailing list
  or RSS feed.
- Documentation: https://www.embedthis.com/goahead/doc/

---

*This user information is provided in compliance with EU Cyber Resilience Act Annex II requirements and accompanies each product release.*
