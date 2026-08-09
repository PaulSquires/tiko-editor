@echo off
rem ---------------------------------------------------------------------------
rem Run the phase 7c shell binary with its DLLs reachable.
rem
rem THE DLLs MUST BE ON PATH, NOT COPIED, and _check_scihost.bat records why:
rem copying the deps set next to the exe still failed with 0xC0000139
rem (ENTRYPOINT_NOT_FOUND) naming nothing useful. Pointing PATH at the real
rem directories is the arrangement that works.
rem
rem The shell needs one directory tiko.exe does not: SDL3.dll, out of
rem deps\out\win64\bin. tiko has never loaded it -- the Win32 host bridge exists
rem so tiko keeps its own window and message loop -- and a missing SDL3.dll is
rem exit 127 with no dialog, which reads as a broken build rather than a missing
rem library. If this script is skipped, that is the error you get.
rem
rem Arguments are forwarded, so --selftest and friends work through it.
rem ---------------------------------------------------------------------------
setlocal
set PSP=%~dp0..\PsPlatform
set PATH=%PSP%\build\out\win64;%PSP%\deps\out\win64\bin;%PATH%
rem .\ is required: PATH was just prepended with two directories, and cmd
rem searches PATH before the current directory.
"%~dp0_shell\tikoshell.exe" %*
exit /b %ERRORLEVEL%
