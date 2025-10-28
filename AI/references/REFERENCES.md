# GoAhead References

This document contains useful references for GoAhead development, including external documentation, tools, standards, and resources.

## Table of Contents

1. [Official Documentation](#official-documentation)
2. [Development Tools](#development-tools)
3. [Standards and Specifications](#standards-and-specifications)
4. [Security Resources](#security-resources)
5. [Related Projects](#related-projects)
6. [Build and Testing](#build-and-testing)
7. [Community and Support](#community-and-support)

## Official Documentation

### GoAhead Documentation
- [GoAhead Official Website](https://www.embedthis.com/goahead/)
- [GoAhead Documentation](https://www.embedthis.com/goahead/doc/index.html)
- [GoAhead API Reference](https://www.embedthis.com/goahead/doc/api.html)
- [GoAhead User Guide](https://www.embedthis.com/goahead/doc/guide.html)
- [GoAhead Release Notes](https://www.embedthis.com/goahead/doc/release.html)

### EmbedThis Documentation
- [EmbedThis Website](https://www.embedthis.com/)
- [Ioto Device Agent](https://www.embedthis.com/ioto/) - Next-generation device management
- [EmbedThis Builder](https://www.embedthis.com/builder/) - OTA update distribution

### GitHub Repository
- [GoAhead GitHub](https://github.com/embedthis/goahead)
- [GitHub Issues](https://github.com/embedthis/goahead/issues)
- [GitHub Releases](https://github.com/embedthis/goahead/releases)
- [Contributing Guide](https://github.com/embedthis/goahead/blob/master/CONTRIBUTING.md)

## Development Tools

### Compilers and Build Systems
- [GCC - GNU Compiler Collection](https://gcc.gnu.org/)
- [Clang - C Language Family Frontend](https://clang.llvm.org/)
- [Visual Studio](https://visualstudio.microsoft.com/) - Windows development
- [Xcode](https://developer.apple.com/xcode/) - macOS development
- [GNU Make](https://www.gnu.org/software/make/)

### Version Control
- [Git](https://git-scm.com/)
- [Git for Windows](https://gitforwindows.org/) - Git with Bash for Windows

### Package Managers
- [vcpkg](https://vcpkg.io/) - C/C++ package manager (Windows)
- [Homebrew](https://brew.sh/) - macOS package manager

### Testing Tools
- [TestMe](https://www.embedthis.com/testme/) - Multi-language test framework
- [Bun](https://bun.sh/) - JavaScript/TypeScript runtime
- [Valgrind](https://valgrind.org/) - Memory debugging and leak detection

### Code Quality
- [Uncrustify](https://github.com/uncrustify/uncrustify) - Code formatting
- [Doxygen](https://www.doxygen.nl/) - Documentation generation
- [cppcheck](http://cppcheck.sourceforge.net/) - Static analysis

## Standards and Specifications

### HTTP and Web Standards
- [RFC 9110 - HTTP Semantics](https://datatracker.ietf.org/doc/html/rfc9110)
- [RFC 9112 - HTTP/1.1](https://datatracker.ietf.org/doc/html/rfc9112)
- [RFC 2617 - HTTP Authentication](https://datatracker.ietf.org/doc/html/rfc2617)
- [RFC 7230-7235 - HTTP/1.1 Specification](https://tools.ietf.org/html/rfc7230)
- [RFC 3875 - CGI Specification](https://datatracker.ietf.org/doc/html/rfc3875)
- [MIME Types](https://www.iana.org/assignments/media-types/media-types.xhtml)

### TLS/SSL Standards
- [RFC 8446 - TLS 1.3](https://datatracker.ietf.org/doc/html/rfc8446)
- [RFC 5246 - TLS 1.2](https://datatracker.ietf.org/doc/html/rfc5246)
- [RFC 6066 - TLS Extensions (SNI)](https://datatracker.ietf.org/doc/html/rfc6066)

### WebSocket
- [RFC 6455 - WebSocket Protocol](https://datatracker.ietf.org/doc/html/rfc6455)

### Character Encoding
- [RFC 3629 - UTF-8](https://datatracker.ietf.org/doc/html/rfc3629)
- [RFC 4648 - Base64 Encoding](https://datatracker.ietf.org/doc/html/rfc4648)

## Security Resources

### Cryptography Libraries
- [OpenSSL](https://www.openssl.org/) - Cryptography and SSL/TLS toolkit (recommended)
- [OpenSSL Documentation](https://www.openssl.org/docs/)
- [MbedTLS](https://www.trustedfirmware.org/projects/mbed-tls/) - Embedded TLS library
- [MbedTLS Documentation](https://mbed-tls.readthedocs.io/)

### Security Standards
- [OWASP Top 10](https://owasp.org/www-project-top-ten/) - Web application security risks
- [CWE - Common Weakness Enumeration](https://cwe.mitre.org/)
- [CVE - Common Vulnerabilities and Exposures](https://cve.mitre.org/)

### Regulatory Compliance
- [EU Cyber Resilience Act (CRA)](https://digital-strategy.ec.europa.eu/en/policies/cyber-resilience-act)
- [NIST Cybersecurity Framework](https://www.nist.gov/cyberframework)

### Security Best Practices
- [OWASP Secure Coding Practices](https://owasp.org/www-project-secure-coding-practices-quick-reference-guide/)
- [CIS Benchmarks](https://www.cisecurity.org/cis-benchmarks/)
- [SANS Security Resources](https://www.sans.org/security-resources/)

### Vulnerability Databases
- [National Vulnerability Database (NVD)](https://nvd.nist.gov/)
- [GitHub Security Advisories](https://github.com/advisories)

## Related Projects

### EmbedThis Projects
- [Ioto Device Agent](https://www.embedthis.com/ioto/) - Modern IoT device management
- [Appweb](https://www.embedthis.com/appweb/) - Full-featured embedded web server
- [MakeMe](https://www.embedthis.com/makeme/) - Build tool (internal)

### Updater Module
- [Updater README](../../src/updater/README.md) - OTA update module documentation
- [EmbedThis Builder](https://www.embedthis.com/builder/) - Update distribution platform

### Dependencies
- [osdep](../../paks/osdep/) - OS abstraction layer
- [SSL Pak](../../paks/ssl/) - SSL/TLS integration
- [Certificates](../../paks/certs/) - Test certificates

## Build and Testing

### Build Documentation
- [Projects README](../../projects/README.md) - Build system details
- [Build Configuration Guide](https://www.embedthis.com/goahead/doc/users/build.html)

### Testing Resources
- [TestMe Documentation](https://www.embedthis.com/testme/doc/)
- [Test Conversion Summary](../../test/CONVERSION_SUMMARY.md) - EJS to TestMe migration
- [Test README](../../test/README-EJS.md) - Legacy test documentation

### Platform-Specific
- [Windows Development](https://docs.microsoft.com/en-us/windows/dev-environment/)
- [macOS Development](https://developer.apple.com/documentation/)
- [Linux Development](https://www.kernel.org/doc/html/latest/)

## Community and Support

### Official Support
- **Email**: support@embedthis.com
- **Security Issues**: security@embedthis.com

### Community Resources
- [GitHub Discussions](https://github.com/embedthis/goahead/discussions)
- [Issue Tracker](https://github.com/embedthis/goahead/issues)

### Learning Resources
- [Embedded Web Server Design Patterns](https://www.embedthis.com/blog/)
- [IoT Security Best Practices](https://www.embedthis.com/ioto/doc/users/security.html)

## Development References

### C Programming
- [C11 Standard (ISO/IEC 9899:2011)](https://www.iso.org/standard/57853.html)
- [CERT C Coding Standard](https://wiki.sei.cmu.edu/confluence/display/c/SEI+CERT+C+Coding+Standard)
- [GNU C Library Documentation](https://www.gnu.org/software/libc/manual/)

### Memory Management
- [Valgrind User Manual](https://valgrind.org/docs/manual/manual.html)
- [AddressSanitizer](https://github.com/google/sanitizers/wiki/AddressSanitizer)

### Debugging
- [GDB Documentation](https://www.gnu.org/software/gdb/documentation/)
- [LLDB Documentation](https://lldb.llvm.org/)

## Platform APIs

### POSIX
- [The Open Group Base Specifications](https://pubs.opengroup.org/onlinepubs/9699919799/)
- [Linux man pages](https://man7.org/linux/man-pages/)

### Windows
- [Windows API Documentation](https://docs.microsoft.com/en-us/windows/win32/api/)
- [Winsock Reference](https://docs.microsoft.com/en-us/windows/win32/winsock/windows-sockets-start-page-2)

### macOS
- [Apple Developer Documentation](https://developer.apple.com/documentation/)
- [BSD System Calls](https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/)

## Embedded Systems

### Embedded Platforms
- [ESP32 Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/)
- [FreeRTOS](https://www.freertos.org/)
- [Embedded Linux](https://elinux.org/)

### Cross-Compilation
- [Buildroot](https://buildroot.org/) - Embedded Linux build system
- [Yocto Project](https://www.yoctoproject.org/) - Linux distribution builder

## Other Useful Resources

### Performance Analysis
- [perf](https://perf.wiki.kernel.org/) - Linux performance tools
- [Instruments](https://developer.apple.com/instruments/) - macOS profiling

### Network Tools
- [Wireshark](https://www.wireshark.org/) - Network protocol analyzer
- [curl](https://curl.se/) - HTTP client for testing
- [netcat](http://netcat.sourceforge.net/) - Network debugging

### Documentation Tools
- [Doxygen Manual](https://www.doxygen.nl/manual/)
- [Markdown Guide](https://www.markdownguide.org/)
- [CommonMark Specification](https://commonmark.org/)

---

**Last Updated**: 2025-10-27

**Note**: Links and resources are subject to change. If you find broken links or have suggestions for additional resources, please contribute via pull request or contact support@embedthis.com.
