@echo off
rem Stage fbcParser artifacts from C:\dev\fbcParser into tiko.
rem Re-run after any fbcParser rebuild.
rem  - fbcParser.dll   -> tiko root (next to the shipped tiko.exe) and src\
rem                       (next to the _compile.bat dev build, src\tiko.exe).
rem                       Implicit linking loads from the exe's own directory;
rem                       the bin\ convention only works for dylibload'ed DLLs.
rem  - fbcParser.bi    -> src\ (the only host header)
rem  - libfbcParser.dll.a -> src\ (import library; _compile.bat links via -p .)

set SRCDIR=C:\dev\fbcParser

copy /y "%SRCDIR%\fbcParser.dll"      "%~dp0fbcParser.dll"
copy /y "%SRCDIR%\fbcParser.dll"      "%~dp0src\fbcParser.dll"
copy /y "%SRCDIR%\fbcParser.bi"       "%~dp0src\fbcParser.bi"
copy /y "%SRCDIR%\libfbcParser.dll.a" "%~dp0src\libfbcParser.dll.a"

echo COPY OK
