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
rem
rem ---- THE HEADER ORDER COMES FROM tiko.bas, NOT FROM THE DIRECTORY ---------
rem
rem This used to include app\*.bi ALPHABETICALLY, and that is wrong: the layer's
rem headers are not order-independent and say so themselves. modFormat.bi carries
rem the line "This file is included AHEAD of clsConfig.bi because clsConfig
rem embeds a FORMAT_RULES" -- and alphabetically clsConfig comes first. Same for
rem clsSymbolDb.bi, which needs FBCP_KIND_* from fbcParser.bi.
rem
rem So the gate reported failures that were its OWN ordering, mixed with real
rem ones and indistinguishable from them in a count. It now reads the order out
rem of tiko.bas -- the one place that already has to get it right -- so the list
rem cannot drift from the build.
rem
rem modThemeKeys.bi needs no special case any more: tiko.bas does not include it,
rem so it is simply absent. It is an X-MACRO TABLE of THTEXTKEY(...) rows whose
rem macro the includer defines -- not a translation unit, and not compilable
rem alone by design.
rem ---------------------------------------------------------------------------
set FBC=..\toolchains\FreeBASIC-1.10.1-winlibs-gcc-9.3.0\fbc64.exe
set /a OK=0
set /a BAD=0
pushd "%~dp0src"

rem The prelude, built once: every app\*.bi that tiko.bas includes, in tiko.bas's
rem own order. PowerShell only because batch cannot split a string on a quote.
powershell -NoProfile -Command ^
  "Select-String -Path tiko.bas -Pattern 'include once \"app/(.+\.bi)\"' |" ^
  "ForEach-Object { '#include once \"app/' + $_.Matches[0].Groups[1].Value + '\"' } |" ^
  "Set-Content -Encoding ascii _prelude.txt"


for %%F in (app\*.inc app\*.bas) do (
    > _standalone.bas echo #include once "core/PsStr.inc"
    >>_standalone.bas echo #include once "core/PsPath.inc"
    >>_standalone.bas echo #include once "core/PsFile.inc"
    >>_standalone.bas echo #include once "core/PsEncoding.inc"
    rem The layer's OWN declarations first, IN tiko.bas's ORDER -- see the note
    rem at the top of this file about why alphabetical was wrong.
    type _prelude.txt >>_standalone.bas
    >>_standalone.bas echo #include once "%%F"
    %FBC% -i ..\..\PsPlatform\src -maxerr 999 -c _standalone.bas > _standalone.log 2>&1
    if !errorlevel! equ 0 (
        set /a OK+=1
    ) else (
        set /a BAD+=1
        rem find.exe BY FULL PATH. This .bat is routinely run from Git Bash, which
        rem puts its own PATH in front -- so a bare `find` resolves to UNIX find,
        rem which ignores /c, treats "error" as a path and walks the whole of C:\.
        rem The gate did not fail when that happened; it HUNG, on the first file
        rem with an error, which reads exactly like a slow compile.
        for /f %%C in ('%SystemRoot%\system32\find.exe /c "error" ^< _standalone.log') do echo   FAIL  %%~nxF  (%%C errors^)
    )
)
del /q _standalone.bas _standalone.log _standalone.o _prelude.txt 2>nul
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
