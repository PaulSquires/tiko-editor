@echo off
rem Build fbcParser.dll (the symbol-extraction engine) and tiko_fbctest.exe
rem (a standalone harness that consumes the DLL like a host app would).
rem
rem The DLL build compiles every .bas module except the harness; the
rem FBCPARSER_BUILDING_DLL define switches fbcParser.bi's declares to the
rem "building" variant. _testfile*.bas are test INPUT for the parser, never
rem compiled. fbc -dll also produces the import library libfbcParser.dll.a,
rem which the harness links against (-p .).

setlocal enabledelayedexpansion
rem THE COMPILER IS FBC-MODERN, resolved through TIKO_FBC (same variable the
rem editor's build scripts use). The editor's own committed 1.10.1 toolchain,
rem two levels up (src\fbcParser -> tiko root), remains the fallback -- and for
rem THIS dll it is a real one: these sources are a fork of fbc's own front end,
rem and if 1.20.0 rejects them the DLL boundary makes building this one engine
rem with the old compiler entirely legal.
rem   set TIKO_FBC=%~dp0..\..\toolchains\FreeBASIC-1.10.1-winlibs-gcc-9.3.0\fbc64.exe
if not defined TIKO_FBC set "TIKO_FBC=C:\dev\fbc-modern\toolchains\fbc-modern-windows\fbc64.exe"
if not exist "%TIKO_FBC%" (
    echo COMPILER NOT FOUND: %TIKO_FBC%
    exit /b 1
)
set FBC=%TIKO_FBC%

rem Everything named _testfile* is parser test INPUT, never compiled.
set SOURCES=
for %%f in (*.bas) do (
    set nm=%%f
    if /i not "!nm:~0,9!"=="_testfile" if /i not "%%f"=="fbcparser-api.bas" if /i not "%%f"=="fbcparser_test.bas" set SOURCES=!SOURCES! "%%f"
)

"%FBC%" -dll -d FBCPARSER_BUILDING_DLL fbcparser-api.bas !SOURCES! -x fbcParser.dll
if errorlevel 1 (
    echo BUILD FAILED: fbcParser.dll
    exit /b 1
)

"%FBC%" fbcparser_test.bas -p . -x tiko_fbctest.exe
if errorlevel 1 (
    echo BUILD FAILED: tiko_fbctest.exe
    exit /b 1
)

echo BUILD OK: fbcParser.dll + tiko_fbctest.exe
