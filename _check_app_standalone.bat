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
rem ---------------------------------------------------------------------------
rem PART TWO: THE WHOLE LAYER, LINKED.
rem
rem Everything above compiles each file SEPARATELY with -c. That answers "does
rem this file need AfxNova" and nothing else -- and two things it cannot see are
rem exactly the two that went wrong:
rem
rem   1. WHETHER THE SIXTEEN HEADERS AGREE WITH EACH OTHER. Compiled one at a
rem      time, each gets the prelude fresh; nothing ever puts all of them in one
rem      translation unit.
rem   2. WHETHER ANYTHING IS MISSING A BODY. -c never links, so an undefined
rem      symbol is invisible to it BY CONSTRUCTION.
rem
rem The second one was live for months. app/clsConfig.bi declares
rem `dim shared gConfig as clsConfig`, so including the header instantiates the
rem object -- but the constructor sat in src\clsConfig.inc, in the SHELL. The
rem layer could not link on its own, and this gate reported 7 clean / 0 errors
rem throughout. It was found by phase 7c's shell binary, the first thing that
rem ever linked the layer, and the fix moved the constructor to app\clsConfig.inc.
rem
rem So this half links a real executable out of the prelude plus every app\*.inc,
rem against PsCore alone. It is cheap -- one fbc invocation -- and it is the only
rem check in either tree that tests the layer as a LAYER rather than as files.
rem ---------------------------------------------------------------------------
rem THE SAME ORDER AS THE PER-FILE HALF ABOVE: PsCore FIRST, then the layer's own
rem declarations, then the bodies. Getting it backwards is not a subtle failure --
rem the app headers declare DWSTRING parameters, so with the prelude ahead of
rem core\ there are two DWSTRINGs in scope and every signature mismatches its own
rem definition. It reads as dozens of unrelated type errors deep in app\ code.
echo.
> _standalone_link.bas echo #include once "core/PsStr.inc"
>>_standalone_link.bas echo #include once "core/PsPath.inc"
>>_standalone_link.bas echo #include once "core/PsFile.inc"
>>_standalone_link.bas echo #include once "core/PsEncoding.inc"
type _prelude.txt >>_standalone_link.bas
for %%F in (app\*.inc) do >>_standalone_link.bas echo #include once "%%F"
>>_standalone_link.bas echo end 0

rem A RATCHET, NOT A PASS/FAIL. ONE body is still outside the layer and is named
rem below; the check fails when a SECOND appears, not while that one remains. A gate that is red on the day it lands gets switched off, and a debt
rem that is counted is a debt someone can finish.
rem
rem   ProcessFromCurdriveApp          CLOSED in 7c step 5. It is in app\modPaths.inc
rem                                   now. This entry said it "moves down as soon
rem                                   as someone does it", and the shell's scanner
rem                                   is who did -- app\clsSymbolDb.inc calls it at
rem                                   three sites, so a second binary including
rem                                   that body could not link without it.
rem   FilenameOriginalCase            CLOSED in 7c step 8. This entry said it needed
rem                                   "a PsCore canonical-path call first" -- and PsCore
rem                                   had one the whole time, PsFileRealCase, which walks
rem                                   each component against the real directory entries.
rem                                   Nobody looked. The body is in app\modPaths.inc now
rem                                   and BOTH binaries answer it the same way; tiko's
rem                                   CreateFileW / GetFinalPathNameByHandleW version is
rem                                   gone, and with it the symlink resolution it did as
rem                                   a side effect
rem   KeyBindings_PickListKeyToValue  CLOSED in 7c step 10, and it was never the
rem                                   right function to call. app\modMenuDefinitions.inc
rem                                   wanted to know whether a stored key name is one
rem                                   the pick list offers, and asked for a VIRTUAL KEY
rem                                   in order to throw it away -- while that function's
rem                                   own header said "MEMBERSHIP IN gKeyNames() is the
rem                                   validity test here, never 'the return is
rem                                   non-zero'". The membership half is now
rem                                   KeyBindings_IsPickListName, in app\, with the
rem                                   vocabulary it needs; the VK half stayed in the
rem                                   shell. The `#include "../modKeyBindings.bi"` that
rem                                   this entry was really about is gone with it.
rem
rem   clsConfig::ProjectSaveToFile    clsConfig is SPLIT -- header and constructor in app\,
rem                                   the rest in src\clsConfig.inc since step 1.
rem
rem clsConfig::ProjectSaveToFile ARRIVED WITH 7c STEP 3 COMMIT 3e, for a good reason: moving
rem clsDocument and clsApp INTO the layer pulled callers in whose callees are still outside
rem it. That is what this counter is for -- it went 3 -> 6, then back to 4 in commit 5 once
rem Doc_EncodeForDisk and Doc_WriteToDisk followed their declarations down. THE BASELINE IS
rem LOWERED WHENEVER THE COUNT IS, or it stops being a ratchet and becomes a licence.
rem
rem 7c STEP 5 TOOK IT 4 -> 3, and TWO entries closed to get there while one opened:
rem ProcessFromCurdriveApp moved down, TodoStore_RemoveFile turned out to have had a real
rem body in app\clsSymbolDb.inc all along (the shell was supplying a stub only because it
rem included that file's header and not its body), and FilenameOriginalCase became a live
rem debt rather than a latent one -- the shell has to implement it now that it links the
rem symbol database.
rem
rem 7c STEP 10 TOOK IT 2 -> 1, and the entry above is the third in three steps whose
rem stated blocker did not survive being re-read. What remains is
rem clsConfig::ProjectSaveToFile, and THAT one is blocked on something real: it calls
rem gTTabCtl -- clsTopTabCtl, Win32 to its bones -- at four sites. It cannot close until
rem the tab control does, which is the largest open item in the port.
rem
rem 7c STEP 8 TOOK IT 3 -> 2. FilenameOriginalCase moved down, and the entry above records
rem why it took five steps: the blocker it named had already been removed, by someone who
rem was not looking for it. A debt list is only as good as the last time its entries were
rem re-read rather than re-counted.
rem
rem 7c STEP 13 TOOK IT 1 -> 0, AND THE BASELINE IS GONE. This file said to delete it when
rem the count reached zero, and leaving a baseline of 0 in place would have been a licence
rem rather than a ratchet: the next undefined symbol would have been compared against it and
rem reported as "debt" instead of failing.
rem
rem THE LAST ENTRY'S STATED BLOCKER WAS THE FOURTH IN THIS FILE NOT TO SURVIVE BEING
rem RE-READ. It said ProjectSaveToFile "cannot close until the tab control does, which is the
rem largest open item in the port". clsTopTabCtl was not touched. A project file records
rem which documents are open and in what ORDER -- a question about a list -- so four fields
rem went onto AppHostServices (TabCount, TabDocAt, TabActiveIndex, TabIndexOfDoc), the
rem twenty lines of SaveActiveTabIndex became Tabs_SaveActiveIndex in app\, and the function
rem moved down.
rem
rem AND CLOSING IT EXPOSED THE NEXT ONE, which is what this counter has always done: with
rem ProjectSaveToFile in app\, ProcessToCurdriveProject became undefined. It moved too --
rem pure gApp and PsCore, and on the same "moves as soon as something needs it" list
rem ProcessFromCurdriveApp was on in step 5. 1 -> 1 -> 0 inside one commit.
set /a LINKBAD=0
%FBC% -i ..\..\PsPlatform\src -maxerr 999 -x _standalone_link.exe _standalone_link.bas > _standalone_link.log 2>&1
if !errorlevel! neq 0 (
    rem Distinct symbol names, not error lines -- one symbol is referenced from
    rem several call sites and ld reports each of them.
    for /f %%C in ('%SystemRoot%\system32\findstr.exe /c:"undefined reference" _standalone_link.log ^| %SystemRoot%\system32\find.exe /c "undefined"') do set /a LINKREFS=%%C
    for /f %%C in ('powershell -NoProfile -Command "(Select-String -Path _standalone_link.log -Pattern 'undefined reference to .(.+).$' | ForEach-Object { $_.Matches[0].Groups[1].Value } | Sort-Object -Unique).Count"') do set /a LINKSYMS=%%C
    set /a LINKBAD=1
    echo   FAIL    the app layer has !LINKSYMS! bodies outside it ^(!LINKREFS! call sites^):
    powershell -NoProfile -Command "Select-String -Path _standalone_link.log -Pattern 'undefined reference to .(.+).$' | ForEach-Object { '            ' + $_.Matches[0].Groups[1].Value } | Sort-Object -Unique"
)

del /q _standalone.bas _standalone.log _standalone.o _prelude.txt 2>nul
del /q _standalone_link.bas _standalone_link.log _standalone_link.exe 2>nul
popd

echo.
echo   %OK% clean, %BAD% with errors
if %BAD% neq 0 (
    echo.
    echo   src\app must build against PsCore with nothing else in scope. Until it
    echo   does, the shell binary cannot include it -- that binary has no AfxNova.
    exit /b 1
)
if !LINKBAD! neq 0 (
    echo.
    echo   AN app-layer body lives outside the app layer. It compiles cleanly
    echo   file by file -- -c never links -- and breaks the moment anything
    echo   builds a binary out of the layer. Move the body into app\.
    echo.
    echo   THERE IS NO BASELINE ANY MORE. The count reached 0 in 7c step 13 and
    echo   this became a plain failure; do not reintroduce one to get green.
    exit /b 1
)
echo   ok      src\app builds against PsCore alone
exit /b 0
