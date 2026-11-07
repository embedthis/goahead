# Security Policy

## Reporting a vulnerability

Report a suspected vulnerability in GoAhead privately to security@embedthis.com. Do not open a public issue or pull request for it. Reports are handled under coordinated disclosure, and the fix is published with a GoAhead security advisory (`GOAHEAD-SA-YYYY-NNNN`) through GitHub Security Advisories at https://github.com/embedthis/goahead/security/advisories. The Embedthis security policy is at https://www.embedthis.com/security.

Name a numbered release: the evaluation edition is not a release and is not triaged. Include the GoAhead version (`goahead --version`), the TLS library and its version, the `ME_GOAHEAD_*` settings that differ from the defaults, the route table, and a minimal request or configuration that shows the problem.

## Supported versions

Security fixes are delivered in the current release only; earlier releases receive no backports. Builder subscribers are notified of a security release by email; the public repository carries the release notes and the advisories.

## Deploying GoAhead securely

GoAhead ships set up for evaluation. Before a device carrying it is put in service, work through [SECURITY-DEPLOYMENT.md](SECURITY-DEPLOYMENT.md).

## For contributors

A change to security-relevant code (authentication, sessions, routing, URL and path handling, CGI, uploads, TLS or logging) must keep the existing tests passing and add a test that fails without the change. See [CONTRIBUTING.md](CONTRIBUTING.md).
