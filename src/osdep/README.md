OSdep -- Operating System Dependent Abstraction Layer
===

OSdep is a single-header C abstraction layer that provides cross-platform portability for embedded IoT applications and all EmbedThis modules. It automatically detects the target CPU architecture, operating system, compiler, and byte order at compile time, then defines a consistent set of types, macros, and system includes so that higher-level code can be written once and compiled everywhere.

OSdep has no library of its own -- the entire abstraction is delivered through `osdep.h`. Include it first in every source file:

```c
#include "osdep.h"
```

## What OSdep Provides

### Platform Detection

At compile time, OSdep examines compiler-defined preprocessor symbols and sets:

| Macro | Purpose | Example values |
|-------|---------|----------------|
| `ME_CPU` | CPU name string | `"arm64"`, `"x64"`, `"riscv"` |
| `ME_CPU_ARCH` | Numeric CPU constant | `ME_CPU_ARM64`, `ME_CPU_X64` |
| `ME_OS` | OS name string | `"linux"`, `"macosx"`, `"freertos"` |
| `ME_OS_TYPE` | Numeric OS constant | `ME_OS_LINUX`, `ME_OS_MACOSX` |
| `ME_ENDIAN` | Byte order | `ME_LITTLE_ENDIAN`, `ME_BIG_ENDIAN` |

### Supported Architectures

ARM, ARM64, x86, x64, MIPS, MIPS64, PowerPC, PowerPC64, SPARC, RISC-V, RISC-V64, Xtensa (ESP32), SuperH, TI DSP, and Itanium.

### Supported Operating Systems

Linux, macOS, FreeBSD, OpenBSD, NetBSD, Windows, VxWorks, FreeRTOS/ESP32, QNX, Solaris, AIX, HP-UX, eCos, and others.

### Portable Types

OSdep defines integer types that behave identically across all platforms:

| Type | Description |
|------|-------------|
| `ssize` | Signed size type (`ssize_t`): 64 bits on 64-bit targets, 32 bits on 32-bit targets |
| `int64` / `uint64` | 64-bit integers |
| `int32` / `uint32` | 32-bit integers |
| `int16` / `uint16` | 16-bit integers |
| `int8` / `uint8` | 8-bit integers |
| `bool` | Boolean (where not provided by the compiler) |
| `Offset` | 64-bit file offset, on every target |
| `Time` / `Ticks` | 64-bit time values |

### Compiler Abstractions

Portable macros for `inline`, `likely`/`unlikely` branch hints, `deprecated`, format-string checking, and memory alignment across GCC, Clang, and MSVC.

A printf-style declaration takes both format macros, because the compilers need the information in different places:

```c
PUBLIC int logMessage(PRINTF_FORMAT cchar *fmt, ...) PRINTF_ATTRIBUTE(1, 2);
```

### Large Files

File sizes and offsets are 64 bits on every target. 32-bit Linux defines `_FILE_OFFSET_BITS 64` before any system header unless the build sets it, and Windows maps `stat`, `fstat`, `lseek` and `off_t` to their 64-bit forms. On Windows, `O_CLOEXEC` is the CRT's no-inherit open flag.

### Feature Defaults

Compile-time feature flags with sensible defaults:

| Define | Default | Purpose |
|--------|---------|---------|
| `ME_FLOAT` | 1 | Floating point support |
| `ME_ROM` | 0 | Execute from ROM |

Override any default before including `osdep.h` or via compiler flags (`-DME_DEBUG=1`).

## Platform-Specific Code

Use the numeric constants for conditional compilation:

```c
#if ME_OS_TYPE == ME_OS_LINUX
    /* Linux-specific code */
#elif ME_OS_TYPE == ME_OS_WINDOWS
    /* Windows-specific code */
#endif

#if ME_CPU_ARCH == ME_CPU_ARM64
    /* ARM64 optimizations */
#endif
```

## Building

OSdep is a header-only module with no compiled library. The Makefile provides utility targets:

```bash
make                # No-op build (header only)
make test           # Run the tests in test/ (requires TestMe)
make doc            # Generate API documentation
make format         # Format osdep.h with uncrustify
make package        # Copy osdep.h to dist/
make clean          # Remove build artifacts
```

## Tests

`make test` runs the TestMe suite in `test/`: native tests on the build host, and checks of other targets that preprocess or syntax check `osdep.h` with the installed compilers and stub system headers. See [test/README.md](test/README.md) for what each test covers and what is only checked by preprocessing.

## API Documentation

Generate HTML API docs from the annotated header:

```bash
make doc
open doc/api/osdep.html
```

## Project Documentation

Design, architecture and reference documentation is in the [osdep repository](https://github.com/embedthis/osdep/tree/master/doc) under `doc/`. Start at [MAP.md](https://github.com/embedthis/osdep/blob/master/doc/MAP.md).

## Licensing

See [LICENSE.md](LICENSE.md) for details.
