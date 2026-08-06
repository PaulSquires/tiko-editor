@echo off
setlocal
rem ---------------------------------------------------------------------------
rem THE 7d FEASIBILITY GATE -- can tiko host a PsSciView at all?
rem
rem Phase 7d replaces CreateWindowEx(0, "Scintilla", ...) with a PsSciView driven
rem through PsPlatform's Win32 host bridge. Three things had to be true first,
rem and none was obvious from reading:
rem
rem   1. tiko must not have to link SDL3 -- otherwise the bridge bought nothing
rem   2. PsSciView must compile with windows.bi AND AfxNova already in scope
rem   3. AfxNova's DWSTRING must be undisturbed afterwards (~1400 sites)
rem
rem See _check_scihost.bas for the two failures this already caught. Both were
rem link-time or C-level, invisible to inspection, and found in about a minute
rem by compiling.
rem
rem THE DLLs MUST BE ON PATH, NOT COPIED. Copying libpsscintilla.dll and the
rem deps/out/win64/bin set next to the exe still failed with 0xC0000139
rem (ENTRYPOINT_NOT_FOUND) -- the copied set was incomplete and the missing
rem entry point named nothing useful. Pointing PATH at the real directories is
rem both correct and what tiko itself will have to do once 7d lands.
rem ---------------------------------------------------------------------------
set FBC=..\toolchains\FreeBASIC-1.10.1-winlibs-gcc-9.3.0\fbc64.exe
set PSP=..\..\PsPlatform
pushd "%~dp0src"

%FBC% -gen gcc -i %PSP%\src -p %PSP%\deps\out\win64\lib -p %PSP%\build\out\win64 ^
      -l psscintilla _check_scihost.bas
if errorlevel 1 (
    echo.
    echo   the probe did not BUILD -- 7d's footing is gone, not merely untested
    popd
    exit /b 1
)

set PATH=%PSP%\build\out\win64;%PSP%\deps\out\win64\bin;%PATH%
rem .\ is required: PATH was just prepended with two directories, and cmd
rem searches PATH before the current directory.
.\_check_scihost.exe
set RC=%errorlevel%
del /q _check_scihost.exe _check_scihost.o 2>nul
popd
exit /b %RC%
