@echo off
setlocal
rem ---------------------------------------------------------------------------
rem DOES tiko.exe RUN WITH NOTHING BUT WINDOWS ON PATH?
rem
rem This is the whole point. Every earlier run of tiko since Phase 7d has had
rem C:\dev\PsPlatform on PATH (via _run_tiko.bat), which proves only that the
rem DLLs exist SOMEWHERE. A machine that is not this one has none of those
rem directories, and neither does a user's.
rem
rem So PATH is reduced to the Windows system directories and nothing else. If
rem tiko starts and its self-test prints, the staged set beside the exe is
rem genuinely self-contained. If a DLL is missing or mismatched it fails at load,
rem typically as 0xC0000135 (module not found) or 0xC0000139 (entry point not
rem found) -- the latter being what an INCOMPLETE copy produces, because the
rem loader silently substitutes whatever it can find elsewhere.
rem
rem TIKO_ENCODING_SELFTEST is the probe rather than a bare start: it prints a
rem result and it exercises the editor, so a run that produces its line has got
rem past DLL loading AND past creating a tikoSciHost. A window appearing is not
rem the same evidence.
rem ---------------------------------------------------------------------------
set PATH=%SystemRoot%\system32;%SystemRoot%;%SystemRoot%\system32\Wbem
set TIKO_ENCODING_SELFTEST=1

rem A BOUNDED run, by PID. tiko prints its result and then goes on being an
rem editor -- it never exits -- so a plain `start /wait` hangs forever. PowerShell
rem is used only to get the PID back: `taskkill /im tiko.exe` would also kill an
rem editor the author happens to have open, which is not this script's business.
pushd "%~dp0"
rem BY FULL PATH. PATH was just reduced to the Windows directories, which is
rem the point of this check -- so powershell.exe is not on it either. Adding
rem its directory back would work and would also be one more thing on PATH
rem that the check is supposed to be doing without.
%SystemRoot%\system32\WindowsPowerShell\v1.0\powershell.exe -NoProfile -Command ^
  "$p = Start-Process (Join-Path (Get-Location) 'tiko.exe') -PassThru -NoNewWindow -RedirectStandardOutput _pkgcheck.log -RedirectStandardError _pkgcheck.err;" ^
  "Start-Sleep -Seconds 20; try { $p.Kill() } catch {}"
popd

findstr /c:"encoding self-test" "%~dp0_pkgcheck.log" >nul 2>&1
if errorlevel 1 (
    echo.
    echo   FAIL    tiko did not run with only Windows on PATH.
    echo.
    type "%~dp0_pkgcheck.log" 2>nul
    echo.
    echo   Run _package.bat. If it was already run, re-derive the DLL list --
    echo   the transitive closure may have changed. See _package.bat.
    del "%~dp0_pkgcheck.log" "%~dp0_pkgcheck.err" 2>nul
    exit /b 1
)

findstr /c:"encoding self-test" "%~dp0_pkgcheck.log"
del "%~dp0_pkgcheck.log" "%~dp0_pkgcheck.err" 2>nul
echo.
echo   ok      tiko.exe is self-contained beside its DLLs
exit /b 0
