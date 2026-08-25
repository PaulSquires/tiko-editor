@echo off
cd /d "%~dp0..\src"
rem ---------------------------------------------------------------------------
rem RELEASE BUILD (gcc backend). See _compile_fast.bat for the gas64 dev loop,
rem which is what every ordinary build uses.
rem
rem THE COMPILER IS FBC-MODERN, resolved through TIKO_FBC. Set that variable to
rem select a different one; the editor's own committed 1.10.1 toolchain is still
rem in toolchains\ and is the rollback path:
rem   set TIKO_FBC=%~dp0..\toolchains\FreeBASIC-1.10.1-winlibs-gcc-9.3.0\fbc64.exe
rem
rem FBC-Modern must be RUN WHERE IT LIVES -- it derives its prefix from its own
rem path, and copying just the exe gives "cannot open linker script file".
rem
rem NO -s gui. tiko-editor.exe is a CONSOLE-subsystem binary on purpose: the
rem TIKO_*_SELFTEST suites print their pass/fail reports to stdout and the test
rem runner redirects it. -s gui would discard every one of them silently.
rem ---------------------------------------------------------------------------
if not defined TIKO_FBC set "TIKO_FBC=C:\dev\fbc-modern\toolchains\fbc-modern-windows\fbc64.exe"
if not exist "%TIKO_FBC%" (
    echo COMPILER NOT FOUND: %TIKO_FBC%
    echo Set TIKO_FBC to an fbc64.exe, or install FBC-Modern at C:\dev\fbc-modern.
    exit /b 1
)

rem Say which compiler this build actually used. A build that silently ran the
rem wrong compiler produces a believable exe, not an error.
echo [compiler] %TIKO_FBC%
for /f "delims=" %%v in ('"%TIKO_FBC%" -version 2^>^&1') do echo [version]  %%v& goto :gotver
:gotver

rem -x puts the exe in the PROJECT ROOT, not src. The exe resolves its support
rem files relative to its own location and errors out at startup if built into src.
"%TIKO_FBC%" -p . -x ..\tiko-editor.exe tiko.bas tiko.rc
