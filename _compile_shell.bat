@echo off
cd src
rem ---------------------------------------------------------------------------
rem PHASE 7c'S SHELL BINARY. Built from shell\tikoshell.bas, which is a SECOND
rem translation unit -- tiko.exe still builds from tiko.bas and is untouched by
rem anything in here. See shell\tikoshell.bas for why the shell is a separate TU
rem rather than a mode of tiko.exe or a PsPlatform demo.
rem
rem gas64, like _compile_fast.bat, and for the same reason: gcc is the release
rem backend and this branch is not shippable. Read the warning in that file about
rem gas64 corrupting static wstring array initializers before adding one.
rem
rem ---- THE ONE FLAG THAT IS NOT IN _compile_fast.bat ------------------------
rem
rem   -i ..\..\PsPlatform\src\bind
rem
rem PsPlatform's own driver passes BOTH src and src\bind (build/build.bas:212),
rem and the second is not optional once anything reaches SDL3: the vendored
rem bindings include each other by their own root -- SDL3\SDL.bi does
rem #include "SDL3/SDL_stdinc.bi" -- so they only resolve with src\bind on the
rem include path. tiko.exe has never needed it because the Win32 host bridge
rem exists precisely so tiko keeps its own window and message loop, and nothing
rem in tiko.bas reaches the SDL3 layer.
rem
rem THERE IS NO NEW LIBRARY FLAG. SDL3 links by #inclib "SDL3" inside
rem bind\SDL3\SDL.bi, and deps\out\win64\lib -- where libSDL3.dll.a lives -- is
rem already on the link path for the render stack. The library half of "does
rem tiko link SDL3" was already solved; only the include root was missing.
rem
rem ---- OUTPUT GOES TO _shell\, NOT THE PROJECT ROOT -------------------------
rem
rem Deliberate, and not tidiness. libpsscintilla.dll SITS BESIDE tiko.exe and is
rem tracked in git; Windows loads that copy in preference to anything on PATH.
rem A shell binary in the same directory would silently bind to it too, so a
rem stale shim would take out both binaries at once -- and the failure mode is
rem exit 127 with no message and no dialog, which reads as a broken harness
rem rather than a broken build. Separate directories keep the two exes from
rem shadowing each other's dependencies.
rem
rem The DLLs are NOT copied next to it, for the reason _check_scihost.bat gives:
rem a partial copy fails at load with 0xC0000139 naming nothing useful. PATH
rem points at the real directories instead -- see _run_shell.bat.
rem ---------------------------------------------------------------------------
if not exist ..\_shell mkdir ..\_shell
..\toolchains\FreeBASIC-1.10.1-winlibs-gcc-9.3.0\fbc64.exe -gen gas64 -p . ^
    -i ..\..\PsPlatform\src -i ..\..\PsPlatform\src\bind ^
    -p ..\..\PsPlatform\deps\out\win64\lib -p ..\..\PsPlatform\build\out\win64 -l psscintilla ^
    -x ..\_shell\tikoshell.exe shell\tikoshell.bas
