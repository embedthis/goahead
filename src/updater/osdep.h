/*
    osdep.h - Operating system dependent abstraction layer.

    This header provides a comprehensive cross-platform abstraction layer for embedded IoT applications.
    It defines standard types, platform detection constants, compiler abstractions, and operating system compatibility
    macros to enable portability across diverse embedded and desktop systems. This is the foundational module consumed
    by all other EmbedThis modules and must be included first in any source file. The module automatically detects
    the target platform's CPU architecture, operating system, compiler, and endianness to provide consistent behavior
    across ARM, x86, MIPS, PowerPC, SPARC, RISC-V, Xtensa, and other architectures running on Linux, macOS, Windows,
    VxWorks, FreeRTOS, ESP32, and other operating systems.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

#ifndef _h_OSDEP
#define _h_OSDEP 1

/********************************** Includes **********************************/

#ifndef OSDEP_USE_ME
#define OSDEP_USE_ME 1
#endif

#if OSDEP_USE_ME
#include "me.h"
#endif

/******************************* Default Features *****************************/
/*
    Default features
 */
#ifndef ME_COM_SSL
    #define ME_COM_SSL 0                /**< Build without SSL support */
#endif
#ifndef ME_DEBUG
    #define ME_DEBUG 0                  /**< Default to a debug build */
#endif
#ifndef ME_FLOAT
    #define ME_FLOAT 1                  /**< Build with floating point support */
#endif
#ifndef ME_ROM
    #define ME_ROM 0                    /**< Build for execute from ROM */
#endif

/**
    @section CPU Architecture Detection

    CPU architecture constants for cross-platform compilation. These constants are used with the ME_CPU_ARCH macro
    to determine the target processor architecture at compile time. The osdep module automatically detects the
    architecture based on compiler-defined symbols and sets ME_CPU_ARCH to the appropriate value.
*/

/**
    Unknown or unsupported CPU architecture.
    @stability Stable
*/
#define ME_CPU_UNKNOWN     0

/**
    ARM 32-bit architecture (ARMv7 and earlier).
    @description Covers ARM Cortex-A, Cortex-R, and Cortex-M series processors commonly used in embedded systems.
    @stability Stable
*/
#define ME_CPU_ARM         1

/**
    ARM 64-bit architecture (ARMv8 and later).
    @description Covers ARM Cortex-A64 and newer 64-bit ARM processors including Apple Silicon and server processors.
    @stability Stable
*/
#define ME_CPU_ARM64       2

/**
    Intel Itanium (IA-64) architecture.
    @description Legacy 64-bit architecture primarily used in high-end servers and workstations.
    @stability Stable
*/
#define ME_CPU_ITANIUM     3

/**
    Intel x86 32-bit architecture.
    @description Standard 32-bit x86 processors including Intel and AMD variants.
    @stability Stable
*/
#define ME_CPU_X86         4

/**
    Intel/AMD x86-64 architecture.
    @description 64-bit x86 processors including Intel x64 and AMD64 variants.
    @stability Stable
*/
#define ME_CPU_X64         5

/**
    MIPS 32-bit architecture.
    @description MIPS processors commonly used in embedded systems and networking equipment.
    @stability Stable
*/
#define ME_CPU_MIPS        6

/**
    MIPS 64-bit architecture.
    @description 64-bit MIPS processors used in high-performance embedded and server applications.
    @stability Stable
*/
#define ME_CPU_MIPS64      7

/**
    PowerPC 32-bit architecture.
    @description IBM PowerPC processors used in embedded systems and legacy workstations.
    @stability Stable
*/
#define ME_CPU_PPC         8

/**
    PowerPC 64-bit architecture.
    @description 64-bit PowerPC processors used in high-performance computing and servers.
    @stability Stable
*/
#define ME_CPU_PPC64       9

/**
    SPARC architecture.
    @description Sun/Oracle SPARC processors used in servers and workstations.
    @stability Stable
*/
#define ME_CPU_SPARC       10

/**
    Texas Instruments DSP architecture.
    @description TI digital signal processors used in specialized embedded applications.
    @stability Stable
*/
#define ME_CPU_TIDSP       11

/**
    SuperH architecture.
    @description Hitachi/Renesas SuperH processors used in embedded systems.
    @stability Stable
*/
#define ME_CPU_SH          12

/**
    RISC-V 32-bit architecture.
    @description Open-source RISC-V processors gaining popularity in embedded and IoT applications.
    @stability Stable
*/
#define ME_CPU_RISCV       13

/**
    RISC-V 64-bit architecture.
    @description 64-bit RISC-V processors for high-performance applications.
    @stability Stable
*/
#define ME_CPU_RISCV64     14

/**
    Xtensa architecture including ESP32.
    @description Tensilica Xtensa processors, notably used in Espressif ESP32 Wi-Fi/Bluetooth microcontrollers.
    @stability Stable
*/
#define ME_CPU_XTENSA      15

/**
    @section Byte Order Detection

    Endianness constants for cross-platform byte order handling. These constants are used with the ME_ENDIAN macro
    to determine the target platform's byte ordering at compile time. Little endian stores the least significant
    byte first, while big endian stores the most significant byte first.
*/

/**
    Little endian byte ordering.
    @description In little endian format, the least significant byte is stored at the lowest memory address.
    Most x86, ARM, and RISC-V processors use little endian ordering.
    @stability Stable
*/
#define ME_LITTLE_ENDIAN   1

/**
    Big endian byte ordering.
    @description In big endian format, the most significant byte is stored at the lowest memory address.
    SPARC, some MIPS, and PowerPC processors traditionally use big endian ordering.
    @stability Stable
*/
#define ME_BIG_ENDIAN      2

/**
    @section Platform Detection Logic

    Automatic detection of CPU architecture and endianness based on compiler-defined preprocessor symbols.
    The osdep module examines compiler-specific macros to determine the target platform and sets the
    appropriate ME_CPU, ME_CPU_ARCH, and CPU_ENDIAN macros. The default endianness can be overridden
    by the build system using configure --endian big|little.
*/
#if defined(__alpha__)
    #define ME_CPU "alpha"
    #define ME_CPU_ARCH ME_CPU_ALPHA
    #define CPU_ENDIAN ME_LITTLE_ENDIAN

#elif defined(__arm64__) || defined(__aarch64__)
    #define ME_CPU "arm64"
    #define ME_CPU_ARCH ME_CPU_ARM64
    #define CPU_ENDIAN ME_LITTLE_ENDIAN

#elif defined(__arm__)
    #define ME_CPU "arm"
    #define ME_CPU_ARCH ME_CPU_ARM
    #define CPU_ENDIAN ME_LITTLE_ENDIAN

#elif defined(__x86_64__) || defined(_M_AMD64)
    #define ME_CPU "x64"
    #define ME_CPU_ARCH ME_CPU_X64
    #define CPU_ENDIAN ME_LITTLE_ENDIAN

#elif defined(__i386__) || defined(__i486__) || defined(__i585__) || defined(__i686__) || defined(_M_IX86)
    #define ME_CPU "x86"
    #define ME_CPU_ARCH ME_CPU_X86
    #define CPU_ENDIAN ME_LITTLE_ENDIAN

#elif defined(_M_IA64)
    #define ME_CPU "ia64"
    #define ME_CPU_ARCH ME_CPU_ITANIUM
    #define CPU_ENDIAN ME_LITTLE_ENDIAN

#elif defined(__mips__)
    #define ME_CPU "mips"
    #define ME_CPU_ARCH ME_CPU_MIPS
    #define CPU_ENDIAN ME_BIG_ENDIAN

#elif defined(__mips64)
    #define ME_CPU "mips64"
    #define ME_CPU_ARCH ME_CPU_MIPS64
    #define CPU_ENDIAN ME_BIG_ENDIAN

#elif defined(__ppc__) || defined(__powerpc__) || defined(__ppc)
    #define ME_CPU "ppc"
    #define ME_CPU_ARCH ME_CPU_PPC
    #define CPU_ENDIAN ME_BIG_ENDIAN

#elif defined(__ppc64__)
    #define CPU "ppc64"
    #define CPU_ARCH CPU_PPC64

#elif defined(__sparc__)
    #define ME_CPU "sparc"
    #define ME_CPU_ARCH ME_CPU_SPARC
    #define CPU_ENDIAN ME_BIG_ENDIAN

#elif defined(_TMS320C6X)
    #define TIDSP 1
    #define ME_CPU "tidsp"
    #define ME_CPU_ARCH ME_CPU_SPARC
    #define CPU_ENDIAN ME_LITTLE_ENDIAN

#elif defined(__sh__)
    #define ME_CPU "sh"
    #define ME_CPU_ARCH ME_CPU_SH
    #define CPU_ENDIAN ME_LITTLE_ENDIAN

#elif defined(__riscv_32)
    #define ME_CPU "riscv"
    #define ME_CPU_ARCH ME_CPU_RISCV
    #define ME_CPU_ENDIAN ME_LITTLE_ENDIAN

#elif defined(__riscv_64)
    #define ME_CPU "riscv64"
    #define ME_CPU_ARCH ME_CPU_RISCV64
    #define ME_CPU_ENDIAN ME_LITTLE_ENDIAN

#elif defined(__XTENSA__)
    #define ME_CPU "xtensa"
    #define ME_CPU_ARCH ME_CPU_XTENSA
    #define ME_CPU_ENDIAN ME_LITTLE_ENDIAN
#else
    #error "Cannot determine CPU type in osdep.h"
#endif

/*
    Set the default endian if me.h does not define it explicitly
 */
#ifndef ME_ENDIAN
    #define ME_ENDIAN CPU_ENDIAN
#endif

/**
    @section Operating System Detection

    Automatic detection of the target operating system based on compiler-defined preprocessor symbols.
    The osdep module examines compiler-specific OS macros and sets appropriate platform flags including
    ME_OS, ME_UNIX_LIKE, ME_WIN_LIKE, ME_BSD_LIKE, and threading support flags. Most operating systems
    provide standard compiler symbols, with VxWorks being a notable exception requiring explicit detection.
*/
#if defined(__APPLE__)
    #define ME_OS "macosx"
    #define MACOSX 1
    #define POSIX 1
    #define ME_UNIX_LIKE 1
    #define ME_WIN_LIKE 0
    #define ME_BSD_LIKE 1
    #define HAS_USHORT 1
    #define HAS_UINT 1

#elif defined(__linux__)
    #define ME_OS "linux"
    #define LINUX 1
    #define POSIX 1
    #define ME_UNIX_LIKE 1
    #define ME_WIN_LIKE 0
    #define PTHREADS 1

#elif defined(__FreeBSD__)
    #define ME_OS "freebsd"
    #define FREEBSD 1
    #define POSIX 1
    #define ME_UNIX_LIKE 1
    #define ME_WIN_LIKE 0
    #define ME_BSD_LIKE 1
    #define HAS_USHORT 1
    #define HAS_UINT 1
    #define PTHREADS 1

#elif defined(__OpenBSD__)
    #define ME_OS "openbsd"
    #define OPENBSD 1
    #define POSIX 1
    #define ME_UNIX_LIKE 1
    #define ME_WIN_LIKE 0
    #define ME_BSD_LIKE 1
    #define PTHREADS 1

#elif defined(_WIN32)
    #define ME_OS "windows"
    #define WINDOWS 1
    #define POSIX 1
    #define ME_UNIX_LIKE 0
    #define ME_WIN_LIKE 1

#elif defined(__OS2__)
    #define ME_OS "os2"
    #define OS2 0
    #define ME_UNIX_LIKE 0
    #define ME_WIN_LIKE 0

#elif defined(MSDOS) || defined(__DME__)
    #define ME_OS "msdos"
    #define WINDOWS 0
    #define ME_UNIX_LIKE 0
    #define ME_WIN_LIKE 0

#elif defined(__NETWARE_386__)
    #define ME_OS "netware"
    #define NETWARE 0
    #define ME_UNIX_LIKE 0
    #define ME_WIN_LIKE 0

#elif defined(__bsdi__)
    #define ME_OS "bsdi"
    #define BSDI 1
    #define POSIX 1
    #define ME_UNIX_LIKE 1
    #define ME_WIN_LIKE 0
    #define ME_BSD_LIKE 1
    #define PTHREADS 1

#elif defined(__NetBSD__)
    #define ME_OS "netbsd"
    #define NETBSD 1
    #define POSIX 1
    #define ME_UNIX_LIKE 1
    #define ME_WIN_LIKE 0
    #define ME_BSD_LIKE 1
    #define PTHREADS 1

#elif defined(__QNX__)
    #define ME_OS "qnx"
    #define QNX 0
    #define ME_UNIX_LIKE 0
    #define ME_WIN_LIKE 0
    #define PTHREADS 1

#elif defined(__hpux)
    #define ME_OS "hpux"
    #define HPUX 1
    #define POSIX 1
    #define ME_UNIX_LIKE 1
    #define ME_WIN_LIKE 0
    #define PTHREADS 1

#elif defined(_AIX)
    #define ME_OS "aix"
    #define AIX 1
    #define POSIX 1
    #define ME_UNIX_LIKE 1
    #define ME_WIN_LIKE 0
    #define PTHREADS 1

#elif defined(__CYGWIN__)
    #define ME_OS "cygwin"
    #define CYGWIN 1
    #define ME_UNIX_LIKE 1
    #define ME_WIN_LIKE 0

#elif defined(__VMS)
    #define ME_OS "vms"
    #define VMS 1
    #define ME_UNIX_LIKE 0
    #define ME_WIN_LIKE 0

#elif defined(VXWORKS)
    /* VxWorks does not have a pre-defined symbol */
    #define ME_OS "vxworks"
    #define POSIX 1
    #define ME_UNIX_LIKE 0
    #define ME_WIN_LIKE 0
    #define HAS_USHORT 1
    #define PTHREADS 1

#elif defined(ECOS)
    /* ECOS may not have a pre-defined symbol */
    #define ME_OS "ecos"
    #define POSIX 1
    #define ME_UNIX_LIKE 0
    #define ME_WIN_LIKE 0

#elif defined(TIDSP)
    #define ME_OS "tidsp"
    #define ME_UNIX_LIKE 0
    #define ME_WIN_LIKE 0
    #define HAS_INT32 1

#elif defined(ESP_PLATFORM)
    #define ME_OS "freertos"
    #define FREERTOS 1
    #define ESP32 1
    #define POSIX 1
    #define ME_UNIX_LIKE 0
    #define ME_WIN_LIKE 0
    #define PLATFORM "esp"
    #define PTHREADS 1
    #define HAS_INT32 1

#elif defined(INC_FREERTOS_H) || defined(FREERTOS_CONFIG_H)
    #define ME_OS "freertos"
    #define FREERTOS 1
    #define POSIX 1
    #define ME_UNIX_LIKE 0
    #define ME_WIN_LIKE 0
    #define PTHREADS 1
    #define HAS_INT32 1

#elif defined(ARDUINO)
    #define ME_OS "freertos"
    #define FREERTOS 1
    #define POSIX 1
    #define ME_UNIX_LIKE 0
    #define ME_WIN_LIKE 0
    #define PTHREADS 1
    #define HAS_INT32 1
#endif

/**
    @section Word Size Detection

    Automatic detection of the target platform's word size (32-bit or 64-bit) based on compiler-defined
    preprocessor symbols. This sets ME_64 and ME_WORDSIZE macros used throughout the codebase for
    size-dependent operations and pointer arithmetic.
*/
#if __WORDSIZE == 64 || __amd64 || __x86_64 || __x86_64__ || _WIN64 || __mips64 || __arch64__ || __arm64__ || __aarch64__
    /**
        64-bit platform indicator.
        @description Set to 1 on 64-bit platforms, 0 on 32-bit platforms.
        @stability Stable
    */
    #define ME_64 1
    /**
        Platform word size in bits.
        @description Set to 64 on 64-bit platforms, 32 on 32-bit platforms.
        @stability Stable
    */
    #define ME_WORDSIZE 64
#else
    #define ME_64 0
    #define ME_WORDSIZE 32
#endif

/**
    @section Unicode Support

    Unicode character support configuration. The ME_CHAR_LEN macro determines the wide character size
    and enables appropriate Unicode handling. This affects string literals and character processing
    throughout the system.
*/
#ifndef ME_CHAR_LEN
    /**
        Character length for Unicode support.
        @description Set to 1 for ASCII/UTF-8, 2 for UTF-16, or 4 for UTF-32.
        @stability Stable
    */
    #define ME_CHAR_LEN 1
#endif
#if ME_CHAR_LEN == 4
    /**
        Wide character type for 32-bit Unicode (UTF-32).
        @stability Stable
    */
    typedef int wchar;
    /**
        Unicode string literal macro for UTF-32.
        @param s String literal to convert to Unicode
        @stability Stable
    */
    #define UT(s) L ## s
    #define UNICODE 1
#elif ME_CHAR_LEN == 2
    /**
        Wide character type for 16-bit Unicode (UTF-16).
        @stability Stable
    */
    typedef short wchar;
    /**
        Unicode string literal macro for UTF-16.
        @param s String literal to convert to Unicode
        @stability Stable
    */
    #define UT(s) L ## s
    #define UNICODE 1
#else
    /**
        Wide character type for ASCII/UTF-8.
        @stability Stable
    */
    typedef char wchar;
    /**
        String literal macro for ASCII/UTF-8 (no conversion).
        @param s String literal
        @stability Stable
    */
    #define UT(s) s
#endif

#define ME_PLATFORM ME_OS "-" ME_CPU "-" ME_PROFILE

/********************************* O/S Includes *******************************/
/*
    Out-of-order definitions and includes. Order really matters in this section.
 */
#if WINDOWS
    #undef      _CRT_SECURE_NO_DEPRECATE
    #define     _CRT_SECURE_NO_DEPRECATE 1
    #undef      _CRT_SECURE_NO_WARNINGS
    #define     _CRT_SECURE_NO_WARNINGS 1
    #define     _WINSOCK_DEPRECATED_NO_WARNINGS 1
    #ifndef     _WIN32_WINNT
        /* Target Windows 7 by default */
        #define _WIN32_WINNT 0x601
    #endif
    /*
        Work-around to allow the windows 7.* SDK to be used with VS 2012
        MSC_VER 1800 2013
        MSC_VER 1900 2015
     */
    #if _MSC_VER >= 1700
        #define SAL_SUPP_H
        #define SPECSTRING_SUPP_H
    #endif
#endif

#if LINUX
    /*
        Use GNU extensions for:
            RTLD_DEFAULT for dlsym()
     */
    #define __STDC_WANT_LIB_EXT2__ 1
    #define _GNU_SOURCE 1
    #define __USE_XOPEN 1
    #if !ME_64
        #define _LARGEFILE64_SOURCE 1
        #ifdef __USE_FILE_OFFSET64
            #define _FILE_OFFSET_BITS 64
        #endif
    #endif
#endif

#if VXWORKS
    #ifndef _VSB_CONFIG_FILE
        #define _VSB_CONFIG_FILE "vsbConfig.h"
    #endif
    #include    <vxWorks.h>
#endif

#if ME_WIN_LIKE
    #include    <winsock2.h>
    #include    <windows.h>
    #include    <winbase.h>
    #include    <winuser.h>
    #include    <shlobj.h>
    #include    <shellapi.h>
    #include    <wincrypt.h>
    #include    <ws2tcpip.h>
    #include    <conio.h>
    #include    <process.h>
    #include    <windows.h>
    #include    <shlobj.h>
    #include    <malloc.h>
    #if _MSC_VER >= 1800
        #include    <stdbool.h>
    #endif
    #if ME_DEBUG
        #include <crtdbg.h>
    #endif
#endif

/*
    Includes in alphabetic order
 */
    #include    <ctype.h>
#if ME_WIN_LIKE
    #include    <direct.h>
#else
    #include    <dirent.h>
#endif
#if ME_UNIX_LIKE
    #include    <dlfcn.h>
#endif
    #include    <fcntl.h>
    #include    <errno.h>
#if ME_FLOAT
    #include    <float.h>
    #define __USE_ISOC99 1
    #include    <math.h>
#endif
#if ME_UNIX_LIKE
    #include    <grp.h>
#endif
#if ME_WIN_LIKE
    #include    <io.h>
#endif
#if MACOSX || LINUX
    #include    <libgen.h>
#endif
    #include    <limits.h>
#if ME_UNIX_LIKE || VXWORKS
    #include    <sys/socket.h>
    #include    <arpa/inet.h>
    #include    <netdb.h>
    #include    <net/if.h>
    #include    <netinet/in.h>
    #include    <netinet/tcp.h>
    #include    <netinet/ip.h>
#endif
#if ME_UNIX_LIKE
    #include    <pthread.h>
    #include    <pwd.h>
#if !CYGWIN
    #include    <resolv.h>
#endif
#endif
#if ME_BSD_LIKE
    #include    <readpassphrase.h>
    #include    <sys/sysctl.h>
    #include    <sys/event.h>
#endif
    #include    <setjmp.h>
    #include    <signal.h>
    #include    <stdarg.h>
    #include    <stddef.h>
    #include    <stdint.h>
    #include    <stdio.h>
    #include    <stdlib.h>
    #include    <string.h>
#if ME_UNIX_LIKE
    #include    <syslog.h>
#endif
#if !TIDSP
    #include    <sys/stat.h>
    #include    <sys/types.h>
#endif
#if ME_UNIX_LIKE
    #include    <sys/ioctl.h>
    #include    <sys/mman.h>
    #include    <sys/resource.h>
    #include    <sys/select.h>
    #include    <sys/time.h>
    #include    <sys/times.h>
    #include    <sys/utsname.h>
    #include    <sys/uio.h>
    #include    <sys/wait.h>
    #include    <poll.h>
    #include    <unistd.h>
#endif
    #include    <time.h>
#if !VXWORKS && !TIDSP
    #include    <wchar.h>
#endif

/*
    Extra includes per O/S
 */
#if CYGWIN
    #include    "w32api/windows.h"
    #include    "sys/cygwin.h"
#endif
#if LINUX
    #include <linux/version.h>
    #if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
        #include    <sys/epoll.h>
    #endif
    #include    <sys/prctl.h>
    #if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,22)
        #include    <sys/eventfd.h>
    #endif
    #if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,13)
        #define HAS_INOTIFY 1
        #include    <sys/inotify.h>
    #else
        #define HAS_INOTIFY 0
    #endif
    #if !__UCLIBC__
        #include    <sys/sendfile.h>
    #endif
#endif
#if MACOSX
    #include    <stdbool.h>
    #include    <mach-o/dyld.h>
    #include    <mach-o/dyld.h>
    #include    <mach/mach_init.h>
    #include    <mach/mach_time.h>
    #include    <mach/task.h>
    #include    <libkern/OSAtomic.h>
    #include    <net/if_dl.h>
#endif
#if VXWORKS
    #include    <vxWorks.h>
    #include    <envLib.h>
    #include    <iosLib.h>
    #include    <loadLib.h>
    #include    <selectLib.h>
    #include    <sockLib.h>
    #include    <inetLib.h>
    #include    <ioLib.h>
    #include    <pipeDrv.h>
    #include    <hostLib.h>
    #include    <sysSymTbl.h>
    #include    <sys/fcntlcom.h>
    #include    <tickLib.h>
    #include    <taskHookLib.h>
    #include    <unldLib.h>
    #if _WRS_VXWORKS_MAJOR >= 6
        #include    <taskLibCommon.h>
        #include    <wait.h>
    #endif
    #if _WRS_VXWORKS_MAJOR > 6 || (_WRS_VXWORKS_MAJOR == 6 && _WRS_VXWORKS_MINOR >= 8)
        #include    <symSync.h>
        #include    <vxAtomicLib.h>
    #endif
#endif

#if TIDSP
    #include    <mathf.h>
    #include    <netmain.h>
    #include    <nettools/inc/dnsif.h>
    #include    <socket.h>
    #include    <file.h>
#endif

#if ME_COMPILER_HAS_ATOMIC
    #ifndef __cplusplus
        #include   <stdatomic.h>
    #endif
#endif

#if FREERTOS
    #include <string.h>
    #include "time.h"
#if ESP32
    #include "freertos/FreeRTOS.h"
    #include "freertos/event_groups.h"
    #include "freertos/task.h"
#else
    #include "FreeRTOS.h"
    #include "event_groups.h"
    #include "task.h"
#endif /* ESP32 */
#endif

#if ESP32
    #include "esp_system.h"
    #include "esp_log.h"
    #include "esp_heap_caps.h"
    #include "esp_err.h"
    #include "esp_event.h"
    #include "esp_log.h"
    #include "esp_system.h"
    #include "esp_heap_caps.h"
    #include "esp_psram.h"
    #include "esp_pthread.h"
    #include "esp_littlefs.h"
    #include "esp_crt_bundle.h"
    #include "esp_pthread.h"
    #include "esp_wifi.h"
    #include "esp_netif.h"
    #include "nvs_flash.h"
    #include "lwip/err.h"
    #include "lwip/sockets.h"
    #include "lwip/sys.h"
    #include "lwip/netdb.h"
#endif

#if PTHREADS
    #include <pthread.h>
#endif

/**
    @section Type Definitions

    Cross-platform type definitions for consistent behavior across different operating systems and compilers.
    These types provide fixed-size integers, enhanced character types, and platform-specific abstractions
    for sockets, file offsets, and time values. All types are designed to be null-tolerant and provide
    consistent sizing across 32-bit and 64-bit platforms.
*/
#ifndef HAS_BOOL
    #ifndef __cplusplus
        #if !MACOSX && !FREERTOS
            #define HAS_BOOL 1
            /**
                Boolean data type.
                @description Provides consistent boolean type across platforms. Uses char underlying type for
                compatibility with systems lacking native bool support. Should be used with true/false constants.
                @stability Stable
             */
            #if !WINDOWS || ((_MSC_VER < 1800) && !defined(bool))
                /* Bool introduced via stdbool in VS 2015 */
                typedef char bool;
            #endif
        #endif
    #endif
#endif

#ifndef HAS_UCHAR
    #define HAS_UCHAR 1
    /**
        Unsigned 8-bit character type.
        @description Provides explicit unsigned char semantics for byte manipulation and binary data handling.
        @stability Stable
     */
    typedef unsigned char uchar;
#endif

#ifndef HAS_SCHAR
    #define HAS_SCHAR 1
    /**
        Signed 8-bit character type.
        @description Provides explicit signed char semantics when the sign of char values matters.
        @stability Stable
     */
    typedef signed char schar;
#endif

#ifndef HAS_CCHAR
    #define HAS_CCHAR 1
    /**
        Constant character pointer type.
        @description Commonly used for read-only string parameters and immutable text data.
        @stability Stable
     */
    typedef const char cchar;
#endif

#ifndef HAS_CUCHAR
    #define HAS_CUCHAR 1
    /**
        Constant unsigned character type.
        @description Provides read-only access to unsigned byte data.
        @stability Stable
     */
    typedef const unsigned char cuchar;
#endif

#ifndef HAS_USHORT
    #define HAS_USHORT 1
    /**
        Unsigned short data type.
        @stability Stable
     */
    typedef unsigned short ushort;
#endif

#ifndef HAS_CUSHORT
    #define HAS_CUSHORT 1
    /**
        Constant unsigned short data type.
        @stability Stable
     */
    typedef const unsigned short cushort;
#endif

#ifndef HAS_CVOID
    #define HAS_CVOID 1
    /**
        Constant void data type.
        @stability Stable
     */
    typedef const void cvoid;
#endif

#ifndef HAS_INT8
    #define HAS_INT8 1
    /**
        Signed 8-bit integer type.
        @description Guaranteed 8-bit signed integer (-128 to 127) for precise byte-level operations.
        @stability Stable
     */
    typedef char int8;
#endif

#ifndef HAS_UINT8
    #define HAS_UINT8 1
    /**
        Unsigned 8-bit integer type.
        @description Guaranteed 8-bit unsigned integer (0 to 255) for byte manipulation and flags.
        @stability Stable
     */
    typedef unsigned char uint8;
#endif

#ifndef HAS_INT16
    #define HAS_INT16 1
    /**
        Signed 16-bit integer type.
        @description Guaranteed 16-bit signed integer (-32,768 to 32,767) for network protocols and compact data.
        @stability Stable
     */
    typedef short int16;
#endif

#ifndef HAS_UINT16
    #define HAS_UINT16 1
    /**
        Unsigned 16-bit integer type.
        @description Guaranteed 16-bit unsigned integer (0 to 65,535) for ports, packet sizes, and compact counters.
        @stability Stable
     */
    typedef unsigned short uint16;
#endif

#ifndef HAS_INT32
    #define HAS_INT32 1
    /**
        Signed 32-bit integer type.
        @description Guaranteed 32-bit signed integer for general-purpose arithmetic and system values.
        @stability Stable
     */
    typedef int int32;
#endif

#ifndef HAS_UINT32
    #define HAS_UINT32 1
    /**
        Unsigned 32-bit integer type.
        @description Guaranteed 32-bit unsigned integer for addresses, large counters, and hash values.
        @stability Stable
     */
    typedef unsigned int uint32;
#endif

#ifndef HAS_UINT
    #define HAS_UINT 1
    /**
        Unsigned integer (machine dependent bit size) data type.
        @stability Stable
     */
    typedef unsigned int uint;
#endif

#ifndef HAS_ULONG
    #define HAS_ULONG 1
    /**
        Unsigned long (machine dependent bit size) data type.
        @stability Stable
     */
    typedef unsigned long ulong;
#endif

#ifndef HAS_CINT
    #define HAS_CINT 1
    /**
        Constant int data type.
        @stability Stable
     */
    typedef const int cint;
#endif

#ifndef HAS_SSIZE
    #define HAS_SSIZE 1
    #if ME_WIN_LIKE
        typedef SSIZE_T ssize;
        typedef SSIZE_T ssize_t;
    #else
        /**
            Signed size type for memory and I/O operations.
            @description Platform-appropriate signed integer type large enough to hold array indices, memory sizes,
            and I/O transfer counts. Can represent negative values for error conditions. Equivalent to size_t but signed.
            @stability Stable
         */
        typedef ssize_t ssize;
    #endif
#endif

/**
    Windows uses uint for write/read counts (Ugh!)
    @stability Stable
 */
#if ME_WIN_LIKE
    typedef uint wsize;
#else
    typedef ssize wsize;
#endif

#ifndef HAS_INT64
    #if ME_UNIX_LIKE
        __extension__ typedef long long int int64;
    #elif VXWORKS || DOXYGEN
        /**
            Integer 64 bit data type.
            @stability Stable
         */
        typedef long long int int64;
    #elif ME_WIN_LIKE
        typedef __int64 int64;
    #else
        typedef long long int int64;
    #endif
#endif

#ifndef HAS_UINT64
    #if ME_UNIX_LIKE
        __extension__ typedef unsigned long long int uint64;
    #elif VXWORKS || DOXYGEN
        typedef unsigned long long int uint64;
    #elif ME_WIN_LIKE
        typedef unsigned __int64 uint64;
    #else
        typedef unsigned long long int uint64;
    #endif
#endif

/**
    Signed 64-bit file offset type.
    @description Supports large files greater than 4GB in size on all systems. Used for file positioning,
    seeking, and size calculations. Always 64-bit regardless of platform word size.
    @stability Stable
 */
typedef int64 Offset;

#if DOXYGEN
    /**
        Size to hold the length of a socket address
        @stability Stable
     */
    typedef int Socklen;
#elif VXWORKS
    typedef int Socklen;
#else
    typedef socklen_t Socklen;
#endif

#if DOXYGEN || ME_UNIX_LIKE || VXWORKS
    /**
        Argument for sockets
        @stability Stable
    */
    typedef int Socket;
    #ifndef SOCKET_ERROR
        #define SOCKET_ERROR -1
    #endif
    #define SOCKET_ERROR -1
    #ifndef INVALID_SOCKET
        #define INVALID_SOCKET -1
    #endif
#elif ME_WIN_LIKE
    typedef SOCKET Socket;
#elif TIDSP
    typedef SOCKET Socket;
    #define SOCKET_ERROR INVALID_SOCKET
#else
    typedef int Socket;
    #ifndef SOCKET_ERROR
        #define SOCKET_ERROR -1
    #endif
    #ifndef INVALID_SOCKET
        #define INVALID_SOCKET -1
    #endif
#endif

/**
    Absolute time in milliseconds since Unix epoch.
    @description Time value representing milliseconds since January 1, 1970 UTC (Unix epoch).
    Used for timestamps, timeouts, and absolute time calculations across the system.
    @stability Stable
*/
typedef int64 Time;

/**
    Relative time in milliseconds for durations and intervals.
    @description Elapsed time measurement in milliseconds from an arbitrary starting point.
    Used for timeouts, delays, performance measurements, and relative time calculations.
    @stability Stable
 */
typedef int64 Ticks;

/**
    Time/Ticks units per second (milliseconds)
    @stability Stable
 */
#define TPS 1000

/**
    @section Utility Macros and Constants

    Common macros and constants for bit manipulation, limits, and cross-platform compatibility.
    These provide consistent behavior for mathematical operations, type introspection, and
    platform-specific value definitions.
*/

#ifndef BITSPERBYTE
    /**
        Number of bits per byte.
        @description Standard definition for bits in a byte, typically 8 on all modern platforms.
        @stability Stable
    */
    #define BITSPERBYTE     ((int) (8 * sizeof(char)))
#endif

#ifndef BITS
    /**
        Calculate number of bits in a data type.
        @description Macro to determine the total number of bits in any data type at compile time.
        @param type Data type to calculate bits for
        @return Number of bits in the specified type
        @stability Stable
    */
    #define BITS(type)      ((int) (BITSPERBYTE * (int) sizeof(type)))
#endif

#if ME_FLOAT
    #ifndef MAXFLOAT
        #if ME_WIN_LIKE
            #define MAXFLOAT        DBL_MAX
        #else
            #define MAXFLOAT        FLT_MAX
        #endif
    #endif
    #if VXWORKS
        #undef isnan
        #define isnan(n)  ((n) != (n))
        #define isnanf(n) ((n) != (n))
        #if defined(__GNUC__)
            #define isinf(n)  __builtin_isinf(n)
            #define isinff(n) __builtin_isinff(n)
        #else
            #include <math.h>
            #define isinf(n)  ((n) == HUGE_VAL || (n) == -HUGE_VAL)
            #define isinff(n) ((n) == HUGE_VALF || (n) == -HUGE_VALF)
        #endif
    #endif
    #if ME_WIN_LIKE
        #define isNan(f) (_isnan(f))
    #elif VXWORKS || MACOSX || LINUX
        #define isNan(f) (isnan(f))
    #else
        #define isNan(f) (fpclassify(f) == FP_NAN)
    #endif
#endif

#if ME_WIN_LIKE
    #define INT64(x)    (x##i64)
    #define UINT64(x)   (x##Ui64)
#else
    #define INT64(x)    (x##LL)
    #define UINT64(x)   (x##ULL)
#endif

#ifndef MAXINT
#if INT_MAX
    #define MAXINT      INT_MAX
#else
    #define MAXINT      0x7fffffff
#endif
#endif

#ifndef MAXUINT
#if UINT_MAX
    #define MAXUINT     UINT_MAX
#else
    #define MAXUINT     0xffffffff
#endif
#endif

#ifndef MAXINT64
    #define MAXINT64    INT64(0x7fffffffffffffff)
#endif
#ifndef MAXUINT64
    #define MAXUINT64   INT64(0xffffffffffffffff)
#endif

#if SSIZE_MAX
    #define MAXSSIZE     ((ssize) SSIZE_MAX)
#elif ME_64
    #define MAXSSIZE     INT64(0x7fffffffffffffff)
#else
    #define MAXSSIZE     MAXINT
#endif

#ifndef SSIZE_MAX
    #define SSIZE_MAX    MAXSSIZE
#endif

#if OFF_T_MAX
    #define MAXOFF       OFF_T_MAX
#else
    #define MAXOFF       INT64(0x7fffffffffffffff)
#endif

/*
    Safe time max value to avoid overflows
 */
#define MAXTIME         (MAXINT64 - MAXINT)

/*
    Word size and conversions between integer and pointer.
 */
#if ME_64
    #define ITOP(i)     ((void*) ((int64) i))
    #define PTOI(i)     ((int) ((int64) i))
    #define LTOP(i)     ((void*) ((int64) i))
    #define PTOL(i)     ((int64) i)
#else
    #define ITOP(i)     ((void*) ((int) i))
    #define PTOI(i)     ((int) i)
    #define LTOP(i)     ((void*) ((int) i))
    #define PTOL(i)     ((int64) (int) i)
#endif

#undef PUBLIC
#undef PUBLIC_DATA
#undef PRIVATE

#if ME_WIN_LIKE
    /*
        Use PUBLIC on function declarations and definitions (*.c and *.h).
     */
    #define PUBLIC      __declspec(dllexport)
    #define PUBLIC_DATA __declspec(dllexport)
    #define PRIVATE     static
#else
    #define PUBLIC
    #define PUBLIC_DATA extern
    #define PRIVATE     static
#endif

/* Undefines for Qt - Ugh */
#undef max
#undef min

/**
    Return the maximum of two values.
    @description Safe macro to return the larger of two values. Arguments are evaluated twice,
    so avoid using expressions with side effects.
    @param a First value to compare
    @param b Second value to compare
    @return The larger of the two values
    @stability Stable
*/
#define max(a,b)  (((a) > (b)) ? (a) : (b))

/**
    Return the minimum of two values.
    @description Safe macro to return the smaller of two values. Arguments are evaluated twice,
    so avoid using expressions with side effects.
    @param a First value to compare
    @param b Second value to compare
    @return The smaller of the two values
    @stability Stable
*/
#define min(a,b)  (((a) < (b)) ? (a) : (b))

/**
    @section Compiler Abstractions

    Compiler-specific macros for function attributes, optimization hints, and cross-platform compatibility.
    These abstractions allow the code to take advantage of compiler-specific features while maintaining
    portability across different toolchains.
*/

#ifndef PRINTF_ATTRIBUTE
    #if ((__GNUC__ >= 3) && !DOXYGEN) || MACOSX
        /**
            Printf-style function format checking attribute.
            @description Enables GCC to check printf-style format strings against their arguments at compile time.
            Helps catch format string bugs and type mismatches early in development.
            @param a1 1-based index of the format string parameter
            @param a2 1-based index of the first format argument parameter
            @stability Stable
         */
        #define PRINTF_ATTRIBUTE(a1, a2) __attribute__ ((format (__printf__, a1, a2)))
    #else
        #define PRINTF_ATTRIBUTE(a1, a2)
    #endif
#endif

#undef likely
#undef unlikely
#if (__GNUC__ >= 3)
    /**
        Branch prediction hint for likely conditions.
        @description Tells the compiler that the condition is likely to be true, enabling better
        branch prediction and code optimization. Use sparingly and only for conditions that are
        overwhelmingly likely to be true.
        @param x Condition expression to evaluate
        @return Same value as x, with optimization hint
        @stability Stable
    */
    #define likely(x)   __builtin_expect(!!(x), 1)

    /**
        Branch prediction hint for unlikely conditions.
        @description Tells the compiler that the condition is likely to be false, enabling better
        branch prediction and code optimization. Commonly used for error handling paths.
        @param x Condition expression to evaluate
        @return Same value as x, with optimization hint
        @stability Stable
    */
    #define unlikely(x) __builtin_expect(!!(x), 0)
#else
    #define likely(x)   (x)
    #define unlikely(x) (x)
#endif

#if !__UCLIBC__ && !CYGWIN && __USE_XOPEN2K
    #define ME_COMPILER_HAS_SPINLOCK    1
#endif

#if ME_COMPILER_HAS_DOUBLE_BRACES
    #define  NULL_INIT    {{0}}
#else
    #define  NULL_INIT    {0}
#endif

#ifdef __USE_FILE_OFFSET64
    #define ME_COMPILER_HAS_OFF64 1
#else
    #define ME_COMPILER_HAS_OFF64 0
#endif

#if ME_UNIX_LIKE
    #define ME_COMPILER_HAS_FCNTL 1
#endif

#ifndef R_OK
    #define R_OK    4
    #define W_OK    2
#if ME_WIN_LIKE
    #define X_OK    R_OK
#else
    #define X_OK    1
#endif
    #define F_OK    0
#endif

#if MACOSX
    #define LD_LIBRARY_PATH "DYLD_LIBRARY_PATH"
#else
    #define LD_LIBRARY_PATH "LD_LIBRARY_PATH"
#endif

#if VXWORKS || WINDOWS
    /*
        Use in arra[ARRAY_FLEX] to avoid compiler warnings
     */
    #define ARRAY_FLEX 0
#else
    #define ARRAY_FLEX
#endif

/*
    Deprecated API warnings
 */
#if ((__GNUC__ >= 3) || MACOSX) && !VXWORKS && ME_DEPRECATED_WARNINGS
    #define ME_DEPRECATED(MSG) __attribute__ ((deprecated(MSG)))
#else
    #define ME_DEPRECATED(MSG)
#endif

#define NOT_USED(x) ((void*) x)

/**
    @section System Configuration Tunables

    Configurable constants that define system limits and buffer sizes. These values are optimized for
    different target platforms, with smaller values for microcontrollers and embedded systems, and
    larger values for desktop and server platforms. Values can be overridden in build configuration
    files using pascal case names (e.g., maxPath: 4096 in settings).
*/
#if ESP32 || FREERTOS || VXWORKS
    //  Microcontrollers and embedded systems with constrained memory
    #ifndef ME_MAX_FNAME
        /**
            Maximum filename length for embedded systems.
            @description Conservative filename size limit for microcontrollers and embedded systems
            where memory is constrained. Sufficient for most embedded application file naming.
            @stability Stable
        */
        #define ME_MAX_FNAME        128
    #endif
    #ifndef ME_MAX_PATH
        /**
            Maximum path length for embedded systems.
            @description Conservative path size limit for microcontrollers and embedded systems.
            Balances functionality with memory constraints typical of embedded applications.
            @stability Stable
        */
        #define ME_MAX_PATH         256
    #endif
    #ifndef ME_BUFSIZE
        /**
            Standard buffer size for embedded systems.
            @description Conservative buffer size for I/O operations, string manipulation, and temporary
            storage in memory-constrained embedded environments.
            @stability Stable
        */
        #define ME_BUFSIZE          1024
    #endif
    #ifndef ME_MAX_BUFFER
        #define ME_MAX_BUFFER       ME_BUFSIZE  /* DEPRECATE */
    #endif
    #ifndef ME_MAX_ARGC
        /**
            Maximum command line arguments for embedded systems.
            @description Conservative limit for command line argument parsing in embedded applications
            where argument lists are typically simple and memory is limited.
            @stability Stable
        */
        #define ME_MAX_ARGC         16
    #endif
    #ifndef ME_DOUBLE_BUFFER
        /**
            Buffer size for double-precision floating point string conversion.
            @description Calculated buffer size needed for converting double values to strings.
            @stability Stable
        */
        #define ME_DOUBLE_BUFFER    (DBL_MANT_DIG - DBL_MIN_EXP + 4)
    #endif
    #ifndef ME_MAX_IP
        /**
            Maximum IP address string length for embedded systems.
            @description Buffer size for IP address string representation in embedded networking.
            @stability Stable
        */
        #define ME_MAX_IP           128
    #endif
#else
    // Desktop, server, and high-resource embedded systems
    #ifndef ME_MAX_FNAME
        /**
            Maximum filename length for desktop/server systems.
            @description Generous filename size limit for desktop and server environments where
            memory is less constrained and longer filenames are common.
            @stability Stable
        */
        #define ME_MAX_FNAME        256
    #endif
    #ifndef ME_MAX_PATH
        /**
            Maximum path length for desktop/server systems.
            @description Standard path size limit for desktop and server systems, accommodating
            deep directory structures and long component names.
            @stability Stable
        */
        #define ME_MAX_PATH         1024
    #endif
    #ifndef ME_BUFSIZE
        /**
            Standard buffer size for desktop/server systems.
            @description Larger buffer size for I/O operations and string manipulation in environments
            with abundant memory. Optimized for performance over memory usage.
            @stability Stable
        */
        #define ME_BUFSIZE          4096
    #endif
    #ifndef ME_MAX_BUFFER
        #define ME_MAX_BUFFER       ME_BUFSIZE  /* DEPRECATE */
    #endif

    #ifndef ME_MAX_ARGC
        /**
            Maximum command line arguments for desktop/server systems.
            @description Higher limit for command line argument parsing in desktop and server
            applications where complex argument lists are common.
            @stability Stable
        */
        #define ME_MAX_ARGC         32
    #endif
    #ifndef ME_DOUBLE_BUFFER
        /**
            Buffer size for double-precision floating point string conversion.
            @description Calculated buffer size needed for converting double values to strings.
            @stability Stable
        */
        #define ME_DOUBLE_BUFFER    (DBL_MANT_DIG - DBL_MIN_EXP + 4)
    #endif
    #ifndef ME_MAX_IP
        /**
            Maximum IP address string length for desktop/server systems.
            @description Extended buffer size for IP address strings, URLs, and network identifiers.
            @stability Stable
        */
        #define ME_MAX_IP           1024
    #endif
#endif


#ifndef ME_STACK_SIZE
#if ME_COMPILER_HAS_MMU && !VXWORKS
    /**
        Default thread stack size for systems with virtual memory.
        @description On systems with MMU and virtual memory support, use system default stack size
        since only actually used pages consume physical memory. Value of 0 means use system default.
        @stability Stable
    */
    #define ME_STACK_SIZE    0
#else
    /**
        Default thread stack size for systems without virtual memory.
        @description On systems without MMU (microcontrollers, embedded), the entire stack size
        consumes physical memory, so this is set conservatively. Increase if using script engines
        or deep recursion. Value in bytes.
        @stability Stable
    */
    #define ME_STACK_SIZE    (32 * 1024)
#endif
#endif

/*********************************** Fixups ***********************************/

#ifndef ME_INLINE
    #if ME_WIN_LIKE
        #define ME_INLINE __inline
    #else
        #define ME_INLINE inline
    #endif
#endif

#if ECOS
    #define     LIBKERN_INLINE          /* to avoid kernel inline functions */
#endif /* ECOS */

#if ME_UNIX_LIKE || VXWORKS || TIDSP
    #define FILE_TEXT        ""
    #define FILE_BINARY      ""
#endif

#if !TIDSP
    #define ME_COMPILER_HAS_MACRO_VARARGS 1
#else
    #define ME_COMPILER_HAS_MACRO_VARARGS 1
#endif

#if ME_UNIX_LIKE
    #define closesocket(x)  close(x)
    #if !defined(PTHREAD_MUTEX_RECURSIVE_NP) || FREEBSD
        #define PTHREAD_MUTEX_RECURSIVE_NP PTHREAD_MUTEX_RECURSIVE
    #else
        #ifndef PTHREAD_MUTEX_RECURSIVE
            #define PTHREAD_MUTEX_RECURSIVE PTHREAD_MUTEX_RECURSIVE_NP
        #endif
    #endif
#endif

#if !ME_WIN_LIKE && !CYGWIN
    #ifndef O_BINARY
        #define O_BINARY    0
    #endif
    #ifndef O_TEXT
        #define O_TEXT      0
    #endif
#endif

#if !LINUX
    #define __WALL          0
    #if !CYGWIN && !defined(MSG_NOSIGNAL)
        #define MSG_NOSIGNAL 0
    #endif
#endif

#if ME_BSD_LIKE
    /*
        Fix for MAC OS X - getenv
     */
    #if !HAVE_DECL_ENVIRON
        #ifdef __APPLE__
            #include <crt_externs.h>
            #define environ (*_NSGetEnviron())
        #else
            extern char **environ;
        #endif
    #endif
#endif

#if SOLARIS
    #define INADDR_NONE     ((in_addr_t) 0xffffffff)
#endif

#ifdef SIGINFO
    #define ME_SIGINFO SIGINFO
#elif defined(SIGPWR)
    #define ME_SIGINFO SIGPWR
#else
    #define ME_SIGINFO (0)
#endif

#if VXWORKS
    #ifndef SHUT_RDWR
        #define SHUT_RDWR 2
    #endif
    #define HAVE_SOCKLEN_T
    #if _DIAB_TOOL
        #define inline __inline__
        #define MPR_INLINE __inline__
    #endif
    #ifndef closesocket
        #define closesocket(x)  close(x)
    #endif
    #ifndef va_copy
        #define va_copy(d, s) ((d) = (s))
    #endif
    #ifndef strcasecmp
        #define strcasecmp scaselesscmp
    #endif
    #ifndef strncasecmp
        #define strncasecmp sncaselesscmp
    #endif
#endif

#if ME_WIN_LIKE
    typedef int     uid_t;
    typedef void    *handle;
    typedef char    *caddr_t;
    typedef long    pid_t;
    typedef int     gid_t;
    typedef ushort  mode_t;
    typedef void    *siginfo_t;
    typedef int     socklen_t;

    #define HAVE_SOCKLEN_T
    #define MSG_NOSIGNAL    0
    #define FILE_BINARY     "b"
    #define FILE_TEXT       "t"
    #define O_CLOEXEC       0

    /*
        Error codes
     */
    #define EPERM           1
    #define ENOENT          2
    #define ESRCH           3
    #define EINTR           4
    #define EIO             5
    #define ENXIO           6
    #define E2BIG           7
    #define ENOEXEC         8
    #define EBADF           9
    #define ECHILD          10
    #define EAGAIN          11
    #define ENOMEM          12
    #define EACCES          13
    #define EFAULT          14
    #define EOSERR          15
    #define EBUSY           16
    #define EEXIST          17
    #define EXDEV           18
    #define ENODEV          19
    #define ENOTDIR         20
    #define EISDIR          21
    #define EINVAL          22
    #define ENFILE          23
    #define EMFILE          24
    #define ENOTTY          25
    #define EFBIG           27
    #define ENOSPC          28
    #define ESPIPE          29
    #define EROFS           30
    #define EMLINK          31
    #define EPIPE           32
    #define EDOM            33
    #define ERANGE          34

    #ifndef EWOULDBLOCK
    #define EWOULDBLOCK     EAGAIN
    #define EINPROGRESS     36
    #define EALREADY        37
    #define ENETDOWN        43
    #define ECONNRESET      44
    #define ECONNREFUSED    45
    #define EADDRNOTAVAIL   49
    #define EISCONN         56
    #define EADDRINUSE      46
    #define ENETUNREACH     51
    #define ECONNABORTED    53
    #endif
    #ifndef ENOTCONN
        #define ENOTCONN    126
    #endif
    #ifndef EPROTO
        #define EPROTO      134
    #endif

    #undef SHUT_RDWR
    #define SHUT_RDWR       2

    #define TIME_GENESIS UINT64(11644473600000000)
    #ifndef va_copy
        #define va_copy(d, s) ((d) = (s))
    #endif

    #if !WINCE
    #ifndef access
    #define access      _access
    #define chdir       _chdir
    #define chmod       _chmod
    #define close       _close
    #define fileno      _fileno
    #define fstat       _fstat
    #define getcwd      _getcwd
    #define getpid      _getpid
    #define gettimezone _gettimezone
    #define lseek       _lseek
    //  SECURITY Acceptable: - the omode parameter is ignored on Windows
    #define mkdir(a,b)  _mkdir(a)
    #define open        _open
    #define putenv      _putenv
    #define read        _read
    #define rmdir(a)    _rmdir(a)
    #define stat        _stat
    #define strdup      _strdup
    #define tempnam     _tempnam
    #define umask       _umask
    #define unlink      _unlink
    #define write       _write
    PUBLIC void         sleep(int secs);
    #endif
    #endif

    #ifndef strcasecmp
    #define strcasecmp scaselesscmp
    #define strncasecmp sncaselesscmp
    #endif
    #ifndef strncasecmp
        #define strncasecmp sncaselesscmp
    #endif

    /*
        Define S_ISREG and S_ISDIR macros for Windows if not already defined
     */
    #ifndef S_ISDIR
        #define S_ISDIR(m) (((m) & _S_IFMT) == _S_IFDIR)
    #endif
    #ifndef S_ISREG
        #define S_ISREG(m) (((m) & _S_IFMT) == _S_IFREG)
    #endif

    /*
        Define strtok_r for Windows if not already defined
    */
    #ifndef strtok_r
        #define strtok_r strtok_s
    #endif

    #pragma comment( lib, "ws2_32.lib" )
#endif /* WIN_LIKE */

#if WINCE
    typedef void FILE;
    typedef int off_t;

    #ifndef EOF
        #define EOF        -1
    #endif
    #define O_RDONLY        0
    #define O_WRONLY        1
    #define O_RDWR          2
    #define O_NDELAY        0x4
    #define O_NONBLOCK      0x4
    #define O_APPEND        0x8
    #define O_CREAT         0x100
    #define O_TRUNC         0x200
    #define O_TEXT          0x400
    #define O_EXCL          0x800
    #define O_BINARY        0x1000
    /*
        stat flags
     */
    #define S_IFMT          0170000
    #define S_IFDIR         0040000
    #define S_IFCHR         0020000         /* character special */
    #define S_IFIFO         0010000
    #define S_IFREG         0100000
    #define S_IREAD         0000400
    #define S_IWRITE        0000200
    #define S_IEXEC         0000100

    #ifndef S_ISDIR
        #define S_ISDIR(X) (((X) & S_IFMT) == S_IFDIR)
    #endif
    #ifndef S_ISREG
        #define S_ISREG(X) (((X) & S_IFMT) == S_IFREG)
    #endif

    /*
        Windows uses strtok_s instead of strtok_r
     */
    #ifndef strtok_r
        #define strtok_r strtok_s
    #endif

    #define STARTF_USESHOWWINDOW 0
    #define STARTF_USESTDHANDLES 0

    #define BUFSIZ   ME_BUFSIZE
    #define PATHSIZE ME_MAX_PATH
    #define gethostbyname2(a,b) gethostbyname(a)
    #pragma comment( lib, "ws2.lib" )
#endif /* WINCE */

#if TIDSP
    #define EINTR   4
    #define EAGAIN  11
    #define INADDR_NONE 0xFFFFFFFF
    #define PATHSIZE ME_MAX_PATH
    #define NBBY 8
    #define hostent _hostent
    #define NFDBITS ((int) (sizeof(fd_mask) * NBBY))
    typedef long fd_mask;
    typedef int Socklen;
    struct sockaddr_storage { char pad[1024]; };
#endif /* TIDSP */

#ifndef NBBY
    #define NBBY 8
#endif

#if FREERTOS
#if !ESP32
    typedef unsigned int socklen_t;
#endif
#ifndef SOMAXCONN
    #define SOMAXCONN 5
#endif
#endif

/*********************************** Externs **********************************/

#ifdef __cplusplus
extern "C" {
#endif

#if LINUX
    extern int pthread_mutexattr_gettype (__const pthread_mutexattr_t *__restrict __attr, int *__restrict __kind);
    extern int pthread_mutexattr_settype (pthread_mutexattr_t *__attr, int __kind);
    extern char **environ;
#endif

#if VXWORKS
    #if _WRS_VXWORKS_MAJOR < 6 || (_WRS_VXWORKS_MAJOR == 6 && _WRS_VXWORKS_MINOR < 9)
        PUBLIC int gettimeofday(struct timeval *tv, struct timezone *tz);
    #endif
    PUBLIC char *strdup(const char *);
    PUBLIC int sysClkRateGet(void);

    #if _WRS_VXWORKS_MAJOR < 6
        #define NI_MAXHOST      128
        extern STATUS access(cchar *path, int mode);
        typedef int     socklen_t;
        struct sockaddr_storage {
            char        pad[1024];
        };
    #else
        /*
            This may or may not be necessary - let us know dev@embedthis.com if your system needs this (and why).
         */
        #if _DIAB_TOOL
            #if ME_CPU_ARCH == ME_CPU_PPC
                #define __va_copy(dest, src) memcpy((dest), (src), sizeof(va_list))
            #endif
        #endif
        #define HAVE_SOCKLEN_T
    #endif
#endif  /* VXWORKS */

#if ME_WIN_LIKE
    struct timezone {
      int  tz_minuteswest;      /* minutes W of Greenwich */
      int  tz_dsttime;          /* type of dst correction */
    };
    PUBLIC int  getuid(void);
    PUBLIC int  geteuid(void);
    PUBLIC int  gettimeofday(struct timeval *tv, struct timezone *tz);
    PUBLIC long lrand48(void);
    PUBLIC long nap(long);
    PUBLIC void srand48(long);
    PUBLIC long ulimit(int, ...);
#endif

#if WINCE
    struct stat {
        int     st_dev;
        int     st_ino;
        ushort  st_mode;
        short   st_nlink;
        short   st_uid;
        short   st_gid;
        int     st_rdev;
        long    st_size;
        time_t  st_atime;
        time_t  st_mtime;
        time_t  st_ctime;
    };
    extern int access(cchar *filename, int flags);
    extern int chdir(cchar   dirname);
    extern int chmod(cchar *path, int mode);
    extern int close(int handle);
    extern void exit(int status);
    extern long _get_osfhandle(int handle);
    extern char *getcwd(char* buffer, int maxlen);
    extern char *getenv(cchar *charstuff);
    extern pid_t getpid(void);
    extern long lseek(int handle, long offset, int origin);
    extern int mkdir(cchar *dir, int mode);
    extern time_t mktime(struct tm *pt);
    extern int _open_osfhandle(int *handle, int flags);
    extern uint open(cchar *file, int mode,...);
    extern int read(int handle, void *buffer, uint count);
    extern int rename(cchar *from, cchar *to);
    extern int rmdir(cchar   dir);
    extern uint sleep(uint secs);
    extern int stat(cchar *path, struct stat *stat);
    extern char *strdup(char *s);
    extern int write(int handle, cvoid *buffer, uint count);
    extern int umask(int mode);
    extern int unlink(cchar *path);
    extern int errno;

    #undef CreateFile
    #define CreateFile CreateFileA
    WINBASEAPI HANDLE WINAPI CreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
        LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes,
        HANDLE hTemplateFile);

    #undef CreateProcess
    #define CreateProcess CreateProcessA

    #undef FindFirstFile
    #define FindFirstFile FindFirstFileA
    WINBASEAPI HANDLE WINAPI FindFirstFileA(LPCSTR lpFileName, LPWIN32_FIND_DATAA lpFindFileData);

    #undef FindNextFile
    #define FindNextFile FindNextFileA
    WINBASEAPI BOOL WINAPI FindNextFileA(HANDLE hFindFile, LPWIN32_FIND_DATAA lpFindFileData);

    #undef GetModuleFileName
    #define GetModuleFileName GetModuleFileNameA
    WINBASEAPI DWORD WINAPI GetModuleFileNameA(HMODULE hModule, LPSTR lpFilename, DWORD nSize);

    #undef GetModuleHandle
    #define GetModuleHandle GetModuleHandleA
    WINBASEAPI HMODULE WINAPI GetModuleHandleA(LPCSTR lpModuleName);

    #undef GetProcAddress
    #define GetProcAddress GetProcAddressA

    #undef GetFileAttributes
    #define GetFileAttributes GetFileAttributesA
    extern DWORD GetFileAttributesA(cchar *path);

    extern void GetSystemTimeAsFileTime(FILETIME *ft);

    #undef LoadLibrary
    #define LoadLibrary LoadLibraryA
    HINSTANCE WINAPI LoadLibraryA(LPCSTR lpLibFileName);

    #define WSAGetLastError GetLastError

    #define _get_timezone getTimezone
    extern int getTimezone(int *secs);

    extern struct tm *localtime_r(const time_t *when, struct tm *tp);
    extern struct tm *gmtime_r(const time_t *t, struct tm *tp);
#endif /* WINCE */

/*
    Help for generated documentation
 */
#if DOXYGEN
    /** Argument for sockets */
    typedef int Socket;
    /** Unsigned integral type. Equivalent in size to void* */
    typedef long size_t;
    /** Unsigned time type. Time in seconds since Jan 1, 1970 */
    typedef long time_t;
#endif

#ifdef __cplusplus
}
#endif

#endif /* _h_OSDEP */

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
