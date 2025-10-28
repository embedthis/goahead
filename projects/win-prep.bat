@echo off
REM
REM   Test prerequisites for running tests on Windows
REM

REM Check for bun.exe in PATH
where bun.exe >nul 2>&1
if %errorlevel% == 0 (
    goto :check_tm
)

echo WARNING: bun.exe not found in PATH.
echo Please install Bun from: https://bun.sh
set HAS_WARNING=1

:check_tm
REM Check for tm (TestMe) in PATH
where tm.exe >nul 2>&1
if %errorlevel% == 0 (
    goto :check_bash
)

where tm >nul 2>&1
if %errorlevel% == 0 (
    goto :check_bash
)

echo WARNING: tm (TestMe) not found in PATH.
echo TestMe is required for running tests.
echo Install with: bun install @embedthis/testme -g
set HAS_WARNING=1

:check_bash
REM Check for bash.exe in PATH
where bash.exe >nul 2>&1
if %errorlevel% == 0 (
    goto :finish
)

REM Check if bash.exe exists in Program Files\Git\bin
if exist "C:\Program Files\Git\bin\bash.exe" (
    echo Found bash.exe in C:\Program Files\Git\bin
    echo Adding to PATH for this session...
    set "PATH=C:\Program Files\Git\bin;%PATH%"
    goto :finish
)

REM Check alternate location (Program Files (x86))
if exist "C:\Program Files (x86)\Git\bin\bash.exe" (
    echo Found bash.exe in C:\Program Files (x86)\Git\bin
    echo Adding to PATH for this session...
    set "PATH=C:\Program Files (x86)\Git\bin;%PATH%"
    goto :finish
)

echo WARNING: bash.exe not found. Please install Git for Windows.
echo Download from: https://git-scm.com/download/win
exit /b 1

:finish
if defined HAS_WARNING (
    echo Some prerequisites are missing. Please install them before running tests.
    exit /b 1
)
