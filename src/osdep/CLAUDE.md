# Operating System Dependent Abstraction Layer (OSDEP)

## Module Overview

The **osdep** module provides the foundational Operating System Dependent abstraction layer for all EmbedThis modules. It's the lowest-level dependency that enables cross-platform portability across embedded and desktop systems.

**Key Characteristics:**
- Foundation module consumed by all other EmbedThis modules
- Single header include (`osdep.h`) with no dependencies
- Defines platform detection, standard types, and compiler abstractions
- Critical for consistent behavior across diverse embedded platforms

## Core Functionality

### Platform Detection
- **CPU Architecture Detection**: ARM, ARM64, x86, x64, MIPS, PowerPC, SPARC, RISC-V, Xtensa, TI DSP
- **Operating System Detection**: Linux, macOS, FreeBSD, Windows, VxWorks, QNX, Solaris, AIX, ESP32/FreeRTOS
- **Compiler Detection**: GCC, Clang, MSVC, and embedded toolchains
- **Endianness Detection**: Big/little endian with runtime and compile-time support

### Type Definitions
- **Standardized Integer Types**: int8, uint8, int16, uint16, int32, uint32, int64, uint64, ssize
- **Boolean Types**: Consistent bool/true/false across C and C++
- **Wide Character Support**: Platform-appropriate wchar definitions
- **Size Types**: ssize for cross-platform size handling

### Compiler Abstractions
- **Function Attributes**: inline, deprecated, format checking
- **Optimization Hints**: likely/unlikely branch prediction
- **Memory Alignment**: Portable alignment macros
- **Debug Support**: Conditional compilation for debug builds

## Integration Usage

All EmbedThis modules depend on osdep. Include it first in your source files:

```c
#include "osdep.h"
```

### Type Usage Examples
```c
// Use osdep types instead of standard C types
ssize len = slen(str);          // Not strlen()
uint32 value = 0x12345678;      // Not unsigned int
int64 timestamp = getTime();    // Not long long
```

### Platform-Specific Code
```c
#if ME_OS == ME_OS_LINUX
    // Linux-specific code
#elif ME_OS == ME_OS_WINDOWS
    // Windows-specific code
#endif

#if ME_CPU_ARCH == ME_CPU_ARM64
    // ARM64 optimizations
#endif
```

## Module-Specific Build Options

Beyond standard build commands (see `../CLAUDE.md`), osdep supports:

```bash
# Cross-compilation for specific platforms
OS=linux ARCH=arm64 make        # ARM64 Linux build
OS=esp32 ARCH=xtensa make       # ESP32 build

# Minimal embedded configuration
PROFILE=minimal make            # Smallest footprint
```

## Key Files

- **`osdep.h`** - Primary header with all abstractions
- **`osdep.c`** - Minimal runtime support (mostly empty)
- **`main.me`** - Platform detection and configuration logic
- **`doc/osdep.dox`** - API documentation source

## Testing

```bash
# Run osdep-specific tests
make test

# Test specific platform combinations
OS=windows ARCH=x64 make test
ARCH=arm64 make test
```

Tests verify platform detection accuracy and type size consistency across targets.

## Common Issues

### Cross-Compilation
Ensure target platform tools are in PATH before building:
```bash
export PATH=/path/to/arm-toolchain/bin:$PATH
OS=linux ARCH=arm64 make
```

### Embedded Targets
Some platforms require specific compiler flags:
```bash
# ESP32 example
OS=esp32 ARCH=xtensa CC=xtensa-esp32-elf-gcc make
```

## Integration Notes

- **All modules MUST include osdep.h first** before any other includes
- **Use osdep types** (ssize, int32, etc.) instead of standard C types
- **Platform detection macros** are available at compile-time for conditional code
- **Runtime functions** should use osdep-provided alternatives when available

## Additional Resources

- **Parent Project**: See `../CLAUDE.md` for general build commands, testing procedures, and EmbedThis architecture
- **API Documentation**: Generated via `make doc` → `doc/index.html`