#!/usr/bin/env bash
#
#   Test prerequisites for running tests on Unix/Linux/macOS
#

HAS_WARNING=0

# Check for bun in PATH
if ! command -v bun >/dev/null 2>&1; then
    echo "WARNING: bun not found in PATH."
    echo "Please install Bun from: https://bun.sh"
    HAS_WARNING=1
fi

# Check for tm (TestMe) in PATH
if ! command -v tm >/dev/null 2>&1; then
    echo "WARNING: tm (TestMe) not found in PATH."
    echo "TestMe is required for running tests."
    echo "Install with: bun install @embedthis/testme -g"
    HAS_WARNING=1
fi

# Check for bash (should always exist on Unix systems, but verify)
if ! command -v bash >/dev/null 2>&1; then
    echo "WARNING: bash not found in PATH."
    echo "Please install bash or ensure it's in your PATH."
    HAS_WARNING=1
fi

# Check for make
if ! command -v make >/dev/null 2>&1; then
    echo "WARNING: make not found in PATH."
    echo "Build tools may be required for some tests."
    HAS_WARNING=1
fi

# Check for gcc/clang (C compiler)
if ! command -v gcc >/dev/null 2>&1 && ! command -v clang >/dev/null 2>&1; then
    echo "WARNING: No C compiler (gcc/clang) found in PATH."
    echo "A C compiler is required for compiling tests."
    case "$(uname -s)" in
        Linux*)
            echo "Install with: sudo apt-get install build-essential gcc"
            ;;
        Darwin*)
            echo "Install Xcode Command Line Tools: xcode-select --install"
            ;;
        *)
            echo "Please install a C compiler for your system."
            ;;
    esac
    HAS_WARNING=1
fi

# Check for OpenSSL development files
if command -v pkg-config >/dev/null 2>&1; then
    if ! pkg-config --exists openssl 2>/dev/null && ! pkg-config --exists libssl 2>/dev/null; then
        echo "WARNING: OpenSSL development files not found."
        echo "OpenSSL is required for HTTPS support."
        case "$(uname -s)" in
            Linux*)
                echo "Install with: sudo apt-get install libssl-dev"
                ;;
            Darwin*)
                echo "Install with: brew install openssl"
                ;;
            *)
                echo "Please install OpenSSL development files for your system."
                ;;
        esac
        HAS_WARNING=1
    fi
else
    # pkg-config not available, try alternative checks
    if [ ! -f /usr/include/openssl/ssl.h ] && [ ! -f /opt/homebrew/include/openssl/ssl.h ] && [ ! -f /usr/local/include/openssl/ssl.h ]; then
        echo "WARNING: OpenSSL development files may not be installed."
        echo "OpenSSL is required for HTTPS support."
        HAS_WARNING=1
    fi
fi

if [ $HAS_WARNING -eq 1 ]; then
    echo ""
    echo "Some prerequisites are missing. Please install them before running tests."
    exit 1
fi

# Link ejscript package for tests
echo "Linking ejscript package for tests..."
if [ -d "paks/ejs" ]; then
    (cd paks/ejs && bun link --silent) || {
        echo "WARNING: Failed to link ejscript from paks/ejs"
        exit 1
    }
else
    echo "WARNING: paks/ejs directory not found"
    exit 1
fi

if [ -d "test" ]; then
    (cd test && bun link testme --silent) || {
        echo "WARNING: Failed to add testme in test directory"
        exit 1
    }
    (cd test && bun link ejscript --silent) || {
        echo "WARNING: Failed to link ejscript in test directory"
        exit 1
    }
else
    echo "WARNING: test directory not found"
    exit 1
fi

echo "Successfully linked ejscript for tests"
exit 0
