@echo off
REM
REM   Auto-detect OpenSSL and set INCLUDE/LIB environment variables.
REM   Also copies OpenSSL DLLs to build\bin for runtime.
REM
REM   Search priority:
REM     1. ME_COM_OPENSSL_PATH (explicit user override)
REM     2. vcpkg install (%USERPROFILE%\vcpkg\installed\x64-windows)
REM     3. System install (C:\Program Files\OpenSSL or OpenSSL-Win64)
REM
REM   NOTE: No setlocal - all variables are exported to the caller.
REM

set ARCH=%PROCESSOR_ARCHITECTURE%
if "%ARCH%"=="" set ARCH=AMD64

REM Map PROCESSOR_ARCHITECTURE to OpenSSL/vcpkg arch names
set SSL_ARCH=x64
if "%ARCH%"=="ARM64" set SSL_ARCH=arm64
if "%ARCH%"=="X86" set SSL_ARCH=x86

REM Search for OpenSSL (single-line ifs to avoid block expansion issues)
set "SSL_DIR="

if defined ME_COM_OPENSSL_PATH if exist "%ME_COM_OPENSSL_PATH%\include\openssl\ssl.h" set "SSL_DIR=%ME_COM_OPENSSL_PATH%"
if defined SSL_DIR goto :found

if exist "%USERPROFILE%\vcpkg\installed\%SSL_ARCH%-windows\include\openssl\ssl.h" set "SSL_DIR=%USERPROFILE%\vcpkg\installed\%SSL_ARCH%-windows"
if defined SSL_DIR goto :found

if exist "C:\Program Files\OpenSSL\include\openssl\ssl.h" set "SSL_DIR=C:\Program Files\OpenSSL"
if defined SSL_DIR goto :found

if exist "C:\Program Files\OpenSSL-Win64\include\openssl\ssl.h" set "SSL_DIR=C:\Program Files\OpenSSL-Win64"
if defined SSL_DIR goto :found

echo WARNING: OpenSSL not found. Build may fail.
echo Searched:
if defined ME_COM_OPENSSL_PATH echo   - %ME_COM_OPENSSL_PATH% (ME_COM_OPENSSL_PATH)
echo   - %USERPROFILE%\vcpkg\installed\%SSL_ARCH%-windows
echo   - C:\Program Files\OpenSSL
echo   - C:\Program Files\OpenSSL-Win64
goto :eof

:found

echo Found OpenSSL at: %SSL_DIR%

REM Set INCLUDE for the compiler
set "INCLUDE=%SSL_DIR%\include;%INCLUDE%"

REM Add all possible lib subdirectories to LIB
REM The Shining Light OpenSSL installer uses lib\VC\<arch>\<crt> layout
REM vcpkg uses lib\ directly
set "LIB=%SSL_DIR%\lib;%LIB%"
if exist "%SSL_DIR%\lib\VC\%SSL_ARCH%\MTd" set "LIB=%SSL_DIR%\lib\VC\%SSL_ARCH%\MTd;%LIB%"
if exist "%SSL_DIR%\lib\VC\%SSL_ARCH%\MT"  set "LIB=%SSL_DIR%\lib\VC\%SSL_ARCH%\MT;%LIB%"
if exist "%SSL_DIR%\lib\VC\%SSL_ARCH%\MDd" set "LIB=%SSL_DIR%\lib\VC\%SSL_ARCH%\MDd;%LIB%"
if exist "%SSL_DIR%\lib\VC\%SSL_ARCH%\MD"  set "LIB=%SSL_DIR%\lib\VC\%SSL_ARCH%\MD;%LIB%"

REM Set ME_COM_OPENSSL_PATH and ARCH for downstream scripts (e.g. TestMe)
set "ME_COM_OPENSSL_PATH=%SSL_DIR%"
set "ARCH=%SSL_ARCH%"

REM Copy DLLs to build\bin for runtime
if not exist "build\bin" mkdir "build\bin"

REM Copy from vcpkg bin directory
if exist "%USERPROFILE%\vcpkg\installed\%SSL_ARCH%-windows\bin\libcrypto-3-%SSL_ARCH%.dll" (
    echo Copying OpenSSL DLLs from vcpkg
    powershell -Command "Copy-Item '%USERPROFILE%\vcpkg\installed\%SSL_ARCH%-windows\bin\libcrypto-*.dll' 'build\bin\' -Force -ErrorAction SilentlyContinue"
    powershell -Command "Copy-Item '%USERPROFILE%\vcpkg\installed\%SSL_ARCH%-windows\bin\libssl-*.dll' 'build\bin\' -Force -ErrorAction SilentlyContinue"
)

REM Copy from SSL_DIR bin directory
if exist "%SSL_DIR%\bin\libcrypto-3-%SSL_ARCH%.dll" (
    echo Copying OpenSSL DLLs from %SSL_DIR%
    powershell -Command "Copy-Item '%SSL_DIR%\bin\libcrypto-*.dll' 'build\bin\' -Force -ErrorAction SilentlyContinue"
    powershell -Command "Copy-Item '%SSL_DIR%\bin\libssl-*.dll' 'build\bin\' -Force -ErrorAction SilentlyContinue"
)

echo OpenSSL configured: %SSL_DIR%
