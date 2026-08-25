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

rem The large harness fixture, built WITH -g because reading that debug info is the
rem whole point of this engine.
rem
rem _testfile_arrays.exe is built ONLY IF MISSING. *.exe is gitignored here, so it is a
rem LOCAL artifact: a fresh clone has no fixture at all and the harness would have
rem nothing to read. Building it on demand fixes that. Not rebuilding it when it is
rem already there is also deliberate -- an existing copy on this machine was produced by
rem FreeBASIC 1.10.1, and keeping it proves the engine still reads the debug info a
rem 1.10.1 toolchain emits, which is what a user's own project will produce.
rem
rem _testfile_big.bas exists for
rem a reason worth stating: DBG_GROW allocates 128 entries up front, so the small fixture
rem NEVER GROWS A SINGLE TABLE. Every doubling path in dbginfo.bas was untested by anything.
rem The big one has 60 types, 300 fields and ~1300 lines, which pushes gDbgLine, gDbgVar and
rem gDbgField well past the initial allocation.
if not exist _testfile_arrays.exe (
    "%FBC%" -g -gen gas64 _testfile_arrays.bas -x _testfile_arrays.exe
    if errorlevel 1 (
        echo BUILD FAILED: _testfile_arrays.exe
        exit /b 1
    )
)

"%FBC%" -g -gen gas64 _testfile_big.bas -x _testfile_big.exe
if errorlevel 1 (
    echo BUILD FAILED: _testfile_big.exe
    exit /b 1
)

echo BUILD OK
