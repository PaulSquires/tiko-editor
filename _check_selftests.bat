@echo off
setlocal
rem ---------------------------------------------------------------------------
rem EVERY SELF-TEST THAT RUNS AT STARTUP, IN ONE RUN.
rem
rem WHY THIS EXISTS. Twenty-one suites fire during tiko's startup, each behind
rem its own TIKO_*_SELFTEST environment variable, and until 7c step 14 NOT ONE
rem of them was in any gate. They were run by hand, when someone remembered, in
rem the step that happened to touch them.
rem
rem So one of them had been failing. TIKO_OPTIONS_SELFTEST reported "11 passed,
rem 6 failed" and had done for some time -- found in step 13 while closing
rem something else, and confirmed not to be that step's doing by stashing its
rem commit and re-running. Nineteen thousand assertions sat behind an
rem environment variable nobody set.
rem
rem THE MECHANISM IS _check_package.bat's, deliberately. That gate already
rem solved starting a GUI from a batch file: launch with output redirected,
rem poll with a ceiling rather than sleeping a fixed time, tree-kill by PID,
rem WaitForExit before reading the log. Inventing a second way to do the same
rem thing would mean two of them to keep working.
rem
rem (7c step 11's report said suites like these "could not be run here" because
rem they need the GUI. Step 12 showed otherwise using exactly this trick, which
rem is what made this gate obvious in step 14.)
rem
rem ---- WHAT IT PARSES -------------------------------------------------------
rem
rem EVERY line matching "<n> passed, <n> failed", because the suites print in at
rem least four different shapes:
rem
rem     --- theme self-test: 929 passed, 0 failed ---
rem     TIKO_SAVE_SELFTEST: 44 passed, 0 failed.
rem     Format engine self-test: 68 passed, 0 failed
rem     ==== TIKO_INPUTBOX_SELFTEST: 34 passed, 0 failed ====
rem
rem A parser keyed to any one of those silently ignores the rest -- which is the
rem same failure this gate exists to end, in a different disguise.
rem
rem ---- THE ASSERTION THAT ACTUALLY MATTERS ----------------------------------
rem
rem THE MINIMUM SUITE COUNT. Summing failures alone is not a gate: a tiko that
rem crashes, or exits early, or never reaches the self-test block prints no
rem report lines at all, sums to zero failures and PASSES. So the count of
rem report lines is asserted against MINSUITES below, and "it did not run" can
rem no longer look like "it passed".
rem
rem The totals are printed rather than only the failures, so a DROP in
rem assertions is visible. A suite that quietly stops running is the same defect
rem as a suite that quietly fails.
rem
rem ---- COST -----------------------------------------------------------------
rem
rem About 20 seconds, and the keyboard suite alone is 18,148 assertions. This is
rem a gate to run deliberately -- before a commit that touches settings, themes,
rem formatting or key bindings, and before a release -- not on every build.
rem ---------------------------------------------------------------------------

rem Raise this when a suite is ADDED. Lower it only with a reason: a suite that
rem stops reporting is exactly what this number is here to catch.
rem 25, MEASURED by running it rather than counted by reading. The first draft
rem said 21 -- the number of suites the source looked like it had -- and the run
rem reported 25: the keyboard suite prints THREE lines, Format prints two, and
rem FindProj prints a model line and a main one. A count of report LINES is what
rem this gate can actually check, so that is what it checks.
set /a MINSUITES=25

pushd "%~dp0"

rem Every startup-safe suite. The AUTOOPEN/AUTORUN variables are deliberately
rem NOT set: those drive dialogs and would make this gate interactive.
set TIKO_THEME_SELFTEST=1
set TIKO_OPTIONS_SELFTEST=1
set TIKO_ENCODING_SELFTEST=1
set TIKO_SAVE_SELFTEST=1
set TIKO_COMPILECMD_SELFTEST=1
set TIKO_COPYDATA_SELFTEST=1
set TIKO_INIPARSE_SELFTEST=1
set TIKO_FONTFILE_SELFTEST=1
set TIKO_DEBUG_SELFTEST=1
set TIKO_UNUSED_SELFTEST=1
set TIKO_FORMAT_SELFTEST=1
set TIKO_FORMATOPTIONS_SELFTEST=1
set TIKO_AUTOINSERT_SELFTEST=1
set TIKO_KEYBOARD_SELFTEST=1
set TIKO_INPUTBOX_SELFTEST=1
set TIKO_CODETIP_SELFTEST=1
set TIKO_NAVHISTORY_SELFTEST=1
set TIKO_FILEWATCH_SELFTEST=1
set TIKO_AUTOC_SELFTEST=1
set TIKO_OUTPUTPANEL_SELFTEST=1
set TIKO_EXPLORER_SELFTEST=1
set TIKO_WORKSPACE_SELFTEST=1
set TIKO_FINDPROJ_SELFTEST=1

echo.
echo   running every startup self-test in one tiko ^(about 20s^)...

del "%~dp0_selftests.log" "%~dp0_selftests.err" 2>nul

rem The poll has a CEILING of 60s rather than a fixed sleep: the keyboard suite
rem dominates and a slower machine must not be cut off mid-run. The stop
rem condition is the LAST suite to report, not any suite -- breaking on the
rem first would truncate the log and trip MINSUITES.
powershell -NoProfile -Command ^
  "$p = Start-Process (Join-Path (Get-Location) 'tiko.exe') -PassThru -NoNewWindow -RedirectStandardOutput _selftests.log -RedirectStandardError _selftests.err;" ^
  "for ($i = 0; $i -lt 240; $i++) {" ^
  "  Start-Sleep -Milliseconds 250;" ^
  "  if ($p.HasExited) { break };" ^
  "  if (Test-Path _selftests.log) {" ^
  "    $n = (Select-String -Path _selftests.log -Pattern '(\d+)\s+passed,\s+(\d+)\s+failed' -AllMatches -ErrorAction SilentlyContinue).Count;" ^
  "    if ($n -ge %MINSUITES%) { Start-Sleep -Milliseconds 1500; break }" ^
  "  }" ^
  "};" ^
  "if (-not $p.HasExited) { taskkill /PID $p.Id /T /F 2>&1 | Out-Null };" ^
  "[void]$p.WaitForExit(5000)"

if not exist "%~dp0_selftests.log" (
    echo.
    echo   FAIL    tiko produced no output at all.
    type "%~dp0_selftests.err" 2>nul
    popd
    exit /b 1
)

echo.
powershell -NoProfile -Command ^
  "$m = Select-String -Path '_selftests.log' -Pattern '(\d+)\s+passed,\s+(\d+)\s+failed';" ^
  "$tp = 0; $tf = 0;" ^
  "foreach ($x in $m) {" ^
  "  $tp += [int]$x.Matches[0].Groups[1].Value; $tf += [int]$x.Matches[0].Groups[2].Value;" ^
  "  $mark = if ([int]$x.Matches[0].Groups[2].Value -gt 0) { 'FAIL  ' } else { 'ok    ' };" ^
  "  Write-Host ('  ' + $mark + '  ' + $x.Line.Trim())" ^
  "};" ^
  "Write-Host '';" ^
  "Write-Host ('  ' + $m.Count + ' suites, ' + $tp + ' passed, ' + $tf + ' failed');" ^
  "if ($tf -gt 0) { exit 1 };" ^
  "if ($m.Count -lt %MINSUITES%) { exit 2 };" ^
  "exit 0"
set RC=%errorlevel%

if %RC% equ 1 (
    echo.
    echo   A startup self-test FAILED. The failing lines are marked above; the
    echo   whole log is in _selftests.log.
    popd
    exit /b 1
)
if %RC% equ 2 (
    echo.
    echo   FAIL    fewer than %MINSUITES% suites reported.
    echo.
    echo   THIS IS THE ASSERTION THAT MATTERS. A tiko that crashes or exits
    echo   before the self-test block prints nothing, sums to zero failures and
    echo   would otherwise PASS. Either a suite stopped running, or tiko did
    echo   not get far enough to run them. Read _selftests.log.
    popd
    exit /b 1
)

del "%~dp0_selftests.log" "%~dp0_selftests.err" 2>nul
echo.
echo   ok      every startup self-test passed
popd
exit /b 0
