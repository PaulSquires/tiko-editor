@echo off
setlocal enabledelayedexpansion
rem ---------------------------------------------------------------------------
rem THE STANDALONE COMPILE -- the property src\app exists to have.
rem
rem _check_app_layer.bat greps for a vocabulary. This asks the COMPILER, which
rem is the only thing that cannot have an incomplete list: it builds each app\
rem file against PsCore ALONE -- no AfxNova, no windows.bi, no tiko headers.
rem
rem That distinction is not academic. The grep-based ratchet was satisfied by
rem the whole layer while modIniParse.inc had `dim pS as CTextStream` in it,
rem because AfxNova's classes are named CTextStream and CFileStream and the
rem Afx-prefix rule never sees them. Three vocabulary gaps were found in three
rem audits. The compiler found the fourth in one run.
rem
rem A file that passes here will build inside the eventual shell binary, which
rem is a fresh translation unit with no AfxNova in it. A file that does not,
rem will not -- however green the ratchet is.
rem
rem Exit code is the verdict. Failures are listed with their error counts so
rem progress is visible while the layer is still partway there.
rem ---------------------------------------------------------------------------
set FBC=..\toolchains\FreeBASIC-1.10.1-winlibs-gcc-9.3.0\fbc64.exe
set /a OK=0
set /a BAD=0
pushd "%~dp0src"

for %%F in (app\*.inc app\*.bas) do (
    > _standalone.bas echo #include once "core/PsStr.inc"
    >>_standalone.bas echo #include once "core/PsPath.inc"
    >>_standalone.bas echo #include once "core/PsFile.inc"
    >>_standalone.bas echo #include once "core/PsEncoding.inc"
    rem The layer's OWN declarations first. A file may legitimately depend on
    rem its neighbours' types -- what it may not depend on is anything outside
    rem app\ and PsCore. Leaving these out tested "needs nothing", which is a
    rem different and wrong question.
    rem modThemeKeys.bi is skipped: it is an X-MACRO TABLE, a list of
    rem THTEXTKEY(...) rows that its includer defines the macro for. It is not a
    rem translation unit and cannot compile alone by design -- excluding it is
    rem correct, not a concession.
    for %%B in (app\*.bi) do if /i not "%%~nxB"=="modThemeKeys.bi" >>_standalone.bas echo #include once "%%B"
    >>_standalone.bas echo #include once "%%F"
    %FBC% -i ..\..\PsPlatform\src -maxerr 999 -c _standalone.bas > _standalone.log 2>&1
    if !errorlevel! equ 0 (
        set /a OK+=1
    ) else (
        set /a BAD+=1
        for /f %%C in ('find /c "error" ^< _standalone.log') do echo   FAIL  %%~nxF  (%%C errors^)
    )
)
del /q _standalone.bas _standalone.log _standalone.o 2>nul
popd

echo.
echo   %OK% clean, %BAD% with errors
if %BAD% neq 0 (
    echo.
    echo   src\app must build against PsCore with nothing else in scope. Until it
    echo   does, the shell binary cannot include it -- that binary has no AfxNova.
    exit /b 1
)
echo   ok      src\app builds against PsCore alone
exit /b 0
