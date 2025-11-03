# EmbedThis Updater

<p align="center">
    <img src="https://www.embedthis.com/images/pics/circuit-3.png" alt="EmbedThis Updater" width="50%">
</p>

The EmbedThis Updater is a secure, lightweight command-line utility and library for downloading and applying device software updates published on the [EmbedThis Builder](https://admin.embedthis.com).

## Security for Devices

The European Union has introduced the Cyber Resilience Act (CRA), a regulation aimed at enhancing cybersecurity for IoT products. This legislation mandates that manufacturers ensure their products are secure throughout their entire lifecycle, from design to decommissioning. This requires that software updates are provided for the lifetime of the device.

To meet this need, the Updater can be used to automatically download and apply software updates to all your connected devices. 

## Description

Devices can use the **updater** command or library to enable Over-The-Air (OTA) software update capabilities. The [EmbedThis Builder](https://www.embedthis.com/builder/) cloud service provides:

**Update Management:**
- Secure hosting on AWS S3 with global CDN distribution
- Upload and manage software update images
- SemVer 2.0 compatible version management
- SHA-256 checksum validation
- HTTPS-only secure downloads

**Selective Distribution:**
- JavaScript-like distribution policy expressions
- Target updates based on device properties, versions, or custom criteria
- Opt-in update mechanism (devices request updates)
- Version comparison and compatibility checking

**Rollout Control:**
- Gradual rollout with percentage-based limits
- Maximum updates per time period throttling
- Update rate control to minimize service load
- Ability to defer or rollback updates


**Monitoring & Analytics:**

Using the EmbedThis Builder cloud service, you can publish, distribute, monitor and analyze update metrics and reporting.

- View real-time update metrics and reporting via the Builder Portal
- Track successful, failed, and deferred updates
- Analyze per-product and per-version update metrics

The Builder cloud service stores and distributes update packages to devices using a secure, global CDN. The Builder update policies are evaluated per device so you can target updates to specific devices, device groups, or device properties.

The updater client works by periodically checking in with the Builder cloud service. The cloud evaluates the software update distribution policy and returns an update URL if the device qualifies. The updater then downloads, verifies, and applies the update.

**Learn More:**
- [Builder Documentation](https://www.embedthis.com/doc/builder/) - Complete Builder platform documentation
- [Software Update Guide](https://www.embedthis.com/blog/builder/software-update.html) - Detailed update workflow and features
- [Design Document](doc/DESIGN.md) - Architecture and implementation details

## Variants

This repository provides multiple implementations of a standalone **updater** program:

* **C implementation** - Production-ready command line utility and library with minimal dependencies
* **NodeJS/Bun implementation** - JavaScript alternative
* **Shell script sample** - Documentation/reference implementation (not for production use)

All implementations include:
- HTTPS enforcement
- Certificate validation
- Checksum verification (SHA-256)
- Content-Length validation
- Timeout protection
- Secure file handling

## Device Agents

The [Ioto](https://www.embedthis.com/ioto/) device agent includes the updater functionality internally. The [Appweb](https://www.embedthis.com/appweb/) and [GoAhead](https://www.embedthis.com/goahead/) web servers include this repository under their **src/updater** directories.

All devices using other embedded web servers and device agents can include this Updater to add OTA software update functionality.

## Updater Command

    updater [options] key=value,...

Where options are:

Option | Description
-|-
--cmd script        | Script to invoke to apply the update
--device ID         | Unique device ID
--file image/path   | Path to save the downloaded update (default: update.bin)
--host host.domain  | Device cloud endpoint from the Builder cloud edit panel
--product ProductID | ProductID from the Builder token list
--token TokenID     | CloudAPI access token from the Builder token list
--version SemVer    | Current device firmware version
--verbose, -v       | Trace execution

The key=value pairs can provide device specific properties that can be used by the Builder software
update policy to determine which devices receive the update.

### Example:

```Bash
updater -v --device "A123456789" \
    --host "https://ygp2t8ckqj.execute-api.ap-southeast-1.amazonaws.com" \
    --product "XABCDACC2T1234567890123455" \
    --token "TT488ETG5H1234567890123456" \
    --version "2.1.2" \
    --cmd ./apply.sh \
    pro=true
```
    
Replace the host, product and token with values from your Builder account.

## Library

You can integrate the updater as a library in your C/C++ programs:

```c
#include "updater.h"

int update(cchar *host, cchar *product, cchar *token, cchar *device,
           cchar *version, cchar *properties, cchar *path, cchar *script,
           int verbose);
```

The `update()` function performs a complete OTA update cycle:
1. Checks for available updates from the Builder service
2. Downloads the update package if available
3. Verifies the SHA-256 checksum
4. Executes the specified script to apply the update
5. Reports update status back to Builder

**Parameters:**
- `host` - Builder cloud endpoint URL
- `product` - Product ID from Builder token list
- `token` - CloudAPI access token
- `device` - Unique device identifier
- `version` - Current firmware version
- `properties` - JSON string of device properties (can be NULL)
- `path` - Local path to save downloaded update
- `script` - Path to script that applies the update (can be NULL to skip application)
- `verbose` - Enable verbose logging (0 or 1)

**Returns:** 0 on success, -1 on error

## Building

### C Implementation

```bash
make                # Build updater program
make clean          # Clean build artifacts
```

**Requirements:**
- GCC or compatible C compiler
- OpenSSL or LibreSSL development libraries
- Make

### Node.js/Bun Implementation

The JavaScript version requires Node.js 18+ or Bun and has no additional dependencies:

```bash
node src/updater.js [options]
# or
bun src/updater.js [options]
```

### Shell Script Implementation

The shell script implementation requires no additional dependencies:

```bash
./src/updater.sh [options]
```

## Tests

The test suite is located in the `test/` directory and uses the [TestMe](https://www.embedthis.com/testme/) framework. 

The test suite requires the following prerequisites:

- **Bun**: v1.2.23 or later
- **TestMe**: Test runner (installed globally)

Install Bun by following the instructions at: 

    https://bun.com/docs/installation

Install TestMe globally with:

    bun install -g --trust @embedthis/testme

Run the tests with:

    make test

or manually via the `tm` command. 

    tm

To run a specific test or group of tests, use the `tm` command with the test name.

    tm NAME

## Security Features

All implementations provide robust security:

- **HTTPS Only** - Enforces HTTPS for all network communication
- **Certificate Validation** - Validates server certificates against system CA bundle
- **Checksum Verification** - SHA-256 checksum validation of downloaded updates
- **Content-Length Validation** - Validates and enforces size limits (100MB max)
- **Timeout Protection** - Network and script execution timeouts prevent hanging
- **Secure File Handling** - Exclusive file creation with restricted permissions (0600)
- **Input Validation** - Validates all inputs and API responses
- **Error Handling** - Comprehensive error checking and reporting

## Files

File | Description
-|-
**C Implementation** |
`Makefile` | Build configuration for C updater
`main.c` | Command-line interface for C updater
`updater.c` | Core update library implementation
`updater.h` | Public API header
**JavaScript Implementation** |
`src/updater.js` | Node.js/Bun command-line updater
**Shell Script Implementation** |
`src/updater.sh` | Shell script sample utility
**Supporting Files** |
`apply.sh` | Sample update application script
`README.md` | This documentation

## Repository

You can find the source for the updater at [https://github.com/embedthis/updater](https://github.com/embedthis/updater).
