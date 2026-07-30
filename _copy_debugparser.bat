@echo off
rem Stage debugParser artifacts from C:\dev\debugParser into tiko.
rem Re-run after any debugParser rebuild.
rem  - debugParser.dll   -> tiko root (next to the shipped tiko.exe) and src\
rem                         (next to the _compile.bat dev build, src\tiko.exe).
rem                         Implicit linking loads from the exe's own directory;
rem                         the bin\ convention only works for dylibload'ed DLLs.
rem  - debugParser.bi    -> src\ (the only host header)
rem  - libdebugParser.dll.a -> src\ (import library; the build links via -p .)

set SRCDIR=C:\dev\debugParser

copy /y "%SRCDIR%\debugParser.dll"      "%~dp0debugParser.dll"
copy /y "%SRCDIR%\debugParser.dll"      "%~dp0src\debugParser.dll"
copy /y "%SRCDIR%\debugParser.bi"       "%~dp0src\debugParser.bi"
copy /y "%SRCDIR%\libdebugParser.dll.a" "%~dp0src\libdebugParser.dll.a"

echo COPY OK
