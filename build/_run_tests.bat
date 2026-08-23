@echo off
rem ---------------------------------------------------------------------------
rem THE TEST RUNNER. One command for every self-test this tree has.
rem
rem   _run_tests.bat            run everything, print a summary
rem   _run_tests.bat -verbose   also echo every suite's own output
rem   _run_tests.bat -keep      leave the raw logs in build\testlogs\ (implied
rem                             by -verbose; they are kept on failure anyway)
rem
rem ERRORLEVEL is the number of suites that FAILED. 0 means every suite that ran
rem reported zero failed assertions. Suites that could not be reached are
rem reported as SKIPPED and are NOT counted as failures -- but they are always
rem named, because a silently omitted suite is the exact failure this runner
rem exists to prevent.
rem
rem WHY THE PARSING LIVES IN POWERSHELL: the editor's console output is MIXED
rem ENCODING. Narrow output arrives as ANSI and wide output as UTF-16LE, in the
rem same stream, so the log is littered with NUL bytes. findstr treats such a
rem file as binary and reports "matches" instead of lines. PowerShell can strip
rem the NULs and parse what is left; batch cannot.
rem
rem NEVER KILL THE EDITOR. tiko-editor.exe is a console-subsystem exe whose
rem stdout is BLOCK-BUFFERED when redirected, so a run that is killed rather
rem than exited loses every line the suites printed -- including the pass/fail
rem reports that ARE the result. The runner waits for the process to exit on its
rem own through one of the TIKO_*_AUTOEXIT hooks, and if it does not, it says so
rem and says the log is incomplete rather than reporting failures it cannot see.
rem ---------------------------------------------------------------------------
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0_run_tests.ps1" %*
exit /b %ERRORLEVEL%
