cd src
rem -x puts the exe in the PROJECT ROOT, not src. tiko.exe resolves its support
rem files relative to its own location and errors out at startup if built into src.
..\toolchains\FreeBASIC-1.10.1-winlibs-gcc-9.3.0\fbc64.exe -p . -x ..\tiko.exe tiko.bas tiko.rc
