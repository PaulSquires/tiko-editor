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
..\toolchains\FreeBASIC-1.10.1-winlibs-gcc-9.3.0\fbc64.exe -p . -i ..\..\PsPlatform\src -x ..\tiko.exe tiko.bas tiko.rc
