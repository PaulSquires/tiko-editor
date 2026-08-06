cd src
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
rem
rem PHASE 7d: the editor is now a PsSciView, so this links PsPlatform's render
rem stack -- libpsscintilla (the vendored Scintilla+Lexilla fork) plus blend2d,
rem freetype and harfbuzz behind it. NOT SDL3: the Win32 host bridge exists
rem precisely so tiko keeps its own window and message loop.
rem
rem The DLLs are NOT copied next to tiko.exe. A partial copy fails at load with
rem 0xC0000139 (ENTRYPOINT_NOT_FOUND) naming nothing useful, so PATH points at
rem the real directories -- see _run_tiko.bat. Packaging is still owed.
..\toolchains\FreeBASIC-1.10.1-winlibs-gcc-9.3.0\fbc64.exe -p . -i ..\..\PsPlatform\src ^
    -p ..\..\PsPlatform\deps\out\win64\lib -p ..\..\PsPlatform\build\out\win64 -l psscintilla ^
    -x ..\tiko.exe tiko.bas tiko.rc
