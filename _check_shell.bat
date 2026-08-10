@echo off
rem Thin wrapper. The checker itself is _check_shell.ps1 -- see its header for
rem what it enforces, and for why it is PowerShell rather than this file.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0_check_shell.ps1"
exit /b %ERRORLEVEL%
