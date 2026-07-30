certs
===

Test SSL/TLS certificate generation for the Ioto embedded IoT platform.

**WARNING: All certificates are for testing only — never use in production.**

## Building

```bash
make                    # Generate all test certificates
make build              # Same as above
make ca-cert            # Generate CA certificate (ca.crt, ca.key)
make self-signed-cert   # Generate self-signed certificate (self.crt, self.key)
make test-cert          # Generate CA-signed test certificate (test.crt, test.key)
make ec-cert            # Generate EC certificate (ec.crt, ec.key)
make cert-request       # Generate CSR for external CA (server.csr, server.key)
make show-certs         # Display all certificate details
make roots              # Download Mozilla root CA bundle
make package            # Copy distributable files to dist/
make cache              # Build, package and cache
make clean              # Remove all generated files
```

## Certificate Files

Generated certificates are placed in the `dist/` directory.

| File | Purpose |
|------|---------|
| `dist/ca.crt` / `dist/ca.key` | Test Certificate Authority (signs test.crt) |
| `dist/self.crt` / `dist/self.key` | Self-signed server certificate |
| `dist/test.crt` / `dist/test.key` | CA-signed test server certificate |
| `dist/ec.crt` / `dist/ec.key` | Elliptic Curve (prime256v1) certificate |
| `src/aws.crt` | AWS IoT root CA certificate |
| `roots.crt` | Full Mozilla root CA bundle |
| `roots-min.crt` | Minimal root CA bundle |
| `CLAUDE.md` | AI coding assistant instructions |
| `README.md` | Module documentation |
| `Makefile` | Build script for certificate generation |

## Configuration

- `src/openssl.conf` — OpenSSL CA configuration with certificate extensions
- `src/*.ans` — Answer files providing non-interactive input to OpenSSL prompts
- Default: 2048-bit RSA keys, 3650-day validity (10 years)
