@echo off
rem ---------------------------------------------------------------------------
rem RUN tiko WITH PsPlatform'S DLLs REACHABLE.
rem
rem Phase 7d made the editor a PsSciView, so tiko.exe now loads libpsscintilla
rem (the vendored Scintilla+Lexilla fork) and blend2d/freetype/harfbuzz behind
rem it. Not SDL3 -- the Win32 host bridge exists so tiko keeps its own window
rem and message loop.
rem
rem PATH, NOT A COPY. Copying the DLLs next to tiko.exe was tried and fails at
rem load with 0xC0000139 (ENTRYPOINT_NOT_FOUND), naming nothing useful: the
rem copied set was incomplete and there is no obvious way to tell which of them
rem is missing a dependency. Pointing at the real directories always works.
rem
rem THIS IS DEVELOPMENT SCAFFOLDING. Shipping tiko needs a real answer -- the
rem DLLs beside the exe, correct and complete, or statically linked. Packaging
rem is owed and is NOT done.
rem
rem NEVER run src\tiko.exe -- tiko resolves its support files relative to its own
rem location. This runs the one in the project root, which is the correct one.
rem ---------------------------------------------------------------------------
set PSP=%~dp0..\PsPlatform
set PATH=%PSP%\build\out\win64;%PSP%\deps\out\win64\bin;%PATH%
"%~dp0tiko.exe" %*
