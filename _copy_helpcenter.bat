@echo off
rem Stage the generated Help Center site into tiko.
rem
rem   C:\dev\HelpCenter\site  ->  settings\helpcenter
rem
rem The Help Center window (F1) loads settings\helpcenter\index.html through WebView2.
rem That tree is NOT in the repo -- ~31,700 files / ~147 MB, and every helpgen regeneration
rem rewrites all of them -- so a fresh clone MUST run this script once or the Help Center
rem reports its files as missing.
rem
rem Re-run after every `python -m helpgen generate` in C:\dev\HelpCenter.
rem
rem /MIR mirrors, which means it DELETES files in settings\helpcenter that are no longer in
rem the source. That is wanted here and is the reason a plain xcopy is not used: a
rem regenerated site drops pages when an upstream symbol goes away, and a stale page left
rem behind stays reachable from the search index and the sitemap. The destination is a pure
rem copy with nothing hand-edited in it, so there is nothing of ours for /MIR to destroy.

set SRCDIR=C:\dev\HelpCenter\site
set DSTDIR=%~dp0settings\helpcenter

if not exist "%SRCDIR%\index.html" (
    echo FAILED: no generated site at "%SRCDIR%"
    echo Run "python -m helpgen generate" from C:\dev\HelpCenter\tools first.
    exit /b 1
)

echo Mirroring "%SRCDIR%" -^> "%DSTDIR%" ...
robocopy "%SRCDIR%" "%DSTDIR%" /MIR /NFL /NDL /NJH /NJS /NP /R:1 /W:1

rem robocopy is not like every other tool: 0 means "nothing needed copying", 1 means "files
rem copied", and only 8 and above are real failures. Testing for nonzero would report a
rem successful copy as an error.
if %ERRORLEVEL% GEQ 8 (
    echo COPY FAILED ^(robocopy exit %ERRORLEVEL%^)
    exit /b 1
)

echo COPY OK
