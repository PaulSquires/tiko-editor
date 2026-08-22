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
rem 33 SINCE 7c STEP 15 added the dialog suites. It was 25, and both numbers
rem were measured rather than counted -- see above.
set /a MINSUITES=33

rem ---- AND A MINIMUM ASSERTION TOTAL, ADDED IN 7c STEP 26 -------------------
rem
rem The note above says a DROP in assertions is "visible" because the totals are
rem printed. Visible to a READER, and not asserted -- so a suite that quietly
rem stops part-way through is caught only if somebody is comparing totals by eye.
rem In step 26 nobody was: the total matched the previous step's number exactly
rem and was read as "unchanged".
rem
rem SEVERAL SUITES COUNT DATA RATHER THAN STATEMENTS. The find-in-project suite
rem asserts per match, so its total moves with what the engine finds -- which is
rem why this is a FLOOR and not an equality. A suite that legitimately grows must
rem not fail the gate; one that quietly shrinks should.
rem
rem ---- WHAT THIS NUMBER IS, AND WHAT IT IS NOT -----------------------------
rem
rem 20,328 is the LOWEST total this tree has been observed to produce, not the
rem highest. Within one hour, on a tree with the same tracked contents, this gate
rem reported both 33 suites / 20,328 and 34 suites / 20,443 -- and the difference
rem was NOT attributed. Step 23 pinned one cause (two untracked .bas files in
rem this directory add a suite); the rest is unexplained, and settings.ini is a
rem candidate because this gate rewrites and restores it and a killed run leaves
rem it mid-flight.
rem
rem SO THE FLOOR IS SET LOW ON PURPOSE. A floor at the high-water mark would fail
rem honest runs, and a gate that cries wolf gets its number lowered until it says
rem nothing -- which is the failure this is trying to prevent, one level up.
rem
rem AND IT IS NOT PROVEN TO CATCH THE DEFECT THAT PROMPTED IT. Re-introducing
rem that defect deliberately left the total UNCHANGED at 20,443. This catches a
rem suite that stops early; it did not catch that one.
set /a MINASSERTS=20328

pushd "%~dp0"

rem Every startup suite, AND -- since 7c step 15 -- the five that need a real
rem dialog. Those are modal and own their own message loop, so each is armed as
rem an AUTOOPEN/AUTOCLOSE PAIR: the first opens it, the second posts WM_CLOSE
rem once its layout suite has run. Arming an AUTOOPEN without its AUTOCLOSE
rem hangs this gate until the poll ceiling, which is a failure rather than a
rem stall, but a slow and confusing one.
rem
rem WM_CLOSE SAVES NOTHING IN ANY OF THE FOUR MODAL DIALOGS -- read before they
rem were armed, not assumed. A dialog whose close path committed would make this
rem gate silently rewrite settings.ini or the .tiko project on every run, which
rem would be far worse than having no gate. frmUnusedSymbols is MODELESS and so
rem needs no AUTOCLOSE at all; it only ever wrote an in-memory geometry record.
rem
rem THE DIALOGS REALLY OPEN. Windows appear and close during this run -- that is
rem the gate working, not a defect. It therefore needs a desktop and cannot run
rem on a headless session. (_check_package.bat has the same property and does
rem not say so.)
rem
rem The AUTORUN/AUTOASSIGN/AUTOEDIT variables stay unset: those drive a debugger
rem against a built exe, or open a second dialog on top of the first.
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
rem TIKO_FINDSEL_SELFTEST runs from the same POSTED message its FINDPROJ neighbours do,
rem so it relies on the same TIKO_FINDPROJ_AUTOEXIT set below.
set TIKO_FINDSEL_SELFTEST=1
set TIKO_PROJECTOPTIONS_SELFTEST=1
set TIKO_USERTOOLS_SELFTEST=1
set TIKO_BUILDCONFIG_SELFTEST=1
set TIKO_ABOUT_SELFTEST=1

rem ---- the dialog pairs. AUTOOPEN + AUTOCLOSE, never one without the other.
set TIKO_KEYBOARD_AUTOOPEN=1
set TIKO_KEYBOARD_AUTOCLOSE=1
set TIKO_USERTOOLS_AUTOOPEN=1
set TIKO_USERTOOLS_AUTOCLOSE=1
set TIKO_BUILDCONFIG_AUTOOPEN=1
set TIKO_BUILDCONFIG_AUTOCLOSE=1
set TIKO_PROJECTOPTIONS_AUTOOPEN=1
set TIKO_PROJECTOPTIONS_AUTOCLOSE=1
set TIKO_ABOUT_AUTOOPEN=1
set TIKO_ABOUT_AUTOCLOSE=1
set TIKO_FORMATOPTIONS_AUTOOPEN=1
set TIKO_FORMATOPTIONS_AUTOCLOSE=1
rem MODELESS -- it needs no AUTOCLOSE, and adding one for symmetry would post
rem WM_CLOSE to a window the user might legitimately have open.
set TIKO_UNUSED_AUTOOPEN=1

rem ---- and QUIT when the last suite has run, rather than being killed.
rem Posted from the end of MSG_USER_PROCESS_COMMANDLINE, which is where the
rem final four suites live. Killing did work -- measured, twice, under two
rem redirect styles -- but a gate that exits its subject cleanly cannot be
rem argued with.
set TIKO_SELFTEST_EXIT=1

rem ---------------------------------------------------------------------------
rem THE USER'S STATE IS BACKED UP AND PUT BACK, and this is not belt-and-braces.
rem
rem MEASURED: a gate run rewrote settings.ini. Not because any dialog committed
rem -- all four close paths were read first and none of them save -- but because
rem TIKO_SELFTEST_EXIT makes tiko exit CLEANLY, and a clean exit saves the
rem config and the workspace, exactly as closing the editor by hand does. The
rem tree-kill this replaced had been hiding that.
rem
rem So the run would persist ITS OWN session -- whatever windows the gate left
rem open, whatever the dialogs left selected -- over the author's. Restoring is
rem the honest fix; suppressing tiko's save-on-exit would be changing the
rem PRODUCT to suit a test.
rem ---------------------------------------------------------------------------
if exist "%~dp0settings\settings.ini" copy /y "%~dp0settings\settings.ini" "%~dp0_selftests.ini.bak" >nul
if exist "%~dp0tiko.tiko" copy /y "%~dp0tiko.tiko" "%~dp0_selftests.tiko.bak" >nul

echo.
echo   running every self-test in one tiko ^(about 20s; dialogs will open^)...

del "%~dp0_selftests.log" "%~dp0_selftests.err" 2>nul

rem THE STOP CONDITION IS tiko EXITING, not a line count. It was a count until
rem 7c step 15, and that was wrong for the same reason the About hook was: with
rem the dialog suites armed, the count reached MINSUITES while four suites were
rem still to come, the poll broke, and the kill truncated them. The log had
rem fewer lines and nothing said why.
rem
rem TIKO_SELFTEST_EXIT makes tiko quit once the last suite has run, so waiting
rem for the process is exact where a line count is a guess. The 60s ceiling and
rem the kill remain as a BACKSTOP for a tiko that hangs -- and a hang then trips
rem MINSUITES rather than passing quietly.
powershell -NoProfile -Command ^
  "$p = Start-Process (Join-Path (Get-Location) 'tiko.exe') -PassThru -NoNewWindow -RedirectStandardOutput _selftests.log -RedirectStandardError _selftests.err;" ^
  "for ($i = 0; $i -lt 240; $i++) {" ^
  "  Start-Sleep -Milliseconds 250;" ^
  "  if ($p.HasExited) { break }" ^
  "};" ^
  "if (-not $p.HasExited) { taskkill /PID $p.Id /T /F 2>&1 | Out-Null };" ^
  "[void]$p.WaitForExit(5000)"

rem PUT IT BACK, before anything can exit down a path that skips it.
if exist "%~dp0_selftests.ini.bak" (
    copy /y "%~dp0_selftests.ini.bak" "%~dp0settings\settings.ini" >nul
    del "%~dp0_selftests.ini.bak" >nul
)
if exist "%~dp0_selftests.tiko.bak" (
    copy /y "%~dp0_selftests.tiko.bak" "%~dp0tiko.tiko" >nul
    del "%~dp0_selftests.tiko.bak" >nul
)

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
  "if ($tp -lt %MINASSERTS%) {" ^
  "  Write-Host ('  FAIL    ' + $tp + ' assertions, fewer than the ' + %MINASSERTS% + ' this tree runs.');" ^
  "  Write-Host '          A suite that quietly SHRINKS is the same defect as one that fails.';" ^
  "  exit 3" ^
  "};" ^
  "exit 0"
set RC=%errorlevel%

if %RC% equ 1 (
    echo.
    echo   A startup self-test FAILED. The failing lines are marked above; the
    echo   whole log is in _selftests.log.
    popd
    exit /b 1
)
rem RC 3 IS THE ASSERTION FLOOR, and it needs its own arm here or the exit code
rem is thrown away. Written without one first: the FAIL line printed, the gate
rem returned 0, and the "ok" line printed underneath it -- a gate that reports a
rem failure and passes is worse than one that never checked.
if %RC% equ 3 (
    echo.
    echo   Either a suite stopped part-way through, or one that counts DATA found
    echo   less of it. Compare the per-suite totals above against a known-good run.
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
