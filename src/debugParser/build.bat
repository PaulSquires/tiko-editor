@echo off
rem Build debugParser.dll (the FreeBASIC debug engine) and tiko_dbgtest.exe
rem (a standalone harness that consumes the DLL exactly as a host app does).
rem
rem DEBUGPARSER_BUILDING_DLL switches debugParser.bi's declares to the "building"
rem variant. fbc -dll also produces the import library libdebugParser.dll.a, which
rem the harness links against (-p .).
rem
rem The engine is 64-bit. It can debug BOTH 64-bit (-gen gas64) and 32-bit (-gen gas)
rem targets -- register access goes through Wow64GetThreadContext for the latter.

setlocal
rem THE COMPILER IS FBC-MODERN, resolved through TIKO_FBC (same variable the
rem editor's build scripts use). The editor's own committed 1.10.1 toolchain,
rem two levels up (src\debugParser -> tiko root), is the rollback:
rem   set TIKO_FBC=%~dp0..\..\toolchains\FreeBASIC-1.10.1-winlibs-gcc-9.3.0\fbc64.exe
if not defined TIKO_FBC set "TIKO_FBC=C:\dev\fbc-modern\toolchains\fbc-modern-windows\fbc64.exe"
if not exist "%TIKO_FBC%" (
    echo COMPILER NOT FOUND: %TIKO_FBC%
    exit /b 1
)
set FBC=%TIKO_FBC%

"%FBC%" -dll -gen gas64 -w all debugparser-api.bas -x debugParser.dll
if errorlevel 1 (
    echo BUILD FAILED: debugParser.dll
    exit /b 1
)

"%FBC%" -gen gas64 -w all debugparser_test.bas -p . -x tiko_dbgtest.exe
if errorlevel 1 (
    echo BUILD FAILED: tiko_dbgtest.exe
    exit /b 1
)

echo BUILD OK
