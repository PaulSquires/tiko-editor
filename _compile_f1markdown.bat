@echo off
cd src
rem ---------------------------------------------------------------------------
rem F1Markdown -- the native markdown help viewer. Built from
rem F1Markdown\F1Markdown.bas, which is a THIRD translation unit: tiko.exe still
rem builds from tiko.bas, tikoshell.exe from shell\tikoshell.bas, and nothing in
rem here touches either.
rem
rem gas64, like _compile_fast.bat and _compile_shell.bat, and for the same
rem reason: gcc is the release backend and a commit is not a release. Read the
rem warning in _compile_fast.bat about gas64 corrupting static wstring array
rem initializers before adding one.
rem
rem ---- THE INCLUDE ROOTS ----------------------------------------------------
rem
rem BOTH, exactly as _compile_shell.bat explains: PsPlatform's own driver passes
rem src and src\bind (build/build.bas:212) and the second is not optional once
rem anything reaches SDL3, because the vendored bindings include each other by
rem their own root -- SDL3\SDL.bi does #include "SDL3/SDL_stdinc.bi".
rem
rem NO -l FLAGS for SDL3, Blend2D, FreeType or HarfBuzz. They resolve through
rem #inclib inside src\bind; deps\out\win64\lib is on the path for the import
rem libraries and that is the whole of it.
rem
rem NO -l psscintilla, and no -p ..\..\PsPlatform\build\out\win64 to find it.
rem This binary includes no Scintilla header and must not link the shim.
rem
rem ---- OUTPUT GOES TO THE PROJECT ROOT, BESIDE tiko.exe ---------------------
rem
rem _compile_shell.bat deliberately does the opposite, and the reason does not
rem apply here: it avoids the project root because libpsscintilla.dll SITS
rem beside tiko.exe and Windows would load that copy in preference to anything
rem on PATH, so a stale shim would take out both binaries at once. F1Markdown
rem references no Scintilla symbol, so there is nothing for that DLL to shadow.
rem
rem What it DOES need beside it is the render stack, and that is why the root is
rem the right place: libblend2d.dll, libfreetype.dll, libharfbuzz.dll and
rem libwinpthread-1.dll were already there for tiko's own use, and SDL3.dll was
rem staged next to them for this. A partial copy fails at load with 0xC0000139
rem naming nothing useful -- see _check_scihost.bat -- so if that appears, add
rem the libharfbuzz-{gpu,raster,subset,vector}.dll variants from
rem ..\PsPlatform\deps\out\win64\bin as well.
rem
rem ---- THE FONTS ------------------------------------------------------------
rem
rem assets\f1markdown\fonts\ holds six STATIC faces. PsPlatform's own
rem CascadiaCode.ttf is a VARIABLE font and TE_Init sets no variation axis, so it
rem cannot supply bold or italic -- see the header of F1Markdown.bas.
rem ---------------------------------------------------------------------------
..\toolchains\FreeBASIC-1.10.1-winlibs-gcc-9.3.0\fbc64.exe -gen gas64 -p . ^
    -i ..\..\PsPlatform\src -i ..\..\PsPlatform\src\bind ^
    -p ..\..\PsPlatform\deps\out\win64\lib ^
    -x ..\F1Markdown.exe F1Markdown\F1Markdown.bas
