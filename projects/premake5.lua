--
--  projects/premake5.lua -- GoAhead Web Server Build
--
--  Builds libgoahead (shared library), goahead, goahead-test, gopass, cgitest.
--
--  Usage (run from the projects/ directory):
--      premake5 gmake2                    # Generate GNU Makefiles for all platforms
--      premake5 vs2022                    # Generate VS2022 projects for Windows
--      premake5 xcode4                    # Generate Xcode projects for macOS
--      make config=debug_macosx           # Build for macOS
--      make config=release_linux          # Build for Linux
--

local ROOT = ".."

-- Read version from package.json (single source of truth)
local pakContent = io.readfile(ROOT .. "/package.json")
local VERSION = pakContent and pakContent:match('"version"%s*:%s*"([^"]+)"') or "0.0.0"
local isVS = (_ACTION == "vs2022")
local isXcode = (_ACTION == "xcode4")

---------------------------------------------------------------------
--  Options
---------------------------------------------------------------------

newoption {
    trigger     = "tls",
    value       = "PROVIDER",
    description = "TLS provider: openssl (default) or mbedtls",
    default     = "openssl",
    allowed     = {
        { "openssl", "OpenSSL (default)" },
        { "mbedtls", "MbedTLS" },
    }
}

newoption {
    trigger     = "openssl-path",
    value       = "PATH",
    description = "Path to OpenSSL for every platform, overriding the per-platform defaults " ..
                  "(/opt/homebrew on macOS, /usr on Linux/FreeBSD, C:/Program Files/OpenSSL on Windows)",
}

newoption {
    trigger     = "mbedtls-path",
    value       = "PATH",
    description = "Path to MbedTLS installation",
}

--
--  OpenSSL sits in a different place on each target, and these project files are generated once,
--  committed, and shipped in the source distribution. Resolving a single path from os.host() at
--  generation time would bake the generating machine's layout into every configuration, so a Linux
--  build from the distribution would be handed the macOS Homebrew prefix and the /usr default for
--  Linux would never reach a user. Resolve per target platform inside a filter instead, so each
--  configuration carries its own path. An explicit --openssl-path overrides all of them, for an
--  installation that is not in the usual place.
--
local opensslPaths = {
    macosx  = "/opt/homebrew",
    linux   = "/usr",
    freebsd = "/usr",
    windows = "C:/Program Files/OpenSSL",
}

--
--  Iterate an ordered list, never pairs() over the table above: the generated files are committed
--  and diffed, so the emission order has to be stable from run to run.
--
local platformList
if isVS then
    platformList = { "windows" }
elseif isXcode then
    platformList = { "macosx" }
else
    platformList = { "macosx", "linux", "freebsd" }
end

local function opensslPathFor(platform)
    return _OPTIONS["openssl-path"] or opensslPaths[platform]
end

local mbedtlsPath = _OPTIONS["mbedtls-path"] or "/usr"
local tlsProvider = _OPTIONS["tls"] or "openssl"

---------------------------------------------------------------------
--  Workspace
---------------------------------------------------------------------

workspace "goahead"
-- Add sanitizer configuration to make projects; IDEs provide their own controls.
    if isVS or isXcode then
        configurations { "debug", "release" }
    else
        configurations { "debug", "release", "asan" }
    end
    language       "C"
    warnings       "Extra"
    objdir         (ROOT .. "/build/obj/%{prj.name}-%{cfg.platform}-%{cfg.buildcfg}")
    targetdir      (ROOT .. "/build/bin")

    if isVS then
        platforms { "windows" }
        location  "vs2022"
        cdialect  "C11"
        architecture "x86_64"
        characterset "MBCS"
-- Use one dynamic C runtime so all components share heap, errno, and descriptors.
        staticruntime "Off"
    elseif isXcode then
        platforms { "macosx" }
        location  "xcode"
        cdialect  "gnu11"
    else
        platforms { "macosx", "linux", "freebsd" }
        location  "gmake2"
        cdialect  "gnu11"
    end

    -- Common include paths
    includedirs { ROOT .. "/src", ROOT .. "/src/osdep" }

    -- Dynamic defines
    defines {
        'ME_VERSION="' .. VERSION .. '"',
    }

    -- TLS provider defines
    if tlsProvider == "openssl" then
        defines {
            "ME_COM_OPENSSL=1",
            "ME_COM_MBEDTLS=0",
            "ME_COM_SSL=1",
            'ME_COM_MBEDTLS_PATH="' .. mbedtlsPath .. '"',
        }
    else
        defines {
            "ME_COM_OPENSSL=0",
            "ME_COM_MBEDTLS=1",
            "ME_COM_SSL=1",
            'ME_COM_MBEDTLS_PATH="' .. mbedtlsPath .. '"',
        }
        includedirs { mbedtlsPath .. "/include" }
    end

    --
    --  The OpenSSL prefix is per platform even when MbedTLS is the provider, because
    --  ME_COM_OPENSSL_PATH is emitted either way.
    --
    for _, platform in ipairs(platformList) do
        filter("platforms:" .. platform)
            defines { 'ME_COM_OPENSSL_PATH="' .. opensslPathFor(platform) .. '"' }
            if tlsProvider == "openssl" then
                includedirs { opensslPathFor(platform) .. "/include" }
            end
    end
    filter {}

    -----------------------------------------------------------------
    --  Debug / Release
    -----------------------------------------------------------------
    filter "configurations:debug"
        symbols  "On"
        optimize "Off"
        defines  { "ME_DEBUG=1" }

    filter "configurations:release"
        symbols  "Off"
        optimize "On"

    filter {}

    --
    --  Release stripping comes from symbols "Off" above, which premake already renders as "-s" on
    --  the toolchains where it means anything. Adding it here as well emitted "-s -s" on Linux and
    --  FreeBSD, and on macOS emitted an "-s" that the linker has retired -- every release link
    --  printed "ld: warning: -s is obsolete" and the binary was not stripped regardless.
    --
    if not isVS then
        filter "configurations:debug"
            linkoptions { "-g" }

-- Abort on sanitizer reports to fail the test. SANITIZE_FLAGS from the Makefile adds the groups the compiler supports.
        filter "configurations:asan"
            symbols  "On"
            optimize "Off"
            defines  { "ME_DEBUG=1" }
            buildoptions {
                "-fsanitize=address,undefined",
                "$(SANITIZE_FLAGS)",
                "-fno-omit-frame-pointer",
                "-fno-sanitize-recover=all",
            }
            linkoptions { "-g", "-fsanitize=address,undefined", "$(SANITIZE_FLAGS)", "$(SANITIZE_LDFLAGS)" }

        filter {}
    end

    -----------------------------------------------------------------
    --  Platform-specific flags
    -----------------------------------------------------------------
    if isVS then
        filter "platforms:windows"
            system  "windows"
            toolset "msc"
            -- Debug information as /Zi: the edit-and-continue form, /ZI, cannot be combined with /guard:cf
            editandcontinue "Off"
-- Enable format checks for annotated variadic functions.
            -- Treat undeclared functions and missing returns as errors.
            -- Treat incompatible pointers, narrowing and shadowing as errors, as the POSIX build does.
            buildoptions {
                "/we4774",
                "/we4013",
                "/we4716",
                "/we4133",
                "/we4244",
                "/we4456",
                "/we4459",
                "/guard:cf",
            }
-- Enable CFG, ASLR, DEP, and CET. Avoid /sdl's errors on intentional C library use.
            linkoptions {
                "/guard:cf",
                "/DYNAMICBASE",
                "/HIGHENTROPYVA",
                "/NXCOMPAT",
                "/CETCOMPAT",
            }
-- Disable only warnings for intentional patterns or checks covered elsewhere.
            disablewarnings {
                "4100",     -- unreferenced parameter: callbacks take fixed signatures
                "4127",     -- constant condition: ME_* configuration tests and while (1) loops
                "4152",     -- function/data pointer conversion: callbacks are stored as symbols
                "4389",     -- signed/unsigned comparison: checked by -Wsign-compare on POSIX
            }
        filter {}
    else
-- Share POSIX compile and ELF link flags. Fixed callback signatures leave some parameters unused.
        local posixBuildOptions = {
            "-Wno-unused-parameter",
            "-Wall",
            "-Wshadow",
            "-fstack-protector-strong",
            "-Wformat=2", "-Wformat-security", "-Werror=format-security",
            "-Wsign-compare",
-- Treat these diagnostics as errors in every configuration.
            "-Werror=format",
            "-Werror=implicit-function-declaration",
            "-Werror=return-type",
            "-Werror=int-conversion",
            "-Werror=null-dereference",
        }
-- Use target-supported CFI_FLAGS from Makefile. Xcode manages its own architecture settings.
        if not isXcode then
            table.insert(posixBuildOptions, "$(CFI_FLAGS)")
        end
-- Mark the stack non-executable while retaining warnings for objects requesting an executable stack.
        local elfLinkOptions = {
            "-Wl,-z,relro,-z,now",
            "-Wl,-z,noexecstack",
        }

        filter "platforms:macosx"
            system  "macosx"
            toolset "clang"
-- Keep unknown-warning diagnostics so misspelled flags fail visibly.
            buildoptions(posixBuildOptions)
-- Clang-only conversion check.
            buildoptions { "-Wshorten-64-to-32", "-Wimplicit-int-conversion" }
            linkoptions {
                "-Wl,-no_warn_duplicate_libraries",
                "-Wl,-rpath,@executable_path/",
                "-Wl,-rpath,@loader_path/",
            }
            links { "dl", "pthread", "m", "pam" }

-- Apple's linker lacks -s; strip local symbols after linking while retaining exports.
        filter { "platforms:macosx", "configurations:release" }
            postbuildcommands { "strip -x \"%{cfg.buildtarget.abspath}\"" }

-- FORTIFY requires optimization. Use level 3 where supported, otherwise level 2.
        filter { "platforms:macosx or linux or freebsd", "configurations:release" }
            defines { "_FORTIFY_SOURCE=3" }

        filter "platforms:linux"
            system  "linux"
            toolset "gcc"
            buildoptions(posixBuildOptions)
            linkoptions(elfLinkOptions)
            linkoptions {
                "-Wl,--as-needed",
                "-Wl,--no-copy-dt-needed-entries",
            }
            links { "rt", "dl", "pthread", "m" }

        filter "platforms:freebsd"
            system  "bsd"
            toolset "gcc"
            buildoptions(posixBuildOptions)
            linkoptions(elfLinkOptions)
            links { "dl", "pthread", "m" }

        --
        -- PIE applies only to executables. Applying -fPIE to shared-library
        -- objects produces R_X86_64_PC32 relocations against extern data
        -- (e.g. stderr) that cannot be resolved when linking the .so.
        --
        filter { "platforms:linux or freebsd", "kind:ConsoleApp" }
            buildoptions { "-fPIE" }
            linkoptions  { "-pie" }

        filter {}
    end


---------------------------------------------------------------------
--  libgoahead (shared library)
---------------------------------------------------------------------

project "goahead-lib"
    kind       "SharedLib"
    if isVS then
        targetname "libgoahead"
    else
        targetname "goahead"
    end

    files {
        ROOT .. "/src/action.c",
        ROOT .. "/src/alloc.c",
        ROOT .. "/src/auth.c",
        ROOT .. "/src/cgi.c",
        ROOT .. "/src/crypt.c",
        ROOT .. "/src/file.c",
        ROOT .. "/src/fs.c",
        ROOT .. "/src/http.c",
        ROOT .. "/src/js.c",
        ROOT .. "/src/jst.c",
        ROOT .. "/src/mbedtls.c",
        ROOT .. "/src/openssl.c",
        ROOT .. "/src/options.c",
        ROOT .. "/src/osdep.c",
        ROOT .. "/src/rom.c",
        ROOT .. "/src/route.c",
        ROOT .. "/src/runtime.c",
        ROOT .. "/src/socket.c",
        ROOT .. "/src/time.c",
        ROOT .. "/src/upload.c",
    }

    --
    --  TLS libraries for the shared library. Per platform for the same reason as the include
    --  paths above: the library prefix must follow the target, not the generating host.
    --
    for _, platform in ipairs(platformList) do
        if platform == "windows" then
            filter "platforms:windows"
                links { "ws2_32", "advapi32", "user32", "kernel32", "oldnames", "shell32" }
                if tlsProvider == "openssl" then
                    local p = opensslPathFor("windows")
                    libdirs { p .. "/lib" }
                    links { "libssl", "libcrypto" }
                    --  The OpenSSL build for the same C runtime: dynamic, debug or release
                    filter { "platforms:windows", "configurations:debug" }
                        libdirs { p .. "/lib/VC/x64/MDd" }
                    filter { "platforms:windows", "configurations:release" }
                        libdirs { p .. "/lib/VC/x64/MD" }
                else
                    libdirs { mbedtlsPath .. "/lib", mbedtlsPath .. "/library" }
                    links   { "mbedtls", "mbedcrypto", "mbedx509" }
                end
        else
            filter("platforms:" .. platform)
                if tlsProvider == "openssl" then
                    libdirs { opensslPathFor(platform) .. "/lib" }
                    links   { "ssl", "crypto" }
                else
                    libdirs { mbedtlsPath .. "/lib", mbedtlsPath .. "/library" }
                    links   { "mbedtls", "mbedcrypto", "mbedx509" }
                end
        end
    end
    filter {}


---------------------------------------------------------------------
--  goahead (server executable)
---------------------------------------------------------------------

project "goahead"
    kind       "ConsoleApp"
    links      { "goahead-lib" }
    dependson  { "goahead-lib" }
    libdirs    { ROOT .. "/build/bin" }

    files { ROOT .. "/src/goahead.c" }

    filter "platforms:windows"
        links   { "ws2_32", "advapi32", "user32", "kernel32", "oldnames", "shell32" }
    filter {}


---------------------------------------------------------------------
--  goahead-test (test executable)
---------------------------------------------------------------------

project "goahead-test"
    kind       "ConsoleApp"
    links      { "goahead-lib" }
    dependson  { "goahead-lib" }
    libdirs    { ROOT .. "/build/bin" }

    files { ROOT .. "/src/test.c" }

    filter "platforms:windows"
        links   { "ws2_32", "advapi32", "user32", "kernel32", "oldnames", "shell32" }
    filter {}


---------------------------------------------------------------------
--  gopass (password utility)
---------------------------------------------------------------------

project "gopass"
    kind       "ConsoleApp"
    links      { "goahead-lib" }
    dependson  { "goahead-lib" }
    libdirs    { ROOT .. "/build/bin" }

    files { ROOT .. "/src/utils/gopass.c" }

    filter "platforms:windows"
        links   { "ws2_32", "advapi32", "user32", "kernel32", "oldnames", "shell32" }
    filter {}


---------------------------------------------------------------------
--  webcomp (compile web pages into C source for ROM builds)
---------------------------------------------------------------------

project "webcomp"
    kind       "ConsoleApp"
    links      { "goahead-lib" }
    dependson  { "goahead-lib" }
    libdirs    { ROOT .. "/build/bin" }

    files { ROOT .. "/src/utils/webcomp.c" }

    filter "platforms:windows"
        links   { "ws2_32", "advapi32", "user32", "kernel32", "oldnames", "shell32" }
    filter {}


---------------------------------------------------------------------
--  cgitest (CGI test utility)
---------------------------------------------------------------------

project "cgitest"
    kind       "ConsoleApp"
    links      { "goahead-lib" }
    dependson  { "goahead-lib" }
    libdirs    { ROOT .. "/build/bin" }

    files { ROOT .. "/src/cgitest.c" }

    filter "platforms:windows"
        links   { "ws2_32", "advapi32", "user32", "kernel32", "oldnames", "shell32" }
    filter {}
