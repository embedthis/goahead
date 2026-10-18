@echo off
REM
REM   make.bat -- Build GoAhead on Windows using the pre-generated VS2022 projects.
REM
REM   windows.bat locates Visual Studio and OpenSSL, then runs msbuild. Extra msbuild options
REM   may be passed on the command line, e.g. "make.bat /p:Configuration=release".
REM
projects\windows.bat msbuild projects\vs2022\goahead.sln /p:Configuration=debug /p:Platform=windows %1 %2 %3 %4 %5 %6 %7 %8 %9
