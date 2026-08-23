@echo off
cd /d "%~dp0..\src"
rem ---------------------------------------------------------------------------
rem FAST DEV-LOOP BUILD. Identical to _compile.bat except for -gen gas64.
rem
rem fbc's default 64-bit backend is "gcc": fbc parses, emits C, then hands it to
rem gcc 9.3.0 (which is why the toolchain dir is named winlibs-gcc-9.3.0). The
rem gas64 backend emits assembly directly and skips gcc entirely.
rem
rem MEASURED 2026-07-27 on the full unity build:
rem     gcc    61.8 s, exe 2,208,256 bytes
rem     gas64   6.9 s, exe 1,949,696 bytes      <-- 8.7x faster, and SMALLER
rem Runtime performance of the gas64 exe has NOT been measured -- only build
rem time and correctness. Decide the release backend deliberately.
rem
rem THE TWO BACKENDS ARE NOT INTERCHANGEABLE, AND THE DIFFERENCE IS SILENT.
rem gas64 corrupts static wstring array initializers. tiko had exactly one --
rem spinner(0 to 7) in modDeclares.bi -- and six of its eight frames came out
rem empty, reported only as 8 x "Literal string too big, truncated". Widening
rem the field to "wstring * 3" SILENCES the warning and leaves the data corrupt,
rem so a warning-free gas64 build is not by itself evidence of correctness.
rem Fixed by declaring that array as DWSTRING: a UDT with constructors is built
rem at runtime rather than emitted as static data, avoiding the bug by
rem construction. Static zstring arrays were checked and are unaffected.
rem If that warning reappears, treat it as data corruption -- never fix it by
rem widening the field.
rem
rem Verified equal across both backends: TIKO_THEME_SELFTEST 364/0,
rem TIKO_ENCODING_SELFTEST 26/0, TIKO_AUTOC_SELFTEST 22/0, byte-identical
rem output; plus TIKO_OPTIONS_SELFTEST and an interactive pass by hand.
rem
rem -x puts the exe in the PROJECT ROOT, not src. The exe resolves its support
rem files relative to its own location and errors out at startup if built into src.
rem
rem THE OUTPUT IS tiko-editor.exe. The editor is tiko-editor; tiko is the compiler.
rem ---------------------------------------------------------------------------
rem ---------------------------------------------------------------------------
rem THE COMPILER IS FBC-MODERN (FreeBASIC 1.20.0 + generics, containers,
rem for each, USTRING), resolved through TIKO_FBC. Set that variable to select a
rem different one; the editor's own committed 1.10.1 toolchain is still in
rem toolchains\ and is the rollback path for the whole migration:
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

"%TIKO_FBC%" -gen gas64 -p . -x ..\tiko-editor.exe tiko.bas tiko.rc
