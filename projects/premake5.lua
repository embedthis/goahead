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
    configurations { "debug", "release" }
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
        staticruntime "On"
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
        filter {}
    end

    -----------------------------------------------------------------
    --  Platform-specific flags
    -----------------------------------------------------------------
    if isVS then
        filter "platforms:windows"
            system  "windows"
            toolset "msc"
            --
            -- C4774: "format string expected in argument N is not a string literal". This is the
            -- MSVC analogue of -Werror=format-security and the only format diagnostic that reaches
            -- a user-defined variadic; it fires on the parameters marked PRINTF_FORMAT in osdep.h.
            -- Without it, code inside #if ME_WIN_LIKE blocks is format-checked by no CI leg at all.
            --
            buildoptions {
                "/we4774",
            }
            disablewarnings {
                "4100",     -- unreferenced formal parameter
                "4127",     -- conditional expression is constant
                "4133",     -- incompatible types (char*/LPCWSTR)
                "4152",     -- function/data pointer conversion
                "4244",     -- conversion, possible loss of data
                "4389",     -- signed/unsigned mismatch
                "4456",     -- declaration hides previous local declaration
                "4459",     -- declaration hides global declaration
            }
        filter {}
    else
        filter "platforms:macosx"
            system  "macosx"
            toolset "clang"
            buildoptions {
                "-Wno-sign-conversion",
                "-Wno-unused-parameter",
                "-Wno-unused-result",
                "-Wshorten-64-to-32",
                "-Wall",
                "-Wno-unknown-warning-option",
                "-fstack-protector",
                "--param=ssp-buffer-size=4",
                "-Wformat=2", "-Wformat-security", "-Werror=format-security",
                "-Wsign-compare",
            }
            linkoptions {
                "-Wl,-no_warn_duplicate_libraries",
                "-Wl,-rpath,@executable_path/",
                "-Wl,-rpath,@loader_path/",
            }
            links { "dl", "pthread", "m", "pam" }

        filter "platforms:linux"
            system  "linux"
            toolset "gcc"
            buildoptions {
                "-Wno-unused-parameter",
                "-Wno-unused-result",
                "-Wall",
                "-fstack-protector",
                "--param=ssp-buffer-size=4",
                "-Wformat=2", "-Wformat-security", "-Werror=format-security",
                "-Wsign-compare",
            }
            --
            --  "-z noexecheap" is not a GNU ld option. GNU ld accepts and ignores it with
            --  "warning: -z noexecheap ignored" on every link, so it bought no hardening and only
            --  noise. A non-executable heap comes from the kernel's NX handling on every Linux
            --  target GoAhead supports; there is no linker flag to request it here.
            --
            linkoptions {
                "-Wl,-z,relro,-z,now",
                "-Wl,--as-needed",
                "-Wl,--no-copy-dt-needed-entries",
                "-Wl,--no-warn-execstack",
            }
            links { "rt", "dl", "pthread", "m" }

        --
        -- PIE applies only to executables. Applying -fPIE to shared-library
        -- objects produces R_X86_64_PC32 relocations against extern data
        -- (e.g. stderr) that cannot be resolved when linking the .so.
        --
        filter { "platforms:linux", "kind:ConsoleApp" }
            buildoptions { "-fPIE" }
            linkoptions  { "-pie" }

        filter "platforms:freebsd"
            system  "bsd"
            toolset "gcc"
            buildoptions {
                "-Wno-unused-parameter",
                "-Wno-unused-result",
                "-Wall",
                "-fstack-protector",
                "--param=ssp-buffer-size=4",
                "-Wformat=2", "-Wformat-security", "-Werror=format-security",
                "-Wsign-compare",
            }
            links { "dl", "pthread", "m" }

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
                    libdirs {
                        p .. "/lib",
                        p .. "/lib/VC/x64/MTd",
                        p .. "/lib/VC/x64/MT",
                        p .. "/lib/VC/x64/MDd",
                        p .. "/lib/VC/x64/MD",
                    }
                    links { "libssl", "libcrypto" }
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
