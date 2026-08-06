@echo off
cd src
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
rem -i ..\..\PsPlatform\src -- PsCore, the portable core tiko is migrating onto
rem (Phase 7a). It is a SIBLING REPOSITORY, not a vendored copy: a copy would fork
rem the moment either side changed, and PsCore's whole value is that the toolkit
rem and the application share ONE implementation. Building tiko therefore needs
rem PsPlatform checked out beside it.
rem
rem The flag is INERT until something includes a PsCore header, so it is safe on
rem this branch before the swap lands. _compile_probe.bat checks the path
rem resolves without waiting for that.
rem
rem -x puts the exe in the PROJECT ROOT, not src. tiko.exe resolves its support
rem files relative to its own location and errors out at startup if built into src.
rem ---------------------------------------------------------------------------
rem
rem PHASE 7d: the editor is now a PsSciView, so this links PsPlatform's render
rem stack -- libpsscintilla (the vendored Scintilla+Lexilla fork) plus blend2d,
rem freetype and harfbuzz behind it. NOT SDL3: the Win32 host bridge exists
rem precisely so tiko keeps its own window and message loop.
rem
rem The DLLs are NOT copied next to tiko.exe. A partial copy fails at load with
rem 0xC0000139 (ENTRYPOINT_NOT_FOUND) naming nothing useful, so PATH points at
rem the real directories -- see _run_tiko.bat. Packaging is still owed.
..\toolchains\FreeBASIC-1.10.1-winlibs-gcc-9.3.0\fbc64.exe -gen gas64 -p . -i ..\..\PsPlatform\src ^
    -p ..\..\PsPlatform\deps\out\win64\lib -p ..\..\PsPlatform\build\out\win64 -l psscintilla ^
    -x ..\tiko.exe tiko.bas tiko.rc
