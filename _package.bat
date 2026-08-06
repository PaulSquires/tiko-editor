@echo off
setlocal
rem ---------------------------------------------------------------------------
rem STAGE PsPlatform'S RUNTIME DLLs BESIDE tiko.exe.
rem
rem Phase 7d made the editor a PsSciView, so tiko.exe now loads the vendored
rem Scintilla fork and the Blend2D/FreeType/HarfBuzz stack behind it. Those five
rem files must sit next to the exe, exactly as Scintilla64.dll and Lexilla64.dll
rem already do -- they are shipped binaries and are committed, per the same rule
rem the .gitignore states for WebView2Loader.dll.
rem
rem ---- THE FIVE, AND WHY IT IS EXACTLY FIVE --------------------------------
rem
rem This list is not a guess. It is the transitive import closure of tiko.exe
rem with the Windows system DLLs removed, computed with objdump -p. Re-derive it
rem rather than adding to it by hand if the set ever changes.
rem
rem   libpsscintilla.dll    the fork
rem   libblend2d.dll        the rasteriser
rem   libfreetype.dll       )  reached through PsTextEngine, not by the fork
rem   libharfbuzz.dll       )  directly
rem   libwinpthread-1.dll   libpsscintilla imports it -- see the warning below
rem
rem NOT SDL3.dll. The Win32 host bridge exists so tiko keeps its own window and
rem message loop, and the closure confirms it: SDL3 is not reached. Nor are the
rem harfbuzz-gpu/raster/subset/vector variants.
rem
rem ---- THE TRAP THAT COST AN AFTERNOON -------------------------------------
rem
rem An INCOMPLETE copy does not fail with "file not found". It fails at load with
rem 0xC0000139 -- STATUS_ENTRYPOINT_NOT_FOUND -- naming nothing useful.
rem
rem libwinpthread-1.dll lives ONLY in build\out\win64; the other four are also in
rem deps\out\win64\bin. Copying from the deps directory alone leaves it out, the
rem loader then finds SOME OTHER libwinpthread on PATH (any mingw install will
rem do), and the mismatched ABI reports a missing entry point rather than a
rem missing file. That is why this script names its sources explicitly instead of
rem globbing a directory.
rem
rem ---- VERIFY IT ------------------------------------------------------------
rem
rem _check_package.bat runs tiko.exe with a MINIMAL PATH -- no PsPlatform, no
rem mingw -- which is the only way to prove the staged set actually stands alone.
rem Running it with the dev directories on PATH proves nothing at all.
rem ---------------------------------------------------------------------------
set PSP=%~dp0..\PsPlatform
set BIN=%PSP%\build\out\win64
set OK=1

for %%F in (libpsscintilla.dll libblend2d.dll libfreetype.dll libharfbuzz.dll libwinpthread-1.dll) do (
    if not exist "%BIN%\%%F" (
        echo   MISSING  %BIN%\%%F
        set OK=0
    ) else (
        copy /y "%BIN%\%%F" "%~dp0%%F" >nul
        echo   staged   %%F
    )
)

if "%OK%"=="0" (
    echo.
    echo   Build PsPlatform first:  cd ..\PsPlatform ^&^& build scintilla
    exit /b 1
)
echo.
echo   ok      5 runtime DLLs staged beside tiko.exe
echo           now run _check_package.bat -- staging is not evidence
exit /b 0
